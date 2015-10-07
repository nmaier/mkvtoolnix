/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   math helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MATH_H
#define MTX_COMMON_MATH_H

#include "common/common_pch.h"

#define irnd(a) ((int64_t)((double)(a) + 0.5))
#define iabs(a) ((a) < 0 ? (a) * -1 : (a))

uint32_t round_to_nearest_pow2(uint32_t value);

int int_log2(uint32_t value);
double int_to_double(int64_t value);

typedef boost::rational<int64_t> int64_rational_c;

#endif  // MTX_COMMON_MATH_H
