/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the Subripper subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_SRT_H
#define __R_SRT_H

#include "os.h"

#include <stdio.h>

#include "mm_io.h"
#include "common.h"
#include "pr_generic.h"
#include "subtitles.h"

class srt_reader_c: public generic_reader_c {
private:
  mm_text_io_c *mm_io;
  subtitles_c subs;

public:
  srt_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~srt_reader_c();

  virtual void parse_file();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_text_io_c *mm_io, int64_t size);
};

#endif  // __R_SRT_H
