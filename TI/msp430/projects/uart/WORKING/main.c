#include <msp430g2553.h>
#include <string.h>

/* Demo UART application.  After you press the button, this transmits
 * a "Hello, world!" string to the computer at 2400 bps.
 */

#define   RED_LED   BIT0
#define   GRN_LED   BIT6

#define   BUTTON    BIT3

#define   TXD       BIT1
#define   RXD       BIT2

/* Ticks per bit.  Use the following values based on speed:
 * 9600 bps ->  13
 * 2400 bps ->  52
 * 1200 bps -> 104
 * I did not have success with slower speeds, like 300 bps.
 */
#define   TPB      13

/* A pointer to the data to send, and a counter of the bytes. */
unsigned char *data;
unsigned int bytestosend = 0;

/* The actual byte we are transmitting, with its start and stop bits,
 * and a counter of the bits left to send.
 */
int TXWord;
unsigned char bitcnt = 0;

/* function prototypes */
void initUart( void );
int sendByte( unsigned char b );
int sendBytes( const unsigned char *d, int len );
int sendString( const char *str );

void main(void) {
    /* stop the watchdog timer */
    WDTCTL = WDTPW + WDTHOLD;

    /* For debouncing: set the auxilliary clock to use very low-power
     * oscillator.  Later, we'll have the Watchdog timer use the
     * auxilliary clock for debouncing the button.
     */
    BCSCTL3 |= LFXT1S_2;

    /* LEDs off, but later we'll blink them as we send bits */
    P1DIR |= RED_LED+GRN_LED;
    P1OUT &= ~ (RED_LED + GRN_LED );

    initUart();

    /* We'll use the button to let the chip know we're ready to communicate.
     * Direction is receive, clear interrupt flag, and
     * interrupts are enabled.
     */
    P1DIR &= ~BUTTON;
    P1IFG &= ~BUTTON;
    P1IE |= BUTTON;

    for( ; ; ) {
        /* go to sleep until button press */
        __bis_SR_register( LPM3_bits + GIE );
        sendString( "Hello, world!\r\n" );
    }
}

void initUart( void ) {
    /* Set up transmit as output pin and set it high */
    P1OUT |= TXD;
    P1DIR |= TXD;

    /* set up the clocks for 1 mhz */
    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL = CALDCO_1MHZ;
    BCSCTL2 &= ~(DIVS_3);

    /* Set timer A to use count up mode 1 mhz / 8 = 125 khz. */
    TACTL = TASSEL_2 + MC_1 + ID_3 + TACLR;

    /* Set ticks-per-bit to specify communication speed */
    TACCR0 = TPB;
}

/* Prepares a block of data to be sent. Returns number of bytes sent. */
int sendBytes( const unsigned char *d, int len ) {
    /* can't queue up data if we're still sending */
    if( bytestosend > 0 ) return 0;

    bitcnt = 0;
    data = d;
    bytestosend = len;

    /* clear interrupt flag, and tell Timer A0 to
     * start triggering interrupts
     */
    TACCTL0 &= ~CCIFG;
    TACCTL0 |= CCIE;

    /* sleep until message sent */
    while( TACCTL0 & CCIE ) {
        __bis_SR_register( LPM0_bits + GIE );
    }

    return len;
}

/* Sends a single byte to the computer.  Returns number of bytes sent. */
int sendByte( unsigned char b ) {
    return sendBytes( &b, 1 );
}

/* Sends a string to the computer.  Returns number of bytes sent. */
int sendString( const char *str ) {
    return sendBytes( str, strlen(str) );
}

/* This continuously sends bits of the TXWord starting from the
 * least significant bit (the 0 start bit).  One bit is sent every
 * time the handler is activated.  When the bits run out, a new
 * byte is loaded from the data pointer, until bytestosend equals 0.
 */
void TimerA0 (void) __attribute__((interrupt(TIMER0_A0_VECTOR)));
void TimerA0(void) {
    TACCTL0 &= ~CCIFG;

    /* if no bits to send, either load a byte or return */
    if( ! bitcnt ) {
        if( bytestosend > 0 ) {
            /* load the byte */
            TXWord = *data++;
            /* add stop bit */
            TXWord |= 0x100;
            /* add start bit */
            TXWord <<= 1;

            /* 1 start bit + 8 data bits + 1 stop bit */
            bitcnt = 10;

            bytestosend --;
        } else {
            /* no bits left, turn off interrupts and wake up */
            TACCTL0 &= ~ CCIE;
            __bic_SR_register_on_exit( LPM0_bits );
            return;
        }
    }

    /* send least significant bit */
    if( TXWord & 0x01 ) {
        P1OUT |= TXD;
        P1OUT |= RED_LED;               // for testing
        P1OUT &= ~ GRN_LED;             // for testing
    } else {
        P1OUT &= ~TXD;
        P1OUT |= GRN_LED;               // for testing
        P1OUT &= ~ RED_LED;             // for testing
    }

    /* shift word to remove one bit */
    TXWord >>= 1;
    bitcnt --;
}

/* A button press triggers this interrupt, which wakes
 * up the main program to send a message.
 */
void Port_1 (void) __attribute__((interrupt(PORT1_VECTOR)));
void Port_1(void) {
    /* disable interrupts for the button to handle button bounce */
    P1IE &= ~BUTTON;
    /* clear the interrupt flag for button */
    P1IFG &= ~BUTTON;

    /* set watchdog timer to trigger every 16*32.768k/12k = 44 ms */
    WDTCTL = WDT_ADLY_16;
    /* clear watchdog timer interrupt flag */
    IFG1 &= ~WDTIFG;
    /* enable watchdog timer interrupts; in 44 ms the button
     * will be re-enabled by WDT_ISR() -- program will continue in
     * the meantime.
     */
    IE1 |= WDTIE;

    /* wake up the main program */
     __bic_SR_register_on_exit( LPM3_bits );
}

/* This function catches watchdog timer interrupts, which are
 * set to happen 44ms after the user presses the button.  The
 * button has had time to "bounce" and we can turn the button
 * interrupts back on.
 */
void WDT_ISR(void) __attribute__((interrupt(WDT_VECTOR)));
void WDT_ISR(void)
{
    /* Disable interrupts on the watchdog timer */
    IE1 &= ~WDTIE;
    /* clear the interrupt flag for watchdog timer */
    IFG1 &= ~WDTIFG;
    /* resume holding the watchdog timer so it doesn't reset the chip */
    WDTCTL = WDTPW + WDTHOLD;
    /* and re-enable interrupts for the button */
    P1IE |= BUTTON;
}
