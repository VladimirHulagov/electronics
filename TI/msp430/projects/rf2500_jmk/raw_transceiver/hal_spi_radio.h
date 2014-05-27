
extern	void hal_spi_init();
extern	uint8_t hal_spi_radio_cmdStrobe( uint8_t addr );
extern	uint8_t hal_spi_radio_readReg( uint8_t addr );
extern	void hal_spi_radio_writeReg( uint8_t addr, uint8_t value );
extern	void hal_spi_radio_writeTxFifo( const uint8_t* pData, uint8_t len );
extern	void hal_spi_radio_readRxFifo( uint8_t* pData, uint8_t len );
extern	void hal_spi_radio_writePAtable01( uint8_t pa0, uint8_t pa1 );

#define SPI_WRITE_BYTE(x)                do { IFG2 &= ~UCB0RXIFG;  UCB0TXBUF = x; } while(0)
#define SPI_READ_BYTE()                  UCB0RXBUF
#define SPI_WAIT_DONE()                  while(!(IFG2 & UCB0RXIFG));
