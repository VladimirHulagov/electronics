#include <stdint.h>
#include <io.h>
#include "hardware.h"
#include "cc2500_regs.h"
#include "hal_spi_radio.h"


void hal_spi_init()
{
	UCB0CTL1 = UCSWRST;
	UCB0CTL1 = UCSWRST | UCSSEL1;
	UCB0CTL0 = UCCKPH | UCMSB | UCMST | UCSYNC;
	UCB0BR0  = 2;
	UCB0BR1  = 0;
	P3SEL |= 1<<BN_P3_CC2500_CLK|1<<BN_P3_CC2500_MOSI|1<<BN_P3_CC2500_MISO;
	UCB0CTL1 &= ~UCSWRST;
}

#define DUMMY_BYTE                  0xDB
#define READ_BIT                    0x80
#define BURST_BIT                   0x40

uint8_t hal_spi_radio_cmdStrobe( uint8_t addr )
{
	if (!(addr>=0x30 && addr<=0x3D))
		return -1;
	RADIO_CS_SET(0);
	RADIO_CS_SET(1);
	SPI_WRITE_BYTE( addr );
	SPI_WAIT_DONE();
	uint8_t status = SPI_READ_BYTE();
	RADIO_CS_SET(0);
	return status;
}

static uint8_t hal_spi_radio_register( uint8_t addr, uint8_t value )
{
	RADIO_CS_SET(0);
	RADIO_CS_SET(1);
	SPI_WRITE_BYTE( addr );
	SPI_WAIT_DONE();
	SPI_WRITE_BYTE( value );
	SPI_WAIT_DONE();
	uint8_t retVal = SPI_READ_BYTE();
	RADIO_CS_SET(0);
	return retVal;
}

static void hal_spi_radio_fifo( uint8_t addr, uint8_t* pData, uint8_t len )
{
	if (len==0)
		return;
	if (!(addr & BURST_BIT))
		return;
	RADIO_CS_SET(0);
	RADIO_CS_SET(1);
	SPI_WRITE_BYTE( addr );
	SPI_WAIT_DONE();
	for( ; len>0 ; --len, ++pData ) {
		SPI_WRITE_BYTE( *pData );
		SPI_WAIT_DONE();
		if (addr & READ_BIT)
			*pData = SPI_READ_BYTE();
	}
	RADIO_CS_SET(0);
}


uint8_t hal_spi_radio_readReg( uint8_t addr )
{
	return hal_spi_radio_register( addr | BURST_BIT | READ_BIT, DUMMY_BYTE );
}

void hal_spi_radio_writeReg( uint8_t addr, uint8_t value )
{
	hal_spi_radio_register( addr, value );
}

void hal_spi_radio_writeTxFifo( const uint8_t* pData, uint8_t len )
{
	hal_spi_radio_fifo( MRFI_CC2500_SPI_REG_TXFIFO | BURST_BIT, (uint8_t*)pData, len );
}

void hal_spi_radio_readRxFifo( uint8_t* pData, uint8_t len )
{
	hal_spi_radio_fifo( MRFI_CC2500_SPI_REG_RXFIFO | BURST_BIT | READ_BIT, pData, len );
}

void hal_spi_radio_writePAtable01( uint8_t pa0, uint8_t pa1 )
{
	uint8_t buf[2] = { pa0, pa1 };
	hal_spi_radio_fifo( MRFI_CC2500_SPI_REG_PATABLE | BURST_BIT, buf, 2 );
}
