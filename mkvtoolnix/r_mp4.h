/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp4.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_mp4.h,v 1.2 2003/05/23 06:34:57 mosu Exp $
    \brief class definitions for the dummy MP4 reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_MP4_H
#define __R_MP4_H

#include <stdio.h>

#include "mm_io.h"

class mp4_reader_c {
public:
  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif  // __R_MP4_H
