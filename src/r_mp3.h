/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the MP3 reader module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_MP3_H
#define __R_MP3_H

#include "os.h"

#include <stdio.h>

#include "mm_io.h"
#include "common.h"
#include "error.h"

#include "p_mp3.h"

class mp3_reader_c: public generic_reader_c {
private:
  unsigned char *chunk;
  mm_io_c *mm_io;
  class mp3_packetizer_c *mp3packetizer;
  int64_t bytes_processed, size;

public:
  mp3_reader_c(track_info_t *nti) throw (error_c);
  virtual ~mp3_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void set_headers();
  virtual void identify();

  virtual int display_priority();
  virtual void display_progress(bool final = false);

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif  // __R_MP3_H
