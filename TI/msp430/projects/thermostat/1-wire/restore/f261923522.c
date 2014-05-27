#include <msp430g2553.h>
#include <stdio.h>
#include <stdint.h>
 
#include "unify.h"
#include "onewire.h"
#include "delay.h"

/***************************************************************/

int onewire_reset(onewire_t *ow)
{
  onewire_line_low(ow);
  DELAY_US(700); //550, 480us minimum
  onewire_line_release(ow);
  DELAY_US(20); //70, slave waits 15-60us
  if (*(ow->port_in) & ow->pin)
	{
		printf("Line pulled down!");
		return 1; // line should be pulled down by slave
	}
  DELAY_US(240); //300, slave TX presence pulse 60-240us
  if (!(*(ow->port_in) & ow->pin)) 
	{
    printf("Line released!");
		return 2; // line should be "released" by slave
	}
    printf("Reseted!");
  return 0;
}

//#####################################################################

void onewire_write_bit(onewire_t *ow, int bit)
{
  DELAY_US(4); // recovery, min 1us
  onewire_line_low(ow);
  if (bit)
    DELAY_US(10); // max 15us
  else
    DELAY_US(80); // min 60us
  onewire_line_release(ow);
  // rest of the write slot
  if (bit)
    DELAY_US(80);
  else
    DELAY_US(10);
}

//#####################################################################

int onewire_read_bit(onewire_t *ow)
{
  int bit;
  DELAY_US(5); // recovery, min 1us
  onewire_line_low(ow);
  DELAY_US(5); // hold min 1us
  onewire_line_release(ow);
  DELAY_US(15); // 15us window
  bit = *(ow->port_in) & ow->pin;
  DELAY_US(60); // rest of the read slot
  return bit;
}

//#####################################################################

void onewire_write_byte(onewire_t *ow, uint8_t byte)
{
  int i;
  for(i = 0; i < 8; i++)
  {
    onewire_write_bit(ow, byte & 1);
    byte >>= 1;
  }
}

//#####################################################################

uint8_t onewire_read_byte(onewire_t *ow)
{
  int i;
  uint8_t byte = 0;
  for(i = 0; i < 8; i++)
  {
    byte >>= 1;
    if (onewire_read_bit(ow)) byte |= 0x80;
  }
  return byte;
}

//#####################################################################

inline void onewire_line_low(onewire_t *ow)
{
  *(ow->port_dir) |= ow->pin;
  *(ow->port_out) &= ~ow->pin;
  *(ow->port_ren) &= ~ow->pin;
}

//#####################################################################

inline void onewire_line_high(onewire_t *ow)
{
  *(ow->port_dir) |= ow->pin;
  *(ow->port_out) |= ow->pin;
  *(ow->port_ren) &= ~ow->pin;
}

//#####################################################################

inline void onewire_line_release(onewire_t *ow)
{
  *(ow->port_dir) &= ~ow->pin; // input
  *(ow->port_ren) |= ow->pin;
	printf("Internal resistor pullup\n");
  *(ow->port_out) |= ow->pin; // internal resistor pullup
}

//#####################################################################
 
int putchar(int c)
{
  if (c == '\n') putchar('\r');
  while (!(IFG2&UCA0TXIFG));
  UCA0TXBUF = c;
  return 0;
}
 

/***************************************************************/
 
void search(onewire_t *ow, uint8_t *id, int depth, int reset)
{
  int i, b1, b2;
 
  if (depth == 64)
  {
    // we have all 64 bit in this search branch
    printf("Founded: ");
    for (i = 0; i < 8; i++) printf("%02x", id[i]);
    printf("\n");
		DELAY_MS(1000);
    return;
  }

  printf("depth != 64 then reset\n");
  //printf(".%i\n",temperature[1]); 
	DELAY_MS(1000);
 
  if (reset)
  {
  printf("Reset!\n");
    if (onewire_reset(ow) != 0) { printf("reset failed\n"); return; }
    onewire_write_byte(ow, 0xF0); // search ROM command
 
    // send currently recognized bits
    for (i = 0; i < depth; i++)
    {
      b1 = onewire_read_bit(ow);
      b2 = onewire_read_bit(ow);
      onewire_write_bit(ow, id[i / 8] & (1 << (i % 8)));
    }
  }
 
  printf("check another bit\n");
  // check another bit
  b1 = onewire_read_bit(ow);
  b2 = onewire_read_bit(ow);
  //printf("%d\n", b1);
  //printf("%d\n", b2);
  printf("%d\n", b1 & 0x1);
  printf("%d\n", b2 & 0x1);
  if (b1 && b2) 
	{
		printf("No response to search!\n");
		return;
	}
  if (!b1 && !b2) // two devices with different bits on this position
  {
    // check devices with this bit = 0
    printf("bit1 = 0\n");
		DELAY_MS(1000);
    onewire_write_bit(ow, 0);
    id[depth / 8] &= ~(1 << (depth % 8));
    search(ow, id, depth + 1, 0);
    // check devices with this bit = 1
    id[depth / 8] |= 1 << (depth % 8);
    search(ow, id, depth + 1, 1); // different branch, reset must be issued
  } else if (b1) {
    // devices have 1 on this position
    onewire_write_bit(ow, 1);
    id[depth / 8] |= 1 << (depth % 8);
    search(ow, id, depth + 1, 0);

  } else if (b2) {
    //devices have 0 on this position
    printf("bit2 = 0\n");
    onewire_write_bit(ow, 0);
    id[depth / 8] &= ~(1 << (depth % 8));
    search(ow, id, depth + 1, 0);
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