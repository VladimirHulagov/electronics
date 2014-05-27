#include <msp430g2553.h>

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  BCSCTL1 = CALBC1_8MHZ;//Проц тактируем от встроенного
  DCOCTL  = CALDCO_8MHZ;//генератора с калибровкой последнего на 16 мгц
  BCSCTL3 = XCAP_1;//встроенная нагрузочная емкость в 6 пФ
  P1DIR |= BIT0 + BIT6;
  CCTL0 = CCIE;                             // CCR0 interrupt enabled
  CCTL1 = CCIE;                             // CCR0 interrupt enabled
  CCR0 = 50;
  CCR1 = 6000;
  TACTL = TASSEL_2 + MC_1 + TAIE;                  // SMCLK, upmode

  _BIS_SR(LPM0_bits + GIE);                 // Enter LPM0 w/ interrupt
}
void TimerA0 (void) __attribute__((interrupt(TIMER0_A0_VECTOR)));
void TimerA0(void) {
  P1OUT ^= BIT0;                            // Toggle P1.0
  CCR0 += 5;
}

void TimerA1 (void) __attribute__((interrupt(TIMER0_A1_VECTOR)));
void TimerA1(void) {
  P1OUT ^= BIT6;
  CCR1 += 5;
}


