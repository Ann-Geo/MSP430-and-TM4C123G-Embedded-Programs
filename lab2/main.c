/***********************************************************************************************************
 * File name: main.c
 * Program Objective: To understand how to utilize the ADC module in the MSP430G2553 microcontroller.
 * Potentiometer specifications : 10Kohm
 * Objectives : 1. ADC Voltages are properly read from the potentionmeter.
 *              2. The LED Bar lights up and can be controlled properly
 *              3. The ADC Value is scaled properly and displayed on the LED Bar (with no oscillations)
 * Description : The microcontroller reads the input voltage value from the potentiometer through
 * port pin 1.1 and the 10-bit ADC converts the input analog voltage to a 10 bit digital value. The digital
 * value is stored in a variable and is used to control the LED bar at the output. The LED bar lights up
 * according to the variations in the input voltage.
 * References : MSP430G2x53/MSP430G2x13 data sheet
 *              MSP430x2xx Family User Guide
 *              MSP430 Optimizing C/C++ Compiler v17.6.0.STS User's Guide
 *              MSP-EXP430G2 LaunchPad Development Kit User's Guide
 *              http://processors.wiki.ti.com/index.php/MSP430_LaunchPad_Tutorials
 *              Resource Explorer, peripheral examples - CCS
 *              http://coder-tronics.com/msp430-adc-tutorial/
 *************************************************************************************************************/

#include <msp430.h> 
#include <intrinsics.h>


void setupADC();
int conversionFunction(int);
void lightLEDs(int);

/***************************************************************************************************************
 * Function name : main()
 * Inputs : none
 * Outputs : none
 * Input port pin : P1.1
 * Output port pins : From MSB to LSB in the order - P1.7, P1.6, P1.5, P1.4, P2.5, P2.4, P2.3, P2.2, P2.1, P2.0
 * Description : This function configures the input and output ports of the microcontroller.
 ****************************************************************************************************************/

int main(void){

    int adcValue;                                                                     //variable stores the converted digital value
    int noOfLEDsToBeLit;                                                              //stores the number of LEDs to be lit after conversion

    WDTCTL = WDTPW | WDTHOLD;                                                         //disables watchdog timer
    P1OUT &= 0x0F;                                                                    //clears output pins P1.7, P1.6, P1.5, P1.4
    P2OUT &= 0xC0;                                                                    //clears output pins P2.5, P2.4, P2.3, P2.2, P2.1, P2.0
    P1DIR |= 0xF0;                                                                    //sets direction for output pins P1.7, P1.6, P1.5, P1.4
    P2DIR |= 0x3F;                                                                    //sets direction for output pins P2.5, P2.4, P2.3, P2.2, P2.1, P2.0
    setupADC();



    while(1){

         ADC10CTL0 &= ~ENC;                                                               //disables conversion
         while (ADC10CTL1 & ADC10BUSY);                                                   //checks ADC10 busy status

         ADC10CTL0 |= ENC + ADC10SC;                                                      //enables conversion

         __delay_cycles(13);                                                              //ADC10 needs 13 clock cycles to complete the conversion. This line provides a delay to finish the conversion for the ADC
         adcValue = ADC10MEM;                                                             //stores the converted digital value from ADC10MEM to adcValue
         noOfLEDsToBeLit = conversionFunction(adcValue);                                  //calls ADC conversion function
         lightLEDs(noOfLEDsToBeLit);                                                      //calls the lightLEds function
    }

}

/***************************************************************************************************************
 * Function name : setupADC()
 * Inputs : none
 * Outputs : none
 * Description : This function configures the ADC control registers.
 ****************************************************************************************************************/

void setupADC(){

    ADC10CTL1 = INCH_1 + CONSEQ_2;                                                    //Selects the input channel A1 and the conversion sequence mode as repeat single channel
    ADC10CTL0 = SREF_1 + ADC10SHT_2 + MSC + REFON +REF2_5V + ADC10ON;                 //selects Vref, sample and hold time(16 ADC10CLKs), multiple sample and conversion and enables ADC

    ADC10AE0 |= 0x02;                                                                 //input channel A1(P1.1)

}

/***************************************************************************************************************
 * Function name : conversionFunction()
 * Inputs : adcValue
 * Outputs : noOfLEDsToBeLit
 * Description : This function calculates the number of LEds to be lit on the LED bar by comparing the converted
 * digital value with all possible digital outputs.
 * Equation to convert the analog value to digital value :
 * digital value =          [Vin - Vref(-)]*[2^N - 1]
 *                      { ---------------------------- + 1/2 }int
 *                            Vref(+) - Vref(-)
 * Vref(-) = 0V
 * Vref(+) = 2.5V
 * N = 10 bits
 * Vin = input analog voltage in Volts
 ****************************************************************************************************************/

int conversionFunction(int adcValue)
{

    int noOfLEDsToBeLit;                                                               //stores the number of LEDs to be lit after conversion

    if(adcValue == 0)                                                                  //condition to evaluate the number of LEDs to be lit on the LED bar
            noOfLEDsToBeLit = 0;
    if(adcValue > 0 && adcValue <= 102)
            noOfLEDsToBeLit = 1;
    if(adcValue > 102 && adcValue <= 205)
            noOfLEDsToBeLit = 2;
    if(adcValue > 205 && adcValue <= 307)
            noOfLEDsToBeLit = 3;
    if(adcValue > 307 && adcValue <= 409)
            noOfLEDsToBeLit = 4;
    if(adcValue > 409 && adcValue <= 512)
            noOfLEDsToBeLit = 5;
    if(adcValue > 512 && adcValue <= 614)
            noOfLEDsToBeLit = 6;
    if(adcValue > 614 && adcValue <= 716)
            noOfLEDsToBeLit = 7;
    if(adcValue > 716 && adcValue <= 818)
            noOfLEDsToBeLit = 8;
    if(adcValue > 818 && adcValue <= 921)
            noOfLEDsToBeLit = 9;
    if(adcValue > 912 && adcValue <= 1023)
            noOfLEDsToBeLit = 10;

    return noOfLEDsToBeLit;

}

/***************************************************************************************************************
 * Function name : lightLEDs()
 * Inputs : noOfLEDsToBeLit
 * Outputs : none
 * Description : This function lights up the LEDs on the LED bar by giving output values to the port 1 and 2
 ****************************************************************************************************************/

void lightLEDs(int noOfLEDsToBeLit){

    switch(noOfLEDsToBeLit) {                                             //switch expression evaluates the values to be send to the ports by checking the variable noOfLEDsToBeLit

       case 0 :
               P1OUT = 0x00;
               P2OUT = 0x00;
               break;
       case 1 :
               P1OUT = 0x00;
               P2OUT = 0x01;
               break;
       case 2 :
               P1OUT = 0x00;
               P2OUT = 0x03;
               break;
       case 3 :
               P1OUT = 0x00;
               P2OUT = 0x07;
               break;
       case 4 :
               P1OUT = 0x00;
               P2OUT = 0x0F;
               break;
       case 5 :
               P1OUT = 0x00;
               P2OUT = 0x1F;
               break;
       case 6 :
               P1OUT = 0x00;
               P2OUT = 0x03F;
               break;
       case 7 :
               P1OUT = 0x10;
               P2OUT = 0x3F;
               break;
       case 8 :
               P1OUT = 0x30;
               P2OUT = 0x3F;
               break;
       case 9 :
               P1OUT = 0x70;
               P2OUT = 0x3F;
               break;
       case 10 :
               P1OUT = 0xF0;
               P2OUT = 0x3F;
               break;

       }
}
