/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * definition of global variables found in mkvmerge.cpp
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "pr_generic.h"

#define DISPLAYPRIORITY_HIGH   10
#define DISPLAYPRIORITY_MEDIUM  5
#define DISPLAYPRIORITY_LOW     1

/* file types */
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
#define TYPEDTS      11
#define TYPEAAC      12
#define TYPESSA      13
#define TYPEREAL     14
#define TYPEQTMP4    15
#define TYPEFLAC     16
#define TYPETTA      17
#define TYPEMPEG     18
#define TYPEMAX      18

int64_t create_track_number(generic_reader_c *reader, int64_t tid);

#endif // __MKVMERGE_H
