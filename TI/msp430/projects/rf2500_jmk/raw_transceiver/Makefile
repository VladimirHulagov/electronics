CCPREFIX	= msp430-
CC	= $(CCPREFIX)gcc
CPU	= msp430x2274
CFLAGS	= -g -W -Wall -O2 -mmcu=$(CPU) -I.. -MD $(addprefix -D,$(DEFINE))
LDFLAGS	= -g -mmcu=$(CPU)

BASENAME	= raw_transceiver

# Tell make to also search for source files in ../ (used for common files)
VPATH	+= ..

OBJS	= main.o hal_spi_radio.o hal_radio.o uart0_polled.o

$(BASENAME).elf	: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	$(CCPREFIX)size $@

clean:
	rm -f $(OBJS:.o=.d) $(OBJS) $(BASENAME).elf

include ../make.debug
-include $(wildcard *.d)
