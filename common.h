/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.h
  helper functions

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#define die(s) _die(s, __FILE__, __LINE__)
void _die(const char *s, const char *file, int line);

extern int verbose;

#endif // __COMMON_H__
