/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: common.cpp,v 1.2 2003/02/16 12:17:10 mosu Exp $
    \brief helper functions, common variables
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>

int verbose = 0;

void _die(const char *s, const char *file, int line) {
  fprintf(stderr, "die @ %s/%d : %s\n", file, line, s);
  exit(1);
}
