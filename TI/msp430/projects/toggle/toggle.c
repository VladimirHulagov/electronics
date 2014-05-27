#include  <msp430g2553.h>

void main(void)
{
  P1DIR |= 0x41;                            // Set P1.0 to output
  P1OUT ^= 0x01;                            // Toggle P1.0
  _BIS_SR(LPM4_bits);                       // Stop all clocks
}

