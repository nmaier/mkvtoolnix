/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_BASE_H
#define __XTR_BASE_H

#include "os.h"

#include <string>

#include <matroska/KaxTracks.h>

#include "common.h"
#include "compression.h"
#include "mkvextract.h"

using namespace std;
using namespace libmatroska;

class xtr_base_c {
public:
  string codec_id, file_name;
  xtr_base_c *master;
  mm_io_c *out;
  int64_t tid;
  int64_t default_duration;

  int64_t bytes_written;

  content_decoder_c content_decoder;

public:
  xtr_base_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);
  virtual ~xtr_base_c();

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_block(KaxBlock &block, KaxBlockAdditions *additions,
                            int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref);
  virtual void finish_file();

  virtual void headers_done();

  static xtr_base_c *create_extractor(const string &new_codec_id,
                                      int64_t new_tid, track_spec_t &tspec);
};

#endif
