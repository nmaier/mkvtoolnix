/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.cpp
  helper functions

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#include <stdio.h>
#include <stdlib.h>

int verbose = 0;

void _die(const char *s, const char *file, int line) {
  fprintf(stderr, "die @ %s/%d : %s\n", file, line, s);
  exit(1);
}
