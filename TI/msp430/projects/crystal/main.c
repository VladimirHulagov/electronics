#include "msp430g2553.h"

#define RED_LED BIT0
#define GRN_LED BIT6
#define BUTTON  BIT3

int blink_mode = 0;        // which mode we start in
int blink_mask = RED_LED;  // which lights we blink
int wdtCounter = 0;

int main(void) {
    /* Set watchdog timer interval to 16ms, won't work without external crystal */
    WDTCTL = WDT_ADLY_16;

    /* enable interrupts for the watchdog timer */
    IE1 |= WDTIE;

    /* Enable interrupts for our button */
    P1IE |= BUTTON;

    /* Clear our BUTTON in the Port 1 interrupt flag. */
    P1IFG &= ~BUTTON;

    /* Configure LED pins on port 1 as output pins */
    P1DIR |= RED_LED + GRN_LED;

    /* make sure green is turned off */
    P1OUT &= ~GRN_LED;

    /* go to sleep, low power mode 0 */
    __bis_SR_register( LPM3_bits + GIE );

    /* infinite loop */
    for( ; ; ) { }
}

/* Port 1 interrupt service routine.  This is for handling
 * our button presses.  First, this prototype tells
 * the compiler that the function handles interrupts for
 * Port 1.  Then the function follows.
 */
void Port_1(void) __attribute__((interrupt(PORT1_VECTOR)));
void Port_1(void)
{
    /* Clear the interrupt flag */
    P1IFG &= ~BUTTON; // P1.3 IFG cleared

    /* Switch blink modes */
    blink_mode = (blink_mode+1)%4;

    switch( blink_mode ) {
        case 0: /* blink red only - green off */
            blink_mask = RED_LED;
            P1OUT &= ~GRN_LED;
            break;
        case 1: /* blink green only - red off */
            blink_mask = GRN_LED;
            P1OUT &= ~RED_LED;
            break;
        case 2: /* blink red and green alternately */
            blink_mask = GRN_LED + RED_LED;
            P1OUT |= RED_LED;
            P1OUT &= ~GRN_LED;
            break;
        case 3: /* blink red and green together */
            blink_mask = GRN_LED + RED_LED;
            P1OUT |= RED_LED + GRN_LED;
            break;
    }
}

/* Watchdog Timer interrupt service routine.  The function prototype
 * tells the compiler that this will service the Watchdog Timer, and
 * then the function follows.
 */
void watchdog_timer(void) __attribute__((interrupt(WDT_VECTOR)));
void watchdog_timer(void)
{
    wdtCounter++;

    /* Count 20 interrupts x 16ms = 320ms, about 1/3 second */
    if(wdtCounter == 20) {
        /* Reset the counter for the next blink */
        wdtCounter = 0;

        /* blink the LEDs */
        P1OUT ^= blink_mask;
    }
}
