Specifications
--------------

1. Compose one application that runs on both the sender and receiver. Use a configuration pin to determine the whether the code is running on a sender or receiver. Use one General-Purpose I/O (or PIO) that can use a jumper at run-time to select the appropriate behavior. For example, you can tie a pin to ground to indicate that the program is running on the Tiva board connected to the potentiometer or tie that pin to Vdd to indicate that is the Tiva board is the one connected to the LED bar.[1]

Configuration pin specifications used to identify the sender and receiver
-------------------------------------------------------------------------
Configuration pin used to identify the sender and receiver : PE3
Connected to Vcc for receiver and to ground for sender


2. Mimic the POT functionality of the MSP430 lab to work on the Launchpad Tiva TM4C1234G so that it can communicated over the wires to the other Tiva board. Every potentiometer in the lab has a different resistance so the circuit must be documented and each group must come up with a circuit-specific encoding. And in the documentation, students will fill in this information! :-) [1]

Circuit Specifications
----------------------------
Potentiometer value: 2Kohm
Vref(-) = 0V
Vref(+) = 3.3V
The input terminal of the potentiometer is connected to Vcc of sender board and the ground terminal is connected to ground via breadboard. And the output terminal is connected to analog input pin PE5
ADC module : ADC0
Analog input pin : PE5
LED bar output pins : PA7, PA6, PB7, PB6, PB5, PB4, PB3, PB2, PB1, PB0(From MSB to LSB in order)


3. Create an encoding that will fit in an 8-bit UART message and transport from one board to another. Think about this ... the student could quantify the analog signal at the sender and send a value with a limited range over the UART or it could send the raw data and leave it up to the receiver to translate the value to the discrete LED array bar. You choose and document.[1]

Encoding used to send the converted values from sender to receiver
-----------------------------------------------------------------------------------------------------
The sender board converts analog voltage from potentiometer to a digital value. If the digital value belongs to any of the ranges specified in the below table the corresponding character is send to the receiver.


Analog value(V)     Digital value     |       Range                 character to be send to receiver
-----------------------------------------------------------------------------------------------------
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
-----------------------------------------------------------------------------------------------------

Equation to convert the analog value to digital value[8] 
------------------------------------------------------
 digital value =          [Vin - Vref(-)]*[2^N - 1]
                      { ---------------------------- + 1/2 }int
                            Vref(+) - Vref(-)
 Vref(-) = 0V
 Vref(+) = 3.3V
 N = 12 bits
 Vin = input analog voltage in Volts


4. On the receiver side, the application must use the UART receive interrupt to receive messages from sender that indicate a change in the POT position.

UART module : UART4
UART receive interrupt mask : UART_INT_RX
UART receiver and transmitter pins : PC4 and PC5
UART message configuration : buadrate - 115200, number of data bits - 8, stop bit -1 and no parity.
The sender UART Tx(PC5) is connected to receiver Rx(PC4) and the sender UART Rx(PC4) is connected to receiver Tx(PC5). Both the boards are grounded.

5. The receiver must send back an acknowledgement after receiving the encoded character from the sender which represents the analog voltage and the sender will illuminate the blue LED on the launchpad board for one second to acknowledge the change to the POT.

Clock source used for UART : Precision Internal Oscillator (PIOSC) which is 16MHz
Blue LED output pin : PF2
Acknowledgement character : 'A'
One second delay: clock is configured as 16MHz. The SysCtlDelay(delay) function will create a delay for one second. The formula to calculate the delay is, 
delay = DelayTimeInSeconds/((1/16000000)*3). Here DelayTimeInSeconds = 1 second
    

6. It is not required that the serial communication use RS-232 voltage levels. (Thus, a MAX3232 chip doesn't have to be part of your external circuit.) You can assume that the boards will be close enough that CMOS signals are sufficient.

The length of jumper wires used to connect transmitter and receiver pins of boards : 30cm

7. Small changes to the POT that will not change the LED bar should be filtered at the sender and not transmitted over the serial communication line.

The current character to be send is stored in ui8ValueToReceiverCurrent. Also the previous value of the character is stored in ui8ValueToReceiverPrevious.The previous and current values are compared and the current value is send to receiver only if it is different from previous value. This is to prevent the receiver getting overloaded by sending small changes in POT position.



Other circuit components used to implement the system
------------------------------------------------------
Two TIVA TM4C123G boards
LED bar with 10 LEDs
Resistor array with nine 1Kohm resistors
Extra 1 Kohm resistor
Jumper wires and bread board
Mini USB connectors

References :
------------
[1]Embedded System Design using TM4C LaunchPadTM Development Kit,SSQU015(Canvas file)
[2]Tiva C Series TM4C123G LaunchPad Evaluation Board User's Guide
[3]Tiva TM4C123GH6PM Microcontroller datasheet
[4]TivaWare Peripheral Driver Library User guide
[5]http://processors.wiki.ti.com/index.php/Tiva_TM4C123G_LaunchPad_Blink_the_RGB
[6]Embedded Systems An Introduction using the Renesas RX63N Microcontroller BY JAMES M. CONRAD
