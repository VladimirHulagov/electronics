#include "74hc595.h"

uint8_t S_PIN_SCK = 0;
uint8_t S_PIN_RCK = 0;
uint8_t S_PIN_SCLR = 0;
uint8_t S_PIN_SI = 0;

volatile uint16_t *S_PORT_DIR_REG = 0;
volatile uint16_t *S_PORT_OUT_REG = 0;

void shift_setup(volatile uint16_t *PORT_DIR_REG, volatile uint16_t *PORT_OUT_REG,
    uint8_t PIN_SCK, uint8_t PIN_RCK, uint8_t PIN_SCLR, uint8_t PIN_SI)
{
  S_PORT_DIR_REG = PORT_DIR_REG;
  S_PORT_OUT_REG = PORT_OUT_REG;
  S_PIN_SCK = PIN_SCK;
  S_PIN_RCK = PIN_RCK;
  S_PIN_SCLR = PIN_SCLR;
  S_PIN_SI = PIN_SI;

  // init port pins direction and default values
  *S_PORT_DIR_REG |= S_PIN_SCK | S_PIN_RCK | S_PIN_SCLR | S_PIN_SI;
  *S_PORT_OUT_REG &= ~(S_PIN_RCK | S_PIN_SCK | S_PIN_SI);
}

void shift_storage(uint8_t todo)
{
  if (todo == SHIFT_STORAGE_CLEAR) {
    // put rck lo
    *S_PORT_OUT_REG &= ~S_PIN_RCK;
  } else {
    // clock rck line
    *S_PORT_OUT_REG |= S_PIN_RCK;
    *S_PORT_OUT_REG &= ~S_PIN_RCK;
  }
}

void shift_clear()
{
  *S_PORT_OUT_REG &= ~S_PIN_SCLR;
  *S_PORT_OUT_REG |= S_PIN_SCLR;
}

void shift_clock_cycle()
{
  *S_PORT_OUT_REG |= S_PIN_SCK;
  *S_PORT_OUT_REG &= ~S_PIN_SCK;
}

void shift_byte(uint8_t byte)
{
  uint8_t cur_bit;
  uint8_t bit;

  for (bit = 0; bit < 8; bit++) {
    cur_bit = (byte >> bit) & 1;
    if (cur_bit) {
      // if bit set then put si hi
      *S_PORT_OUT_REG |= S_PIN_SI;
    } else {
      // put si lo
      *S_PORT_OUT_REG &= ~S_PIN_SI;
    }
    shift_clock_cycle();
  }
}

void shift_end()
{
  shift_storage(SHIFT_STORAGE_CLOCK);
}
