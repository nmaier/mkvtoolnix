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
    \version \$Id: common.h,v 1.3 2003/02/16 12:17:10 mosu Exp $
    \brief definitions used in all programs, helper functions
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#define VERSION "0.0.1"
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

#define FOURCC(a, b, c, d) (unsigned long)((((unsigned char)a) << 24) + \
                           (((unsigned char)b) << 16) + \
                           (((unsigned char)c) << 8) + \
                           ((unsigned char)d))
typedef struct {
  int    displacement;
  double linear;
} audio_sync_t;

typedef struct {
  double start;
  double end;
} range_t;

typedef double stamp_t;
#define MAX_TIMESTAMP ((double)3.40282347e+38F)

#define die(s) _die(s, __FILE__, __LINE__)
void _die(const char *s, const char *file, int line);

extern int verbose;

#endif // __COMMON_H__
