# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign

DEFAULT_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src \
		-I$(top_srcdir)/src/common

noinst_LIBRARIES = libavi.a

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libavi_a_SOURCES = avilib.c avidump.c avimisc.c

EXTRA_DIST = avilib.h README.avilib
