#include  <msp430x20x3.h>

void main(void)
{
  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
  P1DIR = 0x01;                             // P1.0 output, else input
  P1OUT =  0x10;                            // P1.4 set, else reset
  P1REN |= 0x10;                            // P1.4 pullup
  P1IE |= 0x10;                             // P1.4 interrupt enabled
  P1IES |= 0x10;                            // P1.4 Hi/lo edge
  P1IFG &= ~0x10;                           // P1.4 IFG cleared

  _BIS_SR(LPM4_bits + GIE);                 // Enter LPM4 w/interrupt
}

// Port 1 interrupt service routine
void Port_1(void) 
__attribute__((interrupt(PORT1_VECTOR)));
void Port_1(void)
{
  P1OUT ^= 0x01;				// P1.0 = toggle
  P1IFG &= ~0x10;				// P1.4 IFG cleared
  for (unsigned int s = 0; s < 20000; s++){ };	// Delay do enimenate debouncing
}


