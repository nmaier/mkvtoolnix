# User settings
CC = gcc
AR = ar
RANLIB = ranlib
INSTALL = install
CFLAGS = -Wall -Wshadow -g
INCLUDES = -I.

# Used for 'make install'. Change/overwrite as needed.
prefix = /usr/local
libdir = $(prefix)/lib
includedir = $(prefix)/include

# Don't change these settings
SOURCES = rmff.c mb_file_io.c
HEADERS = librmff.h mb_file_io.h
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

CFLAGS += -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

all: librmff.a

clean:
	rm -f $(OBJECTS) librmff.a

%.o: %.c librmff.h
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDES) $<

librmff.a: $(OBJECTS)
	$(AR) rcvu $@ $?
	$(RANLIB) $@

install: all
	@if ! test -d $(includedir); then \
		echo $(INSTALL) -d $(includedir); \
		$(INSTALL) -d $(includedir); \
	fi
	@for i in $(HEADERS); do \
		echo $(INSTALL) $$i $(includedir); \
		$(INSTALL) $$i $(includedir); \
	done
	@if ! test -d $(libdir); then \
		echo $(INSTALL) -d $(libdir); \
		$(INSTALL) -d $(libdir); \
	fi
	$(INSTALL) librmff.a $(libdir)
