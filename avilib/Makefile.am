# # Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.3 foreign
DEFAULT_INCLUDES = -I$(top_srcdir) -I$(srcdir) -I$(top_builddir) -I.

noinst_LIBRARIES = libavi.a

INCLUDES = -I./

CFLAGS=-O3 -funroll-loops -ffast-math -DLINUX -Wall @CFLAGS@
libavi_a_SOURCES = avilib.c avidump.c avimisc.c

EXTRA_DIST = avilib.h README.avilib



