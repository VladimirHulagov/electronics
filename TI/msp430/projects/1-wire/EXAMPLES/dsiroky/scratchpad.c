#include <msp430.h>
#include "onewire.h"
#include "delay.h"
 
int main()
{
  onewire_t ow;
  int i;
  uint8_t scratchpad[9];
 
  WDTCTL = WDTPW + WDTHOLD; //Stop watchdog timer
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
 
  ow.port_out = &P1OUT;
  ow.port_in = &P1IN;
  ow.port_ren = &P1REN;
  ow.port_dir = &P1DIR;
  ow.pin = BIT7;
 
  onewire_reset(&ow);
  onewire_write_byte(&ow, 0xcc); // skip ROM command
  onewire_write_byte(&ow, 0x44); // convert T command
  onewire_line_high(&ow);
  DELAY_MS(800); // at least 750 ms for the default 12-bit resolution
  onewire_reset(&ow);
  onewire_write_byte(&ow, 0xcc); // skip ROM command
  onewire_write_byte(&ow, 0xbe); // read scratchpad command
  for (i = 0; i < 9; i++) scratchpad[i] = onewire_read_byte(&ow);
 
  _BIS_SR(LPM0_bits + GIE);
  return 0;
}
