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
    \version $Id$
    \brief MP4 identification class
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r_mp4.h"

using namespace std;

int mp4_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  unsigned char data[20];

  if (size < 20)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(data, 20) != 20)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if ((data[4] != 'f') || (data[5] != 't') ||
      (data[6] != 'y') || (data[7] != 'p'))
    return 0;
  return 1;
}
