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
  unsigned char chunk[16384];
  mm_io_c *mm_io;
  int64_t bytes_processed, size;
  mp3_header_t mp3header;

public:
  mp3_reader_c(track_info_c *nti) throw (error_c);
  virtual ~mp3_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  virtual int display_priority();
  virtual void display_progress(bool final = false);

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  static int find_valid_headers(mm_io_c *mm_io);
};

#endif  // __R_MP3_H
