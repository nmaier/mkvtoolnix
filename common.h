/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: common.h,v 1.21 2003/05/11 15:52:54 mosu Exp $
    \brief definitions used in all programs, helper functions
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __COMMON_H
#define __COMMON_H

#ifndef __CYGWIN__
#include <stdint.h>
#elif defined WIN32
#include <stdint.h>
#define PACKAGE "mkvtoolnix"
#define VERSION 0.3.2
#endif
#include <sys/types.h>

#ifdef WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define nice(a)
#endif

#include "config.h"

#define VERSIONINFO "mkvmerge v" VERSION

#define DISPLAYPRIORITY_HIGH   10
#define DISPLAYPRIORITY_MEDIUM  5
#define DISPLAYPRIORITY_LOW     1

/* errors */
#define EMOREDATA    -1
#define EMALLOC      -2
#define EBADHEADER   -3
#define EBADEVENT    -4
#define EOTHER       -5

/* types */
#define TYPEUNKNOWN   0
#define TYPEOGM       1
#define TYPEAVI       2
#define TYPEWAV       3 
#define TYPESRT       4
#define TYPEMP3       5
#define TYPEAC3       6
#define TYPECHAPTERS  7
#define TYPEMICRODVD  8
#define TYPEVOBSUB    9
#define TYPEMATROSKA 10

#define FOURCC(a, b, c, d) (unsigned long)((((unsigned char)a) << 24) + \
                           (((unsigned char)b) << 16) + \
                           (((unsigned char)c) << 8) + \
                           ((unsigned char)d))

#define TIMECODE_SCALE 1000000

#define die(s) _die(s, __FILE__, __LINE__)
void _die(const char *s, const char *file, int line);

#define trace() _trace(__func__, __FILE__, __LINE__)
void _trace(const char *func, const char *file, int line);

uint16_t get_uint16(const void *buf);
uint32_t get_uint32(const void *buf);
uint64_t get_uint64(const void *buf);

extern int cc_local_utf8;

int utf8_init(char *charset);
void utf8_done();
char *to_utf8(int handle, char *local);
char *from_utf8(int handle, char *utf8);

int is_unique_uint32(uint32_t number);
void add_unique_uint32(uint32_t number);
uint32_t create_unique_uint32();

#define safefree(p) if ((p) != NULL) free(p);
#define safemalloc(s) _safemalloc(s, __FILE__, __LINE__)
void *_safemalloc(size_t size, const char *file, int line);
#define safestrdup(s) _safestrdup(s, __FILE__, __LINE__)
char *_safestrdup(const char *s, const char *file, int line);
unsigned char *_safestrdup(const unsigned char *s, const char *file, int line);
#define safememdup(src, size) _safememdup(src, size, __FILE__, __LINE__)
void *_safememdup(const void *src, size_t size, const char *file, int line);
#define saferealloc(mem, size) _saferealloc(mem, size, __FILE__, __LINE__)
void *_saferealloc(void *mem, size_t size, const char *file, int line);

extern int verbose;

#endif // __COMMON_H
