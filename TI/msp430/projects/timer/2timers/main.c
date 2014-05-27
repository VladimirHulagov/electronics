#include <msp430g2553.h>

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  BCSCTL1 = CALBC1_8MHZ;//Проц тактируем от встроенного
  DCOCTL  = CALDCO_8MHZ;//генератора с калибровкой последнего на 16 мгц
  BCSCTL3 = XCAP_1;//встроенная нагрузочная емкость в 6 пФ
  P1DIR |= BIT0 + BIT6;
  TA0CCR0  = 2000;//частота
  TA0CCR1  = 60;//стартовое на ШИМ
  TA0CCR2  = 60;//стартовое на ШИМ
  TA0CCTL0 = CCIE;//
  TA0CCTL1 = CCIE;//разрешить прерывания от все трех регистров
  TA0CCTL2 = CCIE;//

  TA1R = 8000;// смешение относительно первого, чтоб прерывания не накладывались
  TA1CCR0  = 2000;// частота
  TA1CCR1  = 60;//стартовое на ШИМ
  TA1CCR2  = 60;//стартовое на ШИМ
  TA1CCTL0 = CCIE;//
  TA1CCTL1 = CCIE;//разрешить прерывания от все трех регистров
  TA1CCTL2 = CCIE;//

  TA1CTL   = TASSEL_1 + MC_1 + TAIE;//запустили таймера и разрешили прерывания
  TA0CTL   = TASSEL_1 + MC_1 + TAIE;//


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


