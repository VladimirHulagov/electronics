/* 
 * test for TI LaunchPad
 * @author: David Siroky (siroky@dasir.cz)
 * @license: public domain
 */
#include <msp430g2231.h>
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
 
//#####################################################################
 
int putchar(int c)
{
  if (c == '\n') putchar('\r');
  while (!(IFG2&UCA0TXIFG));
  UCA0TXBUF = c;
  return 0;
}
 

}
 
/*From Slau144i Documentation *********************************/
void uart_setup() {
  P1SEL = BIT1 + BIT2;		//Set P1.1 & P1.2 to hardware UART mode
  P1SEL2 = BIT1 + BIT2;		//Slau144i, pg.337
  P1DIR &= ~ BIT1;		//P1DIR for P1.1=0, i.e. RX
  P1DIR |= BIT2;		//P1DIR for P1.2=1, i.e. TX
  UCA0CTL1 |= UCSSEL_2;                     // SMCLK
  UCA0BR0 = 0x41;                            // 8MHz 9600
  UCA0BR1 = 0x03;                              // 8MHz 9600
  UCA0MCTL = UCBRS0;                        // Modulation UCBRSx = 1
  UCA0CTL1 &= ~UCSWRST;
}

/***************************************************************/
void timer_setup() {
  printf("timer\n");
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
  CCTL0 = CCIE; // CCR0 interrupt enabled
  TACCTL0 = CCIE;
  TACCR0 = 50000; // DCO = 8Mhz, T = 8000000/2*50000 = 80Hz
  TACTL = TASSEL_2 + MC_1;
  _BIS_SR( LPM0_bits + GIE );
}

/***************************************************************/

void DS18B20_measuring() {
onewire_t ow;
  int i;
  uint8_t scratchpad[9];
  uint8_t id[8];
  unsigned int tmp = 0;
  unsigned char temperature[1];

  ow.port_out = &P1OUT;
  ow.port_in = &P1IN;
  ow.port_ren = &P1REN;
  ow.port_dir = &P1DIR;
//