/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_VOBSUB_H
#define __XTR_VOBSUB_H

#include "os.h"

#include "smart_pointers.h"

#include "xtr_base.h"

class xtr_vobsub_c: public xtr_base_c {
public:
  vector<int64_t> positions, timecodes;
  vector<xtr_vobsub_c *> slaves;
  autofree_ptr<unsigned char> private_data;
  uint32_t private_size;
  string base_name, language;
  int stream_id;

public:
  xtr_vobsub_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_block(KaxBlock &block, KaxBlockAdditions *additions,
                            int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref);
  virtual void finish_file();
  virtual void write_idx(mm_io_c &idx, int index);
};

#endif
