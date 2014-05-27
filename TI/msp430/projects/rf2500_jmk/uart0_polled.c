#include <io.h>
#include "uart0_polled.h"

void uart0_polled_write( const char* pstr, int length )
{
	for( ; length>0; --length, ++pstr ) {
		UCA0TXBUF = pstr[0];
		while (!(IFG2&UCA0TXIFG));
	}
}

void uart0_polled_puts( const char* pstr )
{
	while (*pstr) {
		if (*pstr=='\n') {
			UCA0TXBUF = '\n';
			while (!(IFG2&UCA0TXIFG));
			UCA0TXBUF = '\r';
			++pstr;
		} else
			UCA0TXBUF = *pstr++;
		while (!(IFG2&UCA0TXIFG));
	}
}

void uart0_polled_putc( char c )
{
	UCA0TXBUF = c;
	while (!(IFG2&UCA0TXIFG));
}

void uart0_polled_putcrlf()
{
	uart0_polled_putc('\n');
	uart0_polled_putc('\r');
}

void uart0_polled_putHex8( uint8_t value )
{
	static const char hex[] = "0123456789ABCDEF";
	uart0_polled_putc( hex[(value>>4) & 0x0f] );
	uart0_polled_putc( hex[value & 0x0f] );
}

void uart0_polled_hexdump( const char* ptr, int len )
{
#define	DEBUGDUMPLINELENGTH	16
	uint8_t offs = 0;
	while(len>0) {
		uart0_polled_putHex8( offs );
		uart0_polled_puts(": ");
		int llen = (len>DEBUGDUMPLINELENGTH) ? DEBUGDUMPLINELENGTH : len;
		const char* lptr = ptr;
		for( ; llen>0; --llen, ++lptr ) {
			uart0_polled_putHex8( *lptr );
			uart0_polled_putc( ' ' );
		}
		llen = (len<DEBUGDUMPLINELENGTH) ? DEBUGDUMPLINELENGTH-len : 0;
		for( ; llen>0; --llen )
			uart0_polled_puts("   ");
		llen = (len>DEBUGDUMPLINELENGTH) ? DEBUGDUMPLINELENGTH : len;
		for( ; llen>0; --llen, --len, ++ptr ) {
			char c = *ptr;
			uart0_polled_putc( (c>=' ' && c<127) ? c : '.' );
		}
		uart0_polled_putcrlf();
		offs += DEBUGDUMPLINELENGTH;
	}
}
