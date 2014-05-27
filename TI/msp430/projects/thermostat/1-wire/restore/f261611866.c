/* 
 * test for TI LaunchPad
 * @author: David Siroky (siroky@dasir.cz)
 * @license: public domain
 */
#include <msp430g2231.h>
#include <msp430.h>
#include <stdio.h>
#include <stdint.h>
 
#include "onewire.h"
#include "delay.h"

/***************************************************************/

int onewire_reset(onewire_t *ow)
{
  onewire_line_low(ow);
  DELAY_US(700); //550, 480us minimum
  onewire_line_release(ow);
  DELAY_US(90); //70, slave waits 15-60us
  if (*(ow->port_in) & ow->pin) return 1; // line should be pulled down by slave
  DELAY_US(700); //300, slave TX presence pulse 60-240us
  if (!(*(ow->port_in) & ow->pin)) return 2; // line should be "released" by slave
  return 0;
}

//#####################################################################

void onewire_write_bit(onewire_t *ow, int bit)
{
  DELAY_US(2); // recovery, min 1us
  onewire_line_low(ow);
  if (bit)
    DELAY_US(6); // max 15us
  else
    DELAY_US(64); // min 60us
  onewire_line_release(ow);
  // rest of the write slot
  if (bit)
    DELAY_US(64);
  else
    DELAY_US(6);
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
  *(ow->port_out) |= ow->pin; // internal resistor pullup
}

/***************************************************************/
 
void search(onewire_t *ow, uint8_t *id, int depth, int reset)
{
  int i, b1, b2;
 
  //if (depth == 64)
  //{
    // we have all 64 bit in this search branch
    //printf("found: ");
    //for (i = 0; i < 8; i++) printf("%02x", id[i]);
    //printf("\n");
    //return;
  //}

  //printf("depth != 64 then reset\n");
 
  if (reset)
  {
  //printf("reset\n");
    if (onewire_reset(ow) != 0) {/* printf("reset failed\n"); */return; }
    onewire_write_byte(ow, 0xF0); // search ROM command
 
    // send currently recognized bits
    for (i = 0; i < depth; i++)
    {
      b1 = onewire_read_bit(ow);
      b2 = onewire_read_bit(ow);
      onewire_write_bit(ow, id[i / 8] & (1 << (i % 8)));
    }
  }
 
  //printf("check another bit\n");
  // check another bit
  b1 = onewire_read_bit(ow);
  b2 = onewire_read_bit(ow);
  //printf("%d\n", b1 & 0x1);
  //printf("%d\n", b2 & 0x1);
  if (b1 && b2) return; // no response to search
  if (!b1 && !b2) // two devices with different bits on this position
  {
    // check devices with this bit = 0
    //printf("bit1 = 0\n");
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
  //} else if (b2) {
    // devices have 0 on this position
   // printf("bit2 = 0\n");
    //onewire_write_bit(ow, 0);
    //id[depth / 8] &= ~(1 << (depth % 8));
    //search(ow, id, depth + 1, 0);
  }
}
 

/***************************************************************/
void timer_setup() {
  BCSCTL1 = CALBC1_1MHZ;
  DCOCTL = CALDCO_1MHZ;
  CCTL0 = CCIE; // CCR0 interrupt enabled
  TACCTL0 = CCIE;
  TACCR0 = 6250; // DCO = 1Mhz, T = 8000000/2*6250 = 80Hz
  //TACCR0 = 50000; // DCO = 8Mhz, T = 8000000/2*50000 = 80Hz
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