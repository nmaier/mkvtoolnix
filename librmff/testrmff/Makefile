# User settings
CC = gcc
AR = ar
RANLIB = ranlib
INSTALL = install
CFLAGS = -Wall -Wshadow -g
INCLUDES = -I..
LDFLAGS = -L..

# Used for 'make install'. Change/overwrite as needed.
prefix = /usr/local
libdir = $(prefix)/lib
includedir = $(prefix)/include

# Don't change these settings
SOURCES = testrmff.c
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

CFLAGS += -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

all:
	make -C .. all
	make testrmff

clean:
	rm -f $(OBJECTS) testrmff

%.o: %.c ../librmff.h
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDES) $<

testrmff: $(OBJECTS) ../librmff.a
	$(CC) -o $@ $(LDFLAGS) $(OBJECTS) -lrmff
