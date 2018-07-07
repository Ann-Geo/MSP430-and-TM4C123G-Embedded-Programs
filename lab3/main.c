/******************************************************************************************************************
 * File name: main.c
 * Program Objective: To understand how to utilize the ADC and UART modules in the TM4c123GH6PM microcontroller.
 * Potentiometer specification : 2Kohm
 * Objectives : 1. A single generic program is developed for both receiver and transmitter Boards[2].
                2. Receiver part of the program is able to handle multiple received bytes[2].
                3. UART peripheral should be handled with the aid of interrupt services[2].
                4. Propose a transmission encoding system that represents the 12-bit POT position. Also, the
                system must send back an acknowledgement to transmitter board. By receiving an acknowledgment,
                the blue LED of the launchpad should be illuminated[2].
                5. Propose a method to prevent receiver getting overloaded by not sending of small changes of
                POT position[2].
                6. The ADC Value is read and scaled properly and displayed on the LED Bar (with no oscillations)[2].
 * Description : Two TIVA board are used for this experiment. They are the Sender and Receiver boards. A single
 * program is written to run on both sender and receiver. The program identifies the board using a configuration
 * pin, PE3. If PE3 is connected to VCC, that board acts as receiver else it acts as sender. The sender reads the
 * input voltage value from the potentiometer through port pin PE5 and the 12-bit ADC(ADC0) converts the input
 * analog voltage to a 12 bit digital value. The digital value is stored in a variable and is compared against
 * 11 characters which represent different voltage levels read from the potentiometer. The character corresponding
 * to the converted digital value is send to the receiver via UART(UART4). The receiver compares the received
 * character with the predefined set of characters and lights the LED bar to represent the analog voltage read from 
 * sender. Also the receiver sends back an acknowledgement to sender and sender turns on the blue LED for one second.
 *  References: [1]Embedded System Design using TM4C LaunchPadTM Development Kit,SSQU015(Canvas file)
 *              [2]Tiva C Series TM4C123G LaunchPad Evaluation Board User's Guide
 *              [3]Tiva TM4C123GH6PM Microcontroller datasheet
 *              [4]TivaWare Peripheral Driver Library User guide
 *              [5]http://processors.wiki.ti.com/index.php/Tiva_TM4C123G_LaunchPad_Blink_the_RGB
 *              [6]Embedded Systems An Introduction using the Renesas RX63N Microcontroller BY JAMES M. CONRAD
 *********************************************************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/adc.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"

void GPIOInit(void);
void ADCInit(void);
void UARTInit(void);
void ConversionFunction(void);
void UARTIntHandler(void);
void SenderBoard(void);
void ReceiverBoard(void);
void SendToReceiver(uint8_t);
void LightLEDBar(uint8_t);

/*variable that stores the configuration of pin PE3 to check whether the board is sender or receiver*/
volatile uint32_t ui32ConfigPinStatus;

/*variable that stores the character corresponding to the result of previous conversion. ui8ValueToReceiverPrevious is
 * set to an initial value 'z' (This value can be anything other than the values used for encoding at the sender.)*/
volatile uint8_t ui8ValueToReceiverPrevious = 'z';


/***************************************************************************************************************************
 * Function name : main()
 * Inputs : none
 * Outputs : none
 * Configuration pin : PE3
 * Analog input pin : PE5
 * LED bar output pins : PA7, PA6, PB7, PB6, PB5, PB4, PB3, PB2, PB1, PB0(From MSB to LSB in order)
 * UART receiver and transmitter pins : PC4 and PC5
 * Blue LED output pin : PF2
 * Description : This function initializes the GPIO ports, ADC0 and UART4 modules. Also it determines the board is a sender
 * or receiver by checking the configuration pin. If configuration pin is connected to VCC it is receiver else it is sender.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void main(void)
{
    GPIOInit();
    ADCInit();
    UARTInit();

    while(1)
    {
        ui32ConfigPinStatus = GPIOPinRead(GPIO_PORTE_BASE, GPIO_PIN_3);

        if(ui32ConfigPinStatus)
        {
            ReceiverBoard();
        }

        else
        {
            SenderBoard();
        }
    }
}

/***************************************************************************************************************************
 * Function name : GPIOInit()
 * Inputs : none
 * Outputs : none
 * Configuration pin : PE3
 * Analog input pin : PE5
 * LED bar output pins : PA7, PA6, PB7, PB6, PB5, PB4, PB3, PB2, PB1, PB0(From MSB to LSB in order)
 * Blue LED output pin : PF2
 * Description : This function initializes the GPIO ports.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void GPIOInit(void)
{
        /*Enables the peripheral GPIO port E and configures the PE3 pin as input*/
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
        GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_3);

        /*Enable the peripherals GPIO port A and B and configures the LED bar output pins(mentioned in block comment)
         * as output*/
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
        GPIOPinTypeGPIOOutput(GPIO_PORTA_BASE, GPIO_PIN_7 | GPIO_PIN_6);
        GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 |
                GPIO_PIN_1 | GPIO_PIN_0));

        /*Enables the peripheral GPIO port F and configures the PF2 pin as output*/
        SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
        GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2);
}

/***************************************************************************************************************************
 * Function name : ADCInit()
 * Inputs : none
 * Outputs : none
 * ADC module used : ADC0
 * Description : This function initializes the ADC0 module. For specifications check comments below.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void ADCInit(void)
{
        /*Enables ADC0 module, configures PE5 as ADC input, selects the sample sequencer 3, trigger source for conversion
         * as processor trigger, sample sequencer priority as 0, selects sample sequencer step to be configured as 0, sample
         * channel 8(ADC_CTL_CH8) and tells the ADC that this is the last conversion(ADC_CTL_END). Enables sample sequence 3*/
        SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
        GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_5);
        ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
        ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH8 | ADC_CTL_END);
        ADCSequenceEnable(ADC0_BASE, 3);

}

/***************************************************************************************************************************
 * Function name : UARTInit()
 * Inputs : none
 * Outputs : none
 * UART module used : UART4
 * Description : This function initializes the UART4 module. For specifications check comments below.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void UARTInit(void)
{
      /*Enables UART4 module and GPIO port C, configures PC4 and PC5 as uart rx and tx pins(since pin muxing functionality is
       * present in the processor for theses two pins), selects the clock source to UART as Precision Internal Oscillator
       * (PIOSC) which is 16MHz, configures PC4 and PC5 as UART rx and tx, selects uart clock as 16MHz and buadrate as 115200,
       * defines the number of data bits - 8, stop bit -1 and no parity. */
       SysCtlPeripheralEnable(SYSCTL_PERIPH_UART4);
       SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
       GPIOPinConfigure(GPIO_PC4_U4RX);
       GPIOPinConfigure(GPIO_PC5_U4TX);
       UARTClockSourceSet(UART4_BASE, UART_CLOCK_PIOSC);
       GPIOPinTypeUART(GPIO_PORTC_BASE, GPIO_PIN_4 | GPIO_PIN_5);
       UARTConfigSetExpClk(UART4_BASE, 16000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

       /*Enables processor interrupts, enables UART receive interrupt, Registers UART interrupt handler, Enables UART4 module*/
       IntMasterEnable();
       UARTIntEnable(UART4_BASE, UART_INT_RX);
       UARTIntRegister(UART4_BASE, UARTIntHandler);
       UARTEnable(UART4_BASE);

}


/******************************************************************************************************************************
 * Function name : ConversionFunction()
 * Inputs : none
 * Outputs : none
 * Description : This function triggers the ADC conversion and the converted value is stored in pui32ADCValue[0].
 * The total range of input voltages (0 to 3.3) has been divided into 10 equal intervals and for each interval the corresponding
 * digital value is calculated using below formula. The analog and digital values obtained and are listed below.
 *
 * Analog value(V)     Digital value  |       Range                 character to be send to receiver
 * 0                0                 |       ==0                   'a'
 * 0.33             410               |       >0 and <=410          'b'
 * 0.66             819               |       >410 and <=819        'c'
 * 0.99             1229              |       >819 and <=1229       'd'
 * 1.32             1638              |       >1229 and <=1638      'e'
 * 1.65             2048              |       >1638 and <=2048      'f'
 * 1.98             2457              |       >2048 and <=2457      'g'
 * 2.31             2867              |       >2457 and <=2867      'h'
 * 2.64             3276              |       >2867 and <=3276      'i'
 * 2.97             3686              |       >3276 and <=3686      'j'
 * 3.3              4095              |       >3686 and <=4095      'k'
 * The converted digital value is compared against the above given values to find out the character to be send to the receiver. The
 * range of digital values and character to be send to receiver are listed above. This character is stored in ui8ValueToReceiverCurrent.
 * Also the previous value of the character is stored in ui8ValueToReceiverPrevious.The previous and current values are compared and
 * the current value is send to receiver only if it is different from previous value.This is to prevent the receiver getting overloaded
 * by sending small changes in POT position.
 *
 * Equation to convert the analog value to digital value[8] :
 * digital value =          [Vin - Vref(-)]*[2^N - 1]
 *                      { ---------------------------- + 1/2 }int
 *                            Vref(+) - Vref(-)
 * Vref(-) = 0V
 * Vref(+) = 3.3V
 * N = 12 bits
 * Vin = input analog voltage in Volts
 * Reference for APIs: TivaWare Peripheral Driver Library User guide
 *                     Embedded Systems An Introduction using the Renesas RX63N Microcontroller BY JAMES M. CONRAD
 *******************************************************************************************************************************/

void ConversionFunction(void)
{
    /*variable to store the converted ADC value*/
    uint32_t pui32ADCValue[1];

    /*variable to store the character to be send to the receiver*/
    uint8_t ui8ValueToReceiverCurrent;


    /*Starts ADC conversion by processor triggering, if ADC is not busy get the converted value to pui32ADCValue. compares
     * pui32ADCValue with range of digital values to find the character to be send to the receiver */
        ADCProcessorTrigger(ADC0_BASE, 3);
        while(ADCBusy(ADC0_BASE))
        {
        }
        ADCSequenceDataGet(ADC0_BASE, 3, pui32ADCValue);

       if(pui32ADCValue[0] == 0)
           ui8ValueToReceiverCurrent = 'a';
       if(pui32ADCValue[0] > 0 && pui32ADCValue[0] <= 410)
           ui8ValueToReceiverCurrent = 'b';
       if(pui32ADCValue[0] > 410 && pui32ADCValue[0] <= 819)
           ui8ValueToReceiverCurrent = 'c';
       if(pui32ADCValue[0] > 819 && pui32ADCValue[0] <= 1229)
           ui8ValueToReceiverCurrent = 'd';
       if(pui32ADCValue[0] > 1229 && pui32ADCValue[0] <= 1638)
           ui8ValueToReceiverCurrent = 'e';
       if(pui32ADCValue[0] > 1638 && pui32ADCValue[0] <= 2048)
           ui8ValueToReceiverCurrent = 'f';
       if(pui32ADCValue[0] > 2048 && pui32ADCValue[0] <= 2457)
           ui8ValueToReceiverCurrent = 'g';
       if(pui32ADCValue[0] > 2457 && pui32ADCValue[0] <= 2867)
           ui8ValueToReceiverCurrent = 'h';
       if(pui32ADCValue[0] > 2867 && pui32ADCValue[0] <= 3276)
           ui8ValueToReceiverCurrent = 'i';
       if(pui32ADCValue[0] > 3276 && pui32ADCValue[0] <= 3686)
           ui8ValueToReceiverCurrent = 'j';
       if(pui32ADCValue[0] > 3686 && pui32ADCValue[0] <= 4095)
           ui8ValueToReceiverCurrent = 'k';

       /*if the current character to be send is not equal to the previous character, send the current character to
        * receiver */
       if(ui8ValueToReceiverPrevious != ui8ValueToReceiverCurrent)
       {
               ui8ValueToReceiverPrevious = ui8ValueToReceiverCurrent;
               SendToReceiver(ui8ValueToReceiverCurrent);
       }
}

/***************************************************************************************************************************
 * Function name : UARTIntHandler()
 * Inputs : none
 * Outputs : none
 * UART module used : UART4
 * Acknowledgement character used : 'A'
 * Description : This is the ISR which is serviced when the receive interrupt is generated. This handler function is
 * declared and defined in tm4c123gh6pm_startup_ccs.c file. The handler first clears the interrupt generated. Afterwards it
 * identifies the sender and receiver boards, if it is a receiver board it calls the function to light the LED bar and also
 * sends an acknowledgement character 'A' to sender. If it is a sender board it checks the acknowledgemnet character and
 * lights the blue LED.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void UARTIntHandler(void)
{
    /*variable stores the status of receive interrupt*/
    uint32_t ui32Status = UARTIntStatus(UART4_BASE, true);

    /*clears receive interrupt*/
    UARTIntClear(UART4_BASE, ui32Status);

    /*if configuration pin status is 1, its a receiver board else it is a sender board. In the receiver board the LightLEDBar
     * function is called and also sends an acknowledgement  back to the sender. In the sender board if it receives an 'A' then
     * illuminate the blue LED*/
    if(ui32ConfigPinStatus)
     {
        LightLEDBar(UARTCharGet(UART4_BASE));
        UARTCharPut(UART4_BASE, 'A');
     }
    else
    {
        if(UARTCharGet(UART4_BASE) == 'A')
        {
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
        }
    }
}

/***************************************************************************************************************************
 * Function name : SenderBoard()
 * Inputs : none
 * Outputs : none
 * Description : This function executes one second delay after the ISR turns on the blue LED. Then it turns off the blue LED.
 * After that it calls the ADC conversion function.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void SenderBoard(void)
{
    /*One second delay: clock is configured as 16MHz. The SysCtlDelay(delay) function will create a delay for one second.
     * the formula to calculate the delay is, delay = DelayTimeInSeconds/((1/16000000)*3). Here DelayTimeInSeconds = 1 second */
    SysCtlDelay(16000000/3);

    /*Turns off the blue LED and call conversion function*/
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);
    ConversionFunction();

}

/***************************************************************************************************************************
 * Function name : ReceiverBoard()
 * Inputs : none
 * Outputs : none
 * Description : This function continues in a while loop until the receive interrupt is generated. After servicing the ISR
 * it continues in the while loop
 ***************************************************************************************************************************/

void ReceiverBoard(void)
{
    while(1)
    {
    }
}

/***************************************************************************************************************************
 * Function name : SendToReceiver()
 * Inputs : ui8ValueToReceiverCurrent
 * Outputs : none
 * Description : This function sends ui8ValueToReceiverCurrent character to receiver board.
 * Reference for APIs : TivaWare Peripheral Driver Library User guide
 ***************************************************************************************************************************/

void SendToReceiver(uint8_t ui8ValueToReceiverCurrent)
{
    /*sends the encoded character to receiver*/
    UARTCharPut(UART4_BASE, ui8ValueToReceiverCurrent);
}

/**************************************************************************************************************************
 * Function name : LightLEDBar()
 * Inputs : ui8CharacterReceived
 * Outputs : none
 * Description : This function is called from the interrupt handler of receiver board to light the LED bar by analyzing the
 * character received. The character received and the no of LEDs to be lit are listed below.
 * Character received       No of LEDs to be lit in LED bar     Corresponding port values for port A and port B respectively
 * 'a'                      0                                   0x0, 0x0
 * 'b'                      1                                   0x0, 0x1
 * 'c'                      2                                   0x0, 0x3
 * 'd'                      3                                   0x0, 0x7
 * 'e'                      4                                   0x0, 0xF
 * 'f'                      5                                   0x0, 0x1F
 * 'g'                      6                                   0x0, 0x3F
 * 'h'                      7                                   0x0, 0x7F
 * 'i'                      8                                   0x0, 0xFF
 * 'j'                      9                                   0x40, 0xFF
 * 'k'                      10                                  0xC0, 0xFF
 * Reference for APIs :TivaWare Peripheral Driver Library User guide
 **************************************************************************************************************************/

void LightLEDBar(uint8_t ui8CharacterReceived)
{
    /*compares ui8CharacterReceived with different cases to light the LED bar to represent the digital value*/
    switch(ui8CharacterReceived) {

           case 'a' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x0);
                   break;
           case 'b' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x1);

                   break;
           case 'c' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x3);

                   break;
           case 'd' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x7);

                   break;
           case 'e' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0xF);

                   break;
           case 'f' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x1F);

                   break;
           case 'g' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x3F);

                   break;
           case 'h' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0x7F);

                   break;
           case 'i' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0xFF);

                   break;
           case 'j' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0x40);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0xFF);

                   break;
           case 'k' :
               GPIOPinWrite(GPIO_PORTA_BASE, (GPIO_PIN_7 | GPIO_PIN_6), 0xC0);
               GPIOPinWrite(GPIO_PORTB_BASE, (GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 |
                       GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0), 0xFF);

                   break;

           }

}
