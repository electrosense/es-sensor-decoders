#CFLAGS?=-O2 -g -Wall -W 
CFLAGS?=-Og -g -Wall -W 
CFLAGS+= -I./aisdecoder -I ./aisdecoder/lib -I./tcp_listener -I./aisparser
LDFLAGS+=-lpthread -lm

UNAME := $(shell uname)

CC?=gcc
SOURCES= \
	main.c rtl_ais.c convenience.c \
	./aisdecoder/aisdecoder.c \
	./aisdecoder/sounddecoder.c \
	./aisdecoder/lib/receiver.c \
	./aisdecoder/lib/protodec.c \
	./aisdecoder/lib/hmalloc.c \
	./aisdecoder/lib/filter.c \
	./tcp_listener/tcp_listener.c \
	./ringbuf.c \
	./aisparser/nmea.c \
        ./aisparser/vdm_parse.c \
	./aisparser/sixbit.c

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=ais_decoder

all: $(SOURCES) $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(EXECUTABLE).exe
