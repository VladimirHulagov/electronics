#include "main.h"


/** Delay function. **/
void delay(unsigned int d) {
  int i;
  for (i = 0; i<d; i++) {
    nop();
  }
}

int main(void) {
  WDTCTL = WDTPW | WDTHOLD;
  P1DIR = BIT6 | BIT0;
  P1OUT = BIT6 | BIT0;

  if (CALBC1_16MHZ ==0xFF || CALDCO_16MHZ == 0xFF) {
	  P1OUT = 0b00000001;
	  while(1); // trap
  }

  BCSCTL1 = CALBC1_16MHZ;
  DCOCTL = CALDCO_16MHZ;
  BCSCTL2 |= SELM_0 + DIVM_0;

  //shift_setup((volatile uint16_t *)&P1DIR, (volatile uint16_t *)&P1OUT, BIT0, BIT1, BIT2, BIT3);

  while (1) {
    /*shift_clear();
    temperature internal_sensor = read_internal_temperature();
    shift_byte(internal_sensor.digit_1);
    shift_byte(internal_sensor.digit_2);
    shift_byte(internal_sensor.digit_3);
    shift_end();*/
    read_adc_A1();
    read_internal_temp();
    P1OUT = ~P1OUT;
    delay(10000);
  }
}

int read_adc_A1(void)
{
  ADC10CTL0 = ADC10ON | SREF_0 | ADC10SHT_3;
  ADC10CTL1 = INCH_1 | ADC10DIV_7 | ADC10SSEL_2;
  ADC10AE0 = BIT1;
  int avg = 0;
  uint8_t i = 0;
  for (i=0; i<16; i++) {
    ADC10CTL0 |= ENC | ADC10SC;
    while (ADC10CTL1 & ADC10BUSY) {};
    avg += ADC10MEM;
  }
  ADC10CTL0 &= ~(ENC | ADC10ON);
  long tmp = avg / 16;
  // (((A * 3.56 * 100) / 1024) - 273) * 10 => (((A - 785) * 356) * 10) / 1024
  tmp = (((tmp - 785) * 356) * 10) / 1024;
  int raw_temp = tmp;

  return raw_temp;
}

int read_internal_temp(void)
{
  ADC10CTL0 = ADC10ON | ADC10SHT_3;
  ADC10CTL1 = INCH_10 | ADC10DIV_0 | ADC10SSEL_2;
  ADC10CTL0 |= REFON | SREF_1;
  delay(500);
  int avg = 0;
  uint8_t i = 0;
  for (i=0; i<16; i++) {
    ADC10CTL0 |= ENC | ADC10SC;
    while (ADC10CTL1 & ADC10BUSY) {};
    avg += ADC10MEM;
  }
  ADC10CTL0 &= ~(ENC | REFON | ADC10ON);
  long tmp = avg / 16;
  tmp = (((tmp - 673) * 423) * 10) / 1024;
  int raw_temp = tmp;


  return raw_temp;
}

temperature read_internal_temperature(void)
{
  temperature temp_sensor;

  ADC10CTL1 |= INCH_10 | ADC10DIV_7 | ADC10SSEL_2;
  ADC10CTL0 |= REFON | ADC10ON | SREF_1 | ADC10SHT_3;// | REF2_5V;
  delay(480);
  ADC10CTL0 |= ENC | ADC10SC;
  delay(48);
  while (ADC10CTL1 & ADC10BUSY) {};
  ADC10CTL0 &= ~ENC;
  ADC10CTL0 &= ~(REFON | ADC10ON);

  // (raw * 0.0024414 - 0.986) / 0.00355
  unsigned long temp_tmp;
  int temp_sensor_int;
  div_t temp_sensor_main_digits;

  //temp_tmp = (ADC10MEM - 404) * 11;
  temp_tmp = (ADC10MEM - 673) * 423;
  //temp_sensor_int = temp_tmp / 16;
  temp_sensor_int = temp_tmp / 1024;
  temp_sensor.negative = 0;
  if (temp_sensor_int < 0) {
    temp_sensor.negative = 1;
  }
  temp_sensor_main_digits = div(temp_sensor_int, 10);
  //temp_tmp = temp_tmp * 10 / 16;
  temp_tmp = temp_tmp * 10 / 1024;
  temp_sensor.digit_3 = div(temp_tmp, 10).rem;
  temp_sensor.digit_1 = temp_sensor_main_digits.quot;
  temp_sensor.digit_2 = temp_sensor_main_digits.rem;

  return temp_sensor;
}

//temperature convert_to_3_digits(int temperature)


