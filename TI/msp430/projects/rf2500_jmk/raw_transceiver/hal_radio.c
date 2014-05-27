#include <stdint.h>
#include <io.h>
#include "hardware.h"
#include "cc2500_regs.h"
#include "hal_spi_radio.h"
#include "smartrf_CC2500.h"
#include "hal_radio.h"

#define DELAY(x)                   do { uint16_t i; for( i=4*x; i>0; --i ) asm("nop"); } while (0);

#define MRFI_GDO_SYNC           6
#define MRFI_GDO_CCA            9
#define MRFI_GDO_PA_PD          27  /* low when transmit is active, low during sleep */
#define MRFI_GDO_LNA_PD         28  /* low when receive is active, low during sleep */

static const uint8_t radio_regs_config[][2] =
{
  /* internal radio configuration */
  { MRFI_CC2500_SPI_REG_IOCFG0,    MRFI_GDO_SYNC   },
  { MRFI_CC2500_SPI_REG_IOCFG2,    MRFI_GDO_PA_PD   },
  { MRFI_CC2500_SPI_REG_MCSM1,     0x3F    },
  { MRFI_CC2500_SPI_REG_MCSM0,     0x10 | (SMARTRF_SETTING_MCSM0 & (1<<2|1<<3))    },
/*  { MRFI_CC2500_SPI_REG_PKTLEN,    MRFI_SETTING_PKTLEN   },*/
  { MRFI_CC2500_SPI_REG_PKTCTRL0,  0x05 | (SMARTRF_SETTING_PKTCTRL0 & 1<<6) }, 
/*  { MRFI_CC2500_SPI_REG_PATABLE,   SMARTRF_SETTING_PATABLE0 }, */
  { MRFI_CC2500_SPI_REG_CHANNR,    SMARTRF_SETTING_CHANNR   },
/*  { MRFI_CC2500_SPI_REG_FIFOTHR,   0x07 | (SMARTRF_SETTING_FIFOTHR & (1<<4|1<<5|1<<6))  },*/
  /* imported SmartRF radio configuration */
  { MRFI_CC2500_SPI_REG_FSCTRL1,   SMARTRF_SETTING_FSCTRL1  },
  { MRFI_CC2500_SPI_REG_FSCTRL0,   SMARTRF_SETTING_FSCTRL0  },
  { MRFI_CC2500_SPI_REG_FREQ2,     SMARTRF_SETTING_FREQ2    },
  { MRFI_CC2500_SPI_REG_FREQ1,     SMARTRF_SETTING_FREQ1    },
  { MRFI_CC2500_SPI_REG_FREQ0,     SMARTRF_SETTING_FREQ0    },
  { MRFI_CC2500_SPI_REG_MDMCFG4,   SMARTRF_SETTING_MDMCFG4  },
  { MRFI_CC2500_SPI_REG_MDMCFG3,   SMARTRF_SETTING_MDMCFG3  },
  { MRFI_CC2500_SPI_REG_MDMCFG2,   SMARTRF_SETTING_MDMCFG2  },
  { MRFI_CC2500_SPI_REG_MDMCFG1,   SMARTRF_SETTING_MDMCFG1  },
  { MRFI_CC2500_SPI_REG_MDMCFG0,   SMARTRF_SETTING_MDMCFG0  },
  { MRFI_CC2500_SPI_REG_DEVIATN,   SMARTRF_SETTING_DEVIATN  },
  { MRFI_CC2500_SPI_REG_FOCCFG,    SMARTRF_SETTING_FOCCFG   },
  { MRFI_CC2500_SPI_REG_BSCFG,     SMARTRF_SETTING_BSCFG    },
  { MRFI_CC2500_SPI_REG_AGCCTRL2,  SMARTRF_SETTING_AGCCTRL2 },
  { MRFI_CC2500_SPI_REG_AGCCTRL1,  SMARTRF_SETTING_AGCCTRL1 },
  { MRFI_CC2500_SPI_REG_AGCCTRL0,  SMARTRF_SETTING_AGCCTRL0 },
  { MRFI_CC2500_SPI_REG_FREND1,    SMARTRF_SETTING_FREND1   },
  { MRFI_CC2500_SPI_REG_FREND0,    SMARTRF_SETTING_FREND0   },
  { MRFI_CC2500_SPI_REG_FSCAL3,    SMARTRF_SETTING_FSCAL3   },
  { MRFI_CC2500_SPI_REG_FSCAL2,    SMARTRF_SETTING_FSCAL2   },
  { MRFI_CC2500_SPI_REG_FSCAL1,    SMARTRF_SETTING_FSCAL1   },
  { MRFI_CC2500_SPI_REG_FSCAL0,    SMARTRF_SETTING_FSCAL0   },
  { MRFI_CC2500_SPI_REG_TEST2,     SMARTRF_SETTING_TEST2    },
  { MRFI_CC2500_SPI_REG_TEST1,     SMARTRF_SETTING_TEST1    },
  { MRFI_CC2500_SPI_REG_TEST0,     SMARTRF_SETTING_TEST0    },
  
  /*JMK:*/
  { MRFI_CC2500_SPI_REG_IOCFG0, 6 },
  { MRFI_CC2500_SPI_REG_FIFOTHR, 2 /*RX>8*/},
  { MRFI_CC2500_SPI_REG_PKTCTRL1, 1<<2 /*Append status, no addr check*/ },
  { MRFI_CC2500_SPI_REG_MCSM0, 1<<4|1<<2 },
//  { MRFI_CC2500_SPI_REG_PKTLEN, 16 },
  { MRFI_CC2500_SPI_REG_PATABLE, 0xfe },
};


int hal_radio_init()
{
	hal_spi_init();
	RADIO_CS_SET(1);
	DELAY(10);
	RADIO_CS_SET(0);
	DELAY(100);
	/* pull CSn low and wait for SO to go low */
	RADIO_CS_SET(1);
	while (P3IN & (1<<BN_P3_CC2500_MISO));
	SPI_WRITE_BYTE( MRFI_CC2500_SPI_STROBE_SRES );
	SPI_WAIT_DONE();
	while (P3IN & (1<<BN_P3_CC2500_MISO));
	RADIO_CS_SET(0);
#define TEST_VALUE 0xA5
	hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTLEN, TEST_VALUE );
	if (hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_PKTLEN)!=TEST_VALUE)
		return 1;
	uint8_t i;
	for( i=0; i<sizeof(radio_regs_config)/sizeof(radio_regs_config[0]); ++i )
		hal_spi_radio_writeReg( radio_regs_config[i][0], radio_regs_config[i][1] );
	hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SRX );
	return 0;
}

int hal_radio_receive_frame( struct radio_frame_s* precvframe )
{
	uint8_t rxlen = hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_RXBYTES);
	for(;;) {
		uint8_t rxlen_v;
		rxlen_v = hal_spi_radio_readReg(MRFI_CC2500_SPI_REG_RXBYTES);
		if (rxlen_v==rxlen)
			break;
		rxlen = rxlen_v;
	}
	precvframe->length = rxlen;
	if (rxlen==0)
		return 0;
	if (rxlen>precvframe->maxlength)
		precvframe->length = precvframe->maxlength;
	hal_spi_radio_readRxFifo( precvframe->ptr, precvframe->length );
	if (rxlen>precvframe->length) {
		hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SIDLE );
		hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SFRX );
		hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_SRX );
	}
	return 0;
}

int hal_radio_send_frame( const struct radio_frame_s* pframe )
{
//	uint8_t retries_decount = 8;
	hal_spi_radio_writeReg( MRFI_CC2500_SPI_REG_PKTLEN, pframe->length );
	hal_spi_radio_writeTxFifo( pframe->ptr, pframe->length );
	hal_spi_radio_cmdStrobe( MRFI_CC2500_SPI_STROBE_STX );
	return 0;
}
