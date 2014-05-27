#include <msp430.h>
#include <signal.h>


#include "hal_spi_radio.h"
#include "hal_radio.h"

struct pin_status_s {
	unsigned int	got_sync:1;
	unsigned int	got_timer:1;
	unsigned int	got_button:1;
	unsigned int	button_locked:1;
};

static volatile struct pin_status_s pin_status;

static void timer_restart();

static void dump_radio_info()
{
	uart0_polled_puts("Modulation format is ");
	switch ((hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG2) & (1<<4|1<<5|1<<6))>>4)
	{
	case 0:	uart0_polled_puts("2-FSK");	break;
	case 1:	uart0_polled_puts("GFSK");	break;
	case 3:	uart0_polled_puts("OOK");	break;
	case 7:	uart0_polled_puts("MSK");	break;
	}
	uart0_polled_putcrlf();
	uart0_polled_puts("Digital DC blocking is ");
	uart0_polled_puts( (hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG2) & (1<<7)) ? "disabled\n" : "enabled\n" );
	uart0_polled_puts("Manchester is ");
	uart0_polled_puts( (hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG2) & (1<<3)) ? "enabled\n" : "disabled\n" );
	uart0_polled_puts("Data whitening is ");
	uart0_polled_puts( (hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTCTRL0) & (1<<6)) ? "enabled\n" : "disabled\n" );
	uart0_polled_puts("FEC is ");
	uart0_polled_puts( (hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG1) & (1<<7)) ? "enabled\n" : "disabled\n" );
}

static void radio_send_reference_frame()
{
	static const char sndbuf[] = "ABCDEFGHIJKLMNOP";
	static const struct radio_frame_s sfr = { .ptr=(char*)sndbuf, .length=sizeof(sndbuf)-1 };
	LED_GREEN_SET(1);
	hal_radio_send_frame( &sfr );
	LED_GREEN_SET(0);
}

static int radio_set_data_mode( int mode )
{
	switch (mode) {
	case 0:
		/*Turn data whitening off*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTCTRL0) & ~(1<<6) );
		/*Turn FEC off*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MDMCFG1, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG1) & ~(1<<7) );
		return 0;
	case 1:
		/*Enable data whitening*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTCTRL0) | 1<<6 );
		/*Turn FEC off*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MDMCFG1, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG1) & ~(1<<7) );
		return 0;
	case 2:
		/*Turn data whitening off*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTCTRL0) & ~(1<<6) );
		/*Turn FEC on*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MDMCFG1, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG1) | 1<<7 );
		return 0;
	case 3:
		/*Enable data whitening*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTCTRL0) | 1<<6 );
		/*Turn FEC on*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MDMCFG1, hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_MDMCFG1) | 1<<7 );
		return 0;
	default:
		return 1;
	}
}

int main()
{
	/* disable watchdog timer */
	WDTCTL = WDTPW | WDTHOLD;

	BCSCTL1 = CALBC1_8MHZ;
	DCOCTL = CALDCO_8MHZ;
	BCSCTL3 |= LFXT1S_2;
	
	P1OUT = BM_P1_DEFSTATE | (BM_P1_DEFPULLUP & ~BM_P1_DEFDIR);
	P1REN = BM_P1_DEFPULLUP | BM_P1_DEFPULLDOWN;
	P1DIR = BM_P1_DEFDIR;

	P2SEL = 0;	/*bit 6 and 7 are assigned to crytal by default!*/
	P2OUT = BM_P2_DEFSTATE | (BM_P2_DEFPULLUP & ~BM_P2_DEFDIR);
	P2REN = BM_P2_DEFPULLUP | BM_P2_DEFPULLDOWN;
	P2DIR = BM_P2_DEFDIR;

	P3OUT = BM_P3_DEFSTATE;
	P3DIR = BM_P3_DEFDIR;
	
	/*Copied from TI...*/
	P3SEL |= 1<<BN_P3_TXD0|1<<BN_P3_RXD0;
	UCA0CTL1 = UCSSEL_SMCLK;
	UCA0BR0 = 0x41;		// 9600 from 8MHz
	UCA0BR1 = 0x3;
	UCA0MCTL = UCBRS_2;                       
	UCA0CTL1 &= ~UCSWRST;		// **Initialize USCI state machine**

	uart0_polled_puts("\n\nEz430-RF2500 Transceiver test...\nby Jean-Marc Koller\n");
	
	if (hal_radio_init()) {
		uart0_polled_puts("Transceiver init error!\n");
		LED_RED_SET(1);
		for(;;);
	}
	
	if (!(P1IN & 1<<BN_P1_NSWITCH)) {
		/*Test mode, unmodulated*/
		hal_spi_radio_writePAtable01( 0xfe, 0xfe );	/*Set +0dBm*/
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MDMCFG2, 0x30 /*Modulation=OOK*/ );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, 0x22 /*Infinite packet length mode; no CRC; Random TX mode, no whitening*/ );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_IOCFG0, 0x0c /*Synchronous Data Output*/ );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_IOCFG2, 0x0b /*Serial Clock*/ );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_DEVIATN, 0 );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_MCSM1, 3<<5 | 0<<2 | 2<<0 /*Stay in TX mode!*/ );
		hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTLEN, 255 );
		hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SFRX );
		hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_STX );
		LED_GREEN_SET(1);
		uart0_polled_puts("Continuous emission mode!\n");
		{uint32_t w;for(w=4*1000*1000UL;w--;)asm("nop");}
		LED_GREEN_SET(0);
		for(;;) {
			uint8_t status = hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SNOP );
			uart0_polled_puts("status=0x");
			uart0_polled_putHex8( status );
			uart0_polled_putcrlf();
			if ((status & (1<<4|1<<5|1<<6))!=(0x2<<4)) {
				hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_STX );
				uint8_t status = hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SNOP );
				uart0_polled_puts("status=0x");
				uart0_polled_putHex8( status );
				uart0_polled_putcrlf();
			}
			{uint32_t w;for(w=2*1000*1000UL;w--;)asm("nop");}
		}
		for(;;)
			LPM0;
	}
	
	P2IES = 1<<BN_P2_CC2500_GDO0;
	P2IFG &= ~(1<<BN_P2_CC2500_GDO0);
	P2IE = 1<<BN_P2_CC2500_GDO0;
	
	TACTL = TASSEL_SMCLK | ID_DIV8 | MC_UPTO_CCR0 | TACLR;
	TACCR0 = 65535;
	TACCTL0 = CM_DISABLE | CCIE;
	
	P1IES = 1<<BN_P1_NSWITCH;
	P1IFG &= ~(1<<BN_P1_NSWITCH);
	P1IE = 1<<BN_P1_NSWITCH;
	
	eint();
	
	LED_GREEN_SET(1);
	
	int radio_mode = 0;
	
	hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTCTRL0, 0<<6|1<<2|0<<0 );
	hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTLEN, 16 );
	radio_set_data_mode( radio_mode );
	dump_radio_info();
	for(;;) {
		if (pin_status.got_sync) {
			pin_status.got_sync = 0;
			#define	PACKBUFSIZE	64
			uint8_t buf[PACKBUFSIZE];
			struct radio_frame_s rfr = { .ptr=buf, .maxlength=PACKBUFSIZE };
			hal_radio_receive_frame( &rfr );
			if (rfr.length>0) {
				LED_GREEN_SET(1);
				uart0_polled_puts("* recv len=0x");
				uart0_polled_putHex8(rfr.length);
				uart0_polled_putcrlf();
				uart0_polled_hexdump( buf, rfr.length-2 );
				const uint8_t* pstatus = &buf[rfr.length-2];
				if (pstatus[1] & 1<<7)
					uart0_polled_puts( "CRC=OK ");
				uart0_polled_puts("LQI=0x");
				uart0_polled_putHex8( pstatus[1] & ((1<<7)-1) );
				if (pstatus[0]>=128) {
					uart0_polled_puts(" RSSI=-0x");
					uart0_polled_putHex8( 256-pstatus[0] );
				} else {
					uart0_polled_puts(" RSSI=0x");
					uart0_polled_putHex8( pstatus[0] );
				}
				uart0_polled_putcrlf();
				if (pstatus[1] & 1<<7) {	/*CRC okay... send a reply!*/
					timer_restart();
					LED_RED_SET(0);
					radio_send_reference_frame();
					pin_status.got_sync = 0;
				}
				LED_GREEN_SET(0);
			}
			pin_status.button_locked = 0;
		}
		if (pin_status.got_timer) {
			pin_status.got_timer = 0;
			radio_send_reference_frame();
			pin_status.got_sync = 0;
			pin_status.button_locked = 0;
		}
		if (pin_status.got_button) {
			pin_status.button_locked = 1;
			LED_GREEN_SET(1);
			LED_RED_SET(1);
			++radio_mode;
			if (radio_set_data_mode(radio_mode)) {
				radio_mode = 0;
				uart0_polled_puts("Radio mode cycled!\n");
				radio_set_data_mode( radio_mode );
			}
			dump_radio_info();
			timer_restart();
			pin_status.got_button = 0;
			LED_GREEN_SET(0);
			LED_RED_SET(0);
		}
		LPM0;
	}
}

interrupt(PORT2_VECTOR) wakeup isr_port2()
{
	if (P2IFG & 1<<BN_P2_CC2500_GDO0) {
		pin_status.got_sync = 1;
		P2IFG &= ~(1<<BN_P2_CC2500_GDO0);
	}
}

uint16_t timer_count;

void timer_restart()
{
	timer_count = 0;
	//TACTL |= TACLR;
}

interrupt(TIMERA0_VECTOR) wakeup isr_timera0()
{
	if (++timer_count>16*2) {
		LED_RED_TOGGLE();
		timer_count = 0;
		pin_status.got_timer = 1;
	}
}

interrupt(PORT1_VECTOR) wakeup isr_port1()
{
	if (P1IFG & 1<<BN_P1_NSWITCH) {
		if (!pin_status.button_locked)
			pin_status.got_button = 1;
		P1IFG &= ~(1<<BN_P1_NSWITCH);
	}
}
