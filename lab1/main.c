/*File name: main.c
 * Description:
 * ------------
 * Initially LED2 blinks
 * Blinking is paused while button S2 is pressed the first time
 * When button S2 is held down for at least 2 seconds, when S2 is finally released LED2
 * stops blinking and LED1 begins blinking.
 * The configuration is reversed when S2 is again pressed for at least 2 seconds again.
 * LED2 and LED1 blink at a frequency of 0.5 Hz, 50% duty cycle
 * Code must be written in C on CCS
 *
 * References:
 * -----------
 * MSP430G2x53/MSP430G2x13 data sheet
 * MSP430x2xx Family User Guide
 * MSP430 Optimizing C/C++ Compiler v17.6.0.STS User's Guide
 * MSP-EXP430G2 LaunchPad Development Kit User's Guide
 * http://www.msp430launchpad.com/2010/07/timers-and-clocks-and-pwm-oh-my.html
 * http://processors.wiki.ti.com/index.php/MSP430_LaunchPad_Tutorials
 * http://referencedesigner.com/blog/understanding-timers-in-ms430-2/2062/
 * http://processors.wiki.ti.com/index.php/MSP430_LaunchPad_Interrupt_vs_Polling
 * Embedded_Coding_Shue.pdf
 * Resource Explorer, peripheral examples - CCS
 *
*/
#include <msp430.h>

#define LED_DELAY 50000
#define TWO_SEC 9000


void delayForLed();
void toggleLed2();
void toggleLed1();

int main(void)
{
    unsigned int switchPressTime = 0;
    unsigned int switchCount = 0;
    volatile unsigned int currentStateOfLed = 0;
    unsigned char switchPressDelayFlag = 0;
    volatile unsigned int switchPressTimeTotal = 0;




    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    P1OUT &= 0xBE;
    P1DIR |= 0x41;  // Set P1.0 to output direction

    P1REN |= 0x08;



     while (1)
     {
        if (switchCount % 2 == 0)
        {
            P1OUT &= 0xFE;
            toggleLed2();
        }
        else
        {
            P1OUT &= 0xBF;
            toggleLed1();
        }

       if (!(P1IN & 0x08))
       {

           P1REN |= 0x08;
           currentStateOfLed = P1OUT;
           P1OUT = currentStateOfLed;


           while ((switchPressTime <= TWO_SEC) && !switchPressDelayFlag)
                     {
                         if (!(P1IN & 0x08))
                         {
                              P1REN |= 0x08;
                             //currentStateOfLed = P1OUT;
                            // P1OUT = currentStateOfLed;
                              switchPressTime = switchPressTime + 1;
                         }
                         else
                         {
                             switchPressDelayFlag = 1;
                         }

                     }

                     switchPressDelayFlag = 0;


                     switchPressTimeTotal = switchPressTimeTotal + switchPressTime;
                     switchPressTime = 0;
       }

       if (P1IN & 0x08)
       {
                     P1REN |= 0x08;
                     if (switchPressTimeTotal >= TWO_SEC+1)
                      {
                          switchCount++;
                                             //switchPressTime = 0;


                      }


                     switchPressTimeTotal = 0;
       }


     }

}

void toggleLed2()
    {
        P1OUT ^= 0x40;    // led2
        delayForLed();


    }

void toggleLed1()
    {
           P1OUT ^= 0x01;    // led1
           delayForLed();

    }

void delayForLed()
    {
          volatile unsigned int i = LED_DELAY;                               // Delay
          do (i--);
          while (i != 0);
    }
