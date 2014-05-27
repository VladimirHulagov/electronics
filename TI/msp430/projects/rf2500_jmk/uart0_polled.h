extern	void uart0_polled_write( const char* pstr, int length );
extern	void uart0_polled_puts( const char* pstr );
extern	void uart0_polled_putc( char c );
extern	void uart0_polled_putcrlf();
extern	void uart0_polled_putHex8( uint8_t value );
extern	void uart0_polled_hexdump( const char* ptr, int len );
