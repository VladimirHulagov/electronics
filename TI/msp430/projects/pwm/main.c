#include "msp430g2553.h"

#define GRN_LED BIT6

int pwmDirection = 1;

void main(void)
{
    /* Set watchdog timer interval to 32ms, internal timer */
    WDTCTL = WDT_MDLY_32;

    /* enable interrupts for the watchdog timer */
    IE1 |= WDTIE;

    /* Set green LED for ouput and then to pulse width modulation */
    P1DIR |= GRN_LED;
    P1SEL |= GRN_LED;

    /* The count that determines the PWM period */
    CCR0 = 1000-1;

    /* CCR1 is the PWM duty cycle, i.e. how much of the cycle is on vs. off */
    CCR1 = 1;

    /* CCR1 reset/set -- high voltage below count and low voltage when past */
    CCTL1 = OUTMOD_7;

    /* Timer A control set to submain clock TASSEL_2 and count up mode MC_1 */
    TACTL = TASSEL_2 + MC_1;

    /* go to sleep, low power mode 0 */
    __bis_SR_register(LPM0_bits + GIE);

    /* infinite loop */
    for( ; ; ) { }
}

/* Watchdog Timer interrupt service routine.  The function prototype
 * tells the compiler that this will service the Watchdog Timer, and
 * then the function follows.
 */
void watchdog_timer(void) __attribute__((interrupt(WDT_VECTOR)));
void watchdog_timer(void)
{
    CCR1 += pwmDirection*20;

    if( CCR1 > 980 || CCR1 < 20 ) pwmDirection = -pwmDirection;
}
