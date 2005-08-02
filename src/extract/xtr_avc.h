/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#ifndef __XTR_AVC_H
#define __XTR_AVC_H

#include "os.h"

#include "xtr_base.h"

class xtr_avc_c: public xtr_base_c {
public:
  int nal_size_size;

public:
  xtr_avc_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_block(KaxBlock &block, KaxBlockAdditions *additions,
                            int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref);
  void write_nal(const binary *data, int &pos, int data_size,
                 int nal_size_size);
};

#endif
