/*
 * 1-Wire implementation for MSP430
 *
 * @author: David Siroky <siroky@dasir.cz>
 * @license: MIT
 */

#include <msp430g2553.h>
#include <stdint.h>
#include "onewire.h"
#include "delay.h"


/***************************************************************/


void timer_init()
{
    CCTL0=CCIE;
    CCR0=50000;
    TACTL=TASSEL_1+MC_1;  // 采用ACLK辅助时钟，增减计数模式，开定时器中断
    _BIS_SR(LPM0_bits + GIE);                 // Enter LPM0 w/ interrupt
}

void uart_setup() {
  P1SEL = BIT1 + BIT2;
  P1SEL2 = BIT1 + BIT2;
  P1DIR &= ~ BIT1;
  P1DIR |= BIT2;
  UCA0CTL1 |= UCSSEL_2;                     // SMCLK
  UCA0BR0 = 0x41;                            // 8MHz 9600
  UCA0BR1 = 0x03;                              // 8MHz 9600
  UCA0MCTL = UCBRS0;                        // Modulation UCBRSx = 1
  UCA0CTL1 &= ~UCSWRST;
}

void put_char(unsigned char dat)
{
	while (!(IFG2&UCA0TXIFG));                // USART1 TX buffer ready?
    UCA0TXBUF = dat;                          // RXBUF1 to TXBUF1
}

void put_string(char *str)
{
    while((*str)!='\0')
    {
        put_char(*str);
        str++;
    }
}

/***************************************************************/

int putchar(float c)
{
  if (c == '\n') putchar('\r');
  while (!(IFG2&UCA0TXIFG));
  UCA0TXBUF = c;
  return 0;
}

/***************************************************************/

void UARTWriteString(char *str)
{

  while (*str)
  {
    while (!(IFG2&UCA0TXIFG)); // 橡钼屦赅 泐蝾忭铖蜩 狍翦疣 铗镳噔觇.
    UCA0TXBUF = *str++;
  }
}

/******************************************************************************/
//名称：usart_integer()
//功能：发送一个长整形数据
//入口参数：unsigned long int data
//出口参数：无
/******************************************************************************/
void usart_integer(unsigned long int data)  //不能显示为0；
{
    int dis[8];
    int i=0;
    for(i=7;i>=0;i--)
    {
        dis[i] = '0'+data%10;
        data = data/10;
    }
    for(i=0;i<8;i++)
    {
	if(dis[i] != '0')
	break;
    }
    for(;i<=7;i++)
    {
	put_char(dis[i]);
    }
}
void usart_float(float data)
{
    unsigned long int integer_part;
    unsigned long int decimal_part;
    float buffer;
    integer_part = (unsigned long int)data;
    buffer = data-integer_part;
    decimal_part = (int)(buffer*10000);
    usart_integer(integer_part);
    put_char('.') ;
    usart_integer(decimal_part);
}

void ow_portsetup() {
	OWPORTDIR |= OWPORTPIN;
	OWPORTOUT |= OWPORTPIN;
	OWPORTREN |= OWPORTPIN;
}

/***************************************************************/

void search(uint8_t *id, int depth, int reset)
{
  int i, b1, b2, f;

  if (depth == 64)
  {
    // we have all 64 bit in this search branch
	  UARTWriteString("found: ");
    for (i = 0; i < 8; i++) {
    	UARTWriteString("%02x");
    	putchar(id[i]);
    }

    UARTWriteString("\n");
    return;
  }

  if (reset) {
    if (f = onewire_reset()) {
    	UARTWriteString("reset failed\n");

    	if (f == 1) {
    		UARTWriteString("not pulldown\n");
    	}
    	if (f == 2) {
    	 	UARTWriteString("must be released\n");
    	 }
    	return;
    }
    onewire_write_byte(0xF0); // search ROM command

    // send currently recognized bits
    for (i = 0; i < depth; i++)
    {
      b1 = onewire_read_bit();
      b2 = onewire_read_bit();
      onewire_write_bit(id[i / 8] & (1 << (i % 8)));
    }
  }

  // check another bit
  b1 = onewire_read_bit();
  b2 = onewire_read_bit();
  if (b1 && b2) {
		UARTWriteString("no response to search\n");
	  return; // no response to search
  };
  if (!b1 && !b2) // two devices with different bits on this position
  {
    // check devices with this bit = 0
    onewire_write_bit(0);
    id[depth / 8] &= ~(1 << (depth % 8));
    search(id, depth + 1, 0);
    // check devices with this bit = 1
    id[depth / 8] |= 1 << (depth % 8);
    search(id, depth + 1, 1); // different branch, reset must be issued
  } else if (b1) {
    // devices have 1 on this position
    onewire_write_bit(1);
    id[depth / 8] |= 1 << (depth % 8);
    search(id, depth + 1, 0);
  } else if (b2) {
    // devices have 0 on this position
    onewire_write_bit(0);
    id[depth / 8] &= ~(1 << (depth % 8));
    search(id, depth + 1, 0);
  }
}

float ReadDS1820 ( void )
{
	unsigned int i;
	uint16_t byte = 0;
	for(i = 16; i > 0; i--){
		byte >>= 1;
		if (onewire_read_bit()) {
			byte |= 0x8000;
		}
  }
  return byte;
}


float GetData(void)
{
    uint16_t temp;
	  onewire_reset();
	  onewire_write_byte(0xcc); // skip ROM command
	  onewire_write_byte(0x44); // convert T command
	  OW_HI
	  DELAY_MS(750); // at least 750 ms for the default 12-bit resolution
	  onewire_reset();
	  onewire_write_byte(0xcc); // skip ROM command
	  onewire_write_byte(0xbe); // read scratchpad command
	  temp = ReadDS1820();
    if(temp<0x8000){
        return(temp*0.0625);
    }
    else
    {
        temp=(~temp)+1;
        return(temp*0.0625);
    }
}

int onewire_reset()
{
	OW_LO
	DELAY_US(480); // 480us minimum
	OW_RLS
  DELAY_US(40); // slave waits 15-60us
  if (OWPORTIN & OWPORTPIN) return 1; // line should be pulled down by slave
  DELAY_US(300); // slave TX presence pulse 60-240us
  if (!(OWPORTIN & OWPORTPIN)) return 2; // line should be "released" by slave
  return 0;
}

//#####################################################################

void onewire_write_bit(int bit)
{
//  DELAY_US(1); // recovery, min 1us
  OW_HI
  if (bit) {
	OW_LO
    DELAY_US(5); // max 15us
	OW_RLS	// input
    DELAY_US(56);
  }
  else {
	  OW_LO
	  DELAY_US(60); // min 60us
	  OW_RLS	// input
	  DELAY_US(1);
  }
 }

//#####################################################################

int onewire_read_bit()
{
  int bit=0;
//  DELAY_US(1); // recovery, min 1us
  OW_LO
  DELAY_US(5); // hold min 1us
  OW_RLS
  DELAY_US(10); // 15us window
  if (OWPORTIN & OWPORTPIN) {
	  bit = 1;
  }
  DELAY_US(46); // rest of the read slot
  return bit;
}

//#####################################################################

void onewire_write_byte(uint8_t byte)
{
  int i;
  for(i = 0; i < 8; i++)
  {
    onewire_write_bit(byte & 1);
    byte >>= 1;
  }
}

//#####################################################################

uint8_t onewire_read_byte()
{
	unsigned int i;
  uint8_t byte = 0;
  for(i = 0; i < 8; i++)
  {
    byte >>= 1;
    if (onewire_read_bit()) byte |= 0x80;
  }
  return byte;
}



int main()
{
	WDTCTL = WDTPW + WDTHOLD; //Stop watchdog timer
	BCSCTL1 = CALBC1_8MHZ;
	DCOCTL = CALDCO_8MHZ;
	uart_setup();
    timer_init();
	ow_portsetup();
//	temperate = GetData();
//    _BIS_SR(LPM0_bits + GIE);
}

//#####################################################################
//#####################################################################

/// @return: 0 if ok


#pragma vector=TIMER0_A0_VECTOR   //定时器溢出处理部分
__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TIMERA0_ISR(void)
{
     float temperate=GetData();
     put_string("Temperature is ");
     usart_float(temperate);
     put_char('\n');
/*
     put_string("The sunshine is ：");
     if((P1IN&0x01)==1)
         put_string("yes");
     else
         put_string("no");
         put_char('\n');
*/
     CCR0+=50000;
     //delay(100);
}
