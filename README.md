### MSP430-and-TM4C123G-Embedded-Programs
MSP430 and TM4C123G microcontroller based programs - Embedded Systems labs

• Lab 1:-\
Aim: Program the following requirements on TI's MSP430 Launchpad with the help of its on-board peripherals.\
Objectives:-
1. LED2 Blinks at a frequency of 0.5 Hz, 50% duty cycle
2. The LED only blinks when button S2 is not held down
3. When button S2 is held down for at least 2 seconds, when S2 is finally released LED2 stops blinking and LED1 begins blinking.
4. The configuration is reversed when S2 is again pressed for at least 2 seconds again.

• Lab 2:-\
Aim: Understand how to utilize the analog to digital converter (ADC) peripheral on TI's MSP430 Launchpad. In this lab, a potentiometer is connected to an ADC input pin, the analog value is read, and that voltage is expressed on an LED array bar.\
Objectives:-
1. ADC Voltages are properly read from the potentionmeter.
2. The LED Bar lights up and can be controlled properly
3. The ADC Value is scaled properly and displayed on the LED Bar (with no oscillations)

• Lab 3:-\
Aim: Aquire knowledge of serial communication (UART) and interrupts (INTC). The high-level aim of this lab is to have a potentiometer connected to one TI Tiva TM4C1234G board to communicate an encoded value via serial communication to a second TI Tiva TM4C1234G that (1)
displays the value on a LED array bar and (2) sends an acknowledgement (over the serial communication line). The board with the potentiometer will receive the acknowledgement and light an LED on the Tiva for one second.\
Objectives:-
1. A single generic program is developed for both receiver and transmitter Boards.
2. Receiver part of the program is able to handle multiple received bytes
3. UART peripheral should be handled with the aid of interrupt services
4. Propose a transmission encoding system that represents the 12-bit POT position. Also, the system must send back an acknowledgement to transmitter board. By receiving an acknowledgment, the blue LED of the launchpad should be illuminated.
5. Propose a method to prevent receiver getting overloaded by not sending of small changes of POT position.
6. The ADC Value is read and scaled properly and displayed on the LED Bar (with no oscillations)

• Lab 4:-\
Aim: Understand the usage of LCD and Accelerometer (Educational BoosterPack MKII) connected to the TI Tiva C Series TM4C123G LaunchPad. Show a ball “rolling” around on the Tiva LCD 128 x 128 pixel screen that reacts to the orientation of the board’s tilt. The tilt is measured using the accelerometer on one of the system boards.  
Objectives:-
1. A “ball” moves around the LCD screen depending on the tilt of the board.
2. The movement of the ball is smooth and does not change more than one pixel every 50 milliseconds.
3. When the system is sitting flat on the table, the “ball” is in the center of the screen.
4. When the system is oriented such that the board surface is at a 5 degree angle while one edge is horizontal, the “ball” is half way 5. between the edge and the center of the screen (test X and Y)
6. When the system is oriented such that the board surface is at a 10 degree angle while one edge is horizontal for an extended amount of time, the “ball” is at the edge of the screen (test X and Y)
7. The ball size (2, 4 or 8 pixels) shall be set by the location it is on the screen, such that the size changes at about the 1/3 and 2/3 of the distance from the center to the edge of the LCD screen.


