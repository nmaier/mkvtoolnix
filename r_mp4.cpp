/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp4.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_mp4.cpp,v 1.2 2003/05/22 16:14:29 mosu Exp $
    \brief MP4 identification class
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r_mp4.h"

int mp4_reader_c::probe_file(FILE *file, int64_t size) {
  unsigned char data[20];

  if (size < 20)
    return 0;
  if (fseeko(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(data, 1, 20, file) != 20) {
    fseeko(file, 0, SEEK_SET);
    return 0;
  }
  fseeko(file, 0, SEEK_SET);
  if ((data[4] != 'f') || (data[5] != 't') ||
      (data[6] != 'y') || (data[7] != 'p'))
    return 0;
  return 1;
}
