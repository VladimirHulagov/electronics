;; bluetemp.asm
;;
;; Copyright 2011 - By Michael Kohn
;; http://www.mikekohn.net/
;; mike to mikekohn.net
;;
;; Read temperature from DS18B20 and send over UART to a bluetooth adapter
;; with SPP protocol.  The format is 0xff and then 2 bytes from the DS18B20

.include "msp430x2xx.inc"

RAM equ 0x0200
DS18B20_DATA equ 0x0210
DS18B20_CMD equ 0x0220
DS18B20_CMD_LEN equ 0x0221
DS18B20_BIT equ 0x0222
DEBUG_STATE equ 0x0223

;  r4 =
;  r5 =
;  r6 =
;  r7 =
;  r8 =
;  r9 =
; r10 =
; r11 =
; r12 =
; r13 =
; r14 =
; r15 =

  .org 0xc000
start:
  ;; Turn off watchdog
  mov.w #(WDTPW|WDTHOLD), &WDTCTL

  ;; Please don't interrupt me
  dint

  ;; r13 points to which interrupt routine should be called
  mov.w #null_interrupt, r13

  ;; Setup stack pointer
  mov.w #0x0400, SP

  ;; Set MCLK to 16 MHz with DCO 
  mov.b #DCO_4, &DCOCTL
  mov.b #RSEL_15, &BCSCTL1
  mov.b #0, &BCSCTL2

  ;; Setup output pins
  ;; P1.2 = Debug LED
  mov.b #1, &P1DIR        ; P1.0
  mov.b #1, &P1OUT
  mov.b #6, &P1SEL
  mov.b #6, &P1SEL2

  ;; Setup UART
  mov.b #UCSSEL_2|UCSWRST, &UCA0CTL1
  mov.b #0, &UCA0CTL0
  ;mov.b #0x82, &UCA0BR0
  ;mov.b #0x06, &UCA0BR1
  mov.b #0x8a, &UCA0BR0
  mov.b #0x00, &UCA0BR1
  bic.b #UCSWRST, &UCA0CTL1

  ;; Set up Timer
  mov.w #2000, &TACCR0
  mov.w #(TASSEL_2|MC_1), &TACTL ; SMCLK, DIV1, COUNT to TACCR0
  mov.w #CCIE, &TACCTL0
  mov.w #0, &TACCTL1

  ;; Okay, I can be interrupted now
  eint

  ;call #read_temp

  mov #0, r9
  ;mov.b #'A', &UCA0TXBUF

main:
  call #read_temp

  ;call wait_transmit
wait_ff1:
  bit.b #UCA0TXIFG, &IFG2
  jz wait_ff1
  mov.b #0xff, &UCA0TXBUF

  ;call wait_transmit
wait_ff2:
  bit.b #UCA0TXIFG, &IFG2
  jz wait_ff2
  mov.b #0xff, &UCA0TXBUF

  ;call wait_transmit
wait_1:
  bit.b #UCA0TXIFG, &IFG2
  jz wait_1
  mov.b &DS18B20_DATA, &UCA0TXBUF

  ;call wait_transmit
wait_2:
  bit.b #UCA0TXIFG, &IFG2
  jz wait_2
  mov.b &DS18B20_DATA+1, &UCA0TXBUF

  mov.w #61, r7
repeat_outer:
  mov.w #0, r8
repeat_inner:
  dec.w r8
  jne repeat_inner
  dec.w r7
  jne repeat_outer

  inc.w r9

  jmp main

wait_transmit:
  bit.b #UCA0TXIFG, &IFG2
  jz wait_transmit
  ret

read_temp:
  mov.w #ds18b20_signal_interrupt, r13
  ;; First do a convert
  call #init_ds18b20
  mov.b #0xcc, &DS18B20_CMD
  call #send_ds18b20_cmd
  mov.b #0x44, &DS18B20_CMD
  call #send_ds18b20_cmd
  ;;mov.w #0, r4  ;; DEBUG
  call #wait_ds18b20_convert

  ;; Second fetch the scratch-pad
  call #init_ds18b20
  mov.b #0xcc, &DS18B20_CMD
  call #send_ds18b20_cmd
  mov.b #0xbe, &DS18B20_CMD
  call #send_ds18b20_cmd
  call #fetch_ds18b20_scratch_pad

  mov.w #null_interrupt, r13
  ret

;; init DS18B20
init_ds18b20:
  ;mov.w #0, r4
  ;mov.w #0, r5
  bic.b #1, P1OUT
  mov.w #7680, &TACCR0   ; 480us
  mov.w #0, TAR
  mov.w #0, r11
master_reset_pulse:
  cmp.w #1, r11
  jne master_reset_pulse
  bis.b #1, P1OUT
  bic.b #1, P1DIR
wait_slave_low:
  ;inc r4
  bit.b #1, P1IN
  jnz wait_slave_low
wait_slave_hi:
  ;inc r5
  bit.b #1, P1IN
  jz wait_slave_hi
  bis.b #1, P1DIR
  mov.w #960, &TACCR0   ; 60us
  ret

;; send_ds18b20_cmd
send_ds18b20_cmd:
  mov.b #8, &DS18B20_CMD_LEN
send_next_bit:
  bit.b #1, &DS18B20_CMD
  jz cmd_bit_0
  call #write_ds18b20_1
  jmp cmd_bit_done
cmd_bit_0:
  call #write_ds18b20_0
cmd_bit_done:
  mov.w #6, r10
cmd_wait_1us:
  dec r10
  jnz cmd_wait_1us
  rra.b &DS18B20_CMD
  dec.b &DS18B20_CMD_LEN
  jnz send_next_bit
  ret

;; wait_ds18b20_convert
wait_ds18b20_convert:
  mov.w #6, r10
convert_wait_1us:
  dec r10
  jnz convert_wait_1us
  call #read_ds18b20
  cmp.w #0, r10
  jeq wait_ds18b20_convert
  ret

;; fetch_ds18b20_scratch_pad
fetch_ds18b20_scratch_pad:
  mov.w #DS18B20_DATA, r14
fetch_next_byte:
  mov.w #8, &DS18B20_BIT

fetch_next_bit:
  mov.w #6, r10
scratch_wait_1us:
  dec r10
  jnz scratch_wait_1us

  call #read_ds18b20
  rra.b 0(r14)
  cmp.w #0, r10
  jeq fetched_0
  bis.b #128, 0(r14)
  jmp done_set_fetch
fetched_0:
  bic.b #128, 0(r14)
done_set_fetch:
  dec &DS18B20_BIT
  jnz fetch_next_bit

  inc r14
  cmp.w #DS18B20_DATA+9, r14
  jne fetch_next_byte
  ret

;; write 1 to DS18B20
write_ds18b20_1:
  bic.b #1, P1OUT
  mov.w #0, TAR
  mov.w #0, r11
  mov.w #6, r10
wait_at_least_1us_write_1:
  dec r10
  jnz wait_at_least_1us_write_1
  bis.b #1, P1OUT
master_write_1:
  cmp.w #1, r11
  jne master_write_1
  ret

;; write 0 to DS18B20
write_ds18b20_0:
  bic.b #1, P1OUT
  mov.w #0, TAR
  mov.w #0, r11
master_write_0:
  cmp.w #1, r11
  jne master_write_0
  bis.b #1, P1OUT
  ret

;; read from DS18B20 (returns 0 in r10 for a 0, return 1 for a 1)
read_ds18b20:
  bic.b #1, P1OUT
  mov.w #6, r10
wait_at_least_1us_read:
  dec r10
  jnz wait_at_least_1us_read
  bis.b #1, P1OUT
  bic.b #1, P1DIR
  mov.w #0, TAR
  mov.w #0, r11
wait_read_bit:
  bit.w #1, P1IN
  jnz read_bit_not_zero
  inc r10
read_bit_not_zero:
  cmp.w #1, r11
  jne wait_read_bit
  bis.b #1, P1DIR

  cmp.w #0, r10
  jeq return_1
  mov.w #0, r10
  ret
return_1:
  mov.w #1, r10
  ret

timer_interrupt:
  br r13

null_interrupt:
  reti

ds18b20_signal_interrupt:
  mov.w #1, r11
  reti

  org 0xffe8
vectors:
  dw 0
  dw 0
  dw 0
  dw 0
  dw 0
  dw timer_interrupt       ; Timer_A2 TACCR0, CCIFG
  dw 0
  dw 0
  dw 0
  dw 0
  dw 0
  dw start                 ; Reset



