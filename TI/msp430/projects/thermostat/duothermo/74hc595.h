#ifndef _74HC595_H_
#define _74HC595_H_

#include <sys/types.h>

#define SHIFT_STORAGE_CLEAR 0
#define SHIFT_STORAGE_CLOCK 1

void shift_setup(volatile uint16_t *PORT_DIR_REG, volatile uint16_t *PORT_OUT_REG,
    uint8_t PIN_SCK, uint8_t PIN_RCK, uint8_t PIN_SCLR, uint8_t PIN_SI);
void shift_clear();
void shift_byte(uint8_t byte);
void shift_end();

#endif /* _74HC595_H_ */
