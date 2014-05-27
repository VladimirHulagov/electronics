#ifndef MAIN_H_
#define MAIN_H_

#include <msp430g2231.h>
#include <sys/types.h>
#include <stdlib.h>
#include "74hc595.h"

#define CALDCO_16MHZ_         0x10F8    /* DCOCTL  Calibration Data for 16MHz */
const_sfrb(CALDCO_16MHZ, CALDCO_16MHZ_);
#define CALBC1_16MHZ_         0x10F9    /* BCSCTL1 Calibration Data for 16MHz */
const_sfrb(CALBC1_16MHZ, CALBC1_16MHZ_);
#define CALDCO_12MHZ_         0x10FA    /* DCOCTL  Calibration Data for 12MHz */
const_sfrb(CALDCO_12MHZ, CALDCO_12MHZ_);
#define CALBC1_12MHZ_         0x10FB    /* BCSCTL1 Calibration Data for 12MHz */
const_sfrb(CALBC1_12MHZ, CALBC1_12MHZ_);
#define CALDCO_8MHZ_          0x10FC    /* DCOCTL  Calibration Data for 8MHz */
const_sfrb(CALDCO_8MHZ, CALDCO_8MHZ_);
#define CALBC1_8MHZ_          0x10FD    /* BCSCTL1 Calibration Data for 8MHz */
const_sfrb(CALBC1_8MHZ, CALBC1_8MHZ_);

typedef struct
{
  uint8_t digit_1;
  uint8_t digit_2;
  uint8_t digit_3;
  uint8_t negative;
} temperature;

temperature read_internal_temperature(void);
int read_adc_A1(void);
int read_internal_temp(void);

#endif /* MAIN_H_ */
