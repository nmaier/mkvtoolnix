/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_RMFF_H
#define __XTR_RMFF_H

#include "os.h"

#include "librmff.h"

#include "xtr_base.h"

class xtr_rmff_c: public xtr_base_c {
public:
  rmff_file_t *file;
  rmff_track_t *rmtrack;

public:
  xtr_rmff_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_block(KaxBlock &block, KaxBlockAdditions *additions,
                            int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref);
  virtual void finish_file();
  virtual void headers_done();
};

#endif
