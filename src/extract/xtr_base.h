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

int64_t kt_get_default_duration(KaxTrackEntry &track);
int64_t kt_get_number(KaxTrackEntry &track);
string kt_get_codec_id(KaxTrackEntry &track);
int kt_get_max_blockadd_id(KaxTrackEntry &track);

int kt_get_a_channels(KaxTrackEntry &track);
float kt_get_a_sfreq(KaxTrackEntry &track);
float kt_get_a_osfreq(KaxTrackEntry &track);
int kt_get_a_bps(KaxTrackEntry &track);

int kt_get_v_pixel_width(KaxTrackEntry &track);
int kt_get_v_pixel_height(KaxTrackEntry &track);

#endif
