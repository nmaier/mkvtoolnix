/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_BASE_H
#define __XTR_BASE_H

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxTracks.h>

#include "common/compression.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

class xtr_base_c {
public:
  std::string m_codec_id, m_file_name, m_container_name;
  xtr_base_c *m_master;
  mm_io_cptr m_out;
  int64_t m_tid, m_track_num;
  int64_t m_default_duration;

  int64_t m_bytes_written;

  content_decoder_c m_content_decoder;
  bool m_content_decoder_initialized;

public:
  xtr_base_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec, const char *container_name = NULL);
  virtual ~xtr_base_c();

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void handle_codec_state(memory_cptr &/* codec_state */) {
  };
  virtual void finish_track();
  virtual void finish_file();

  virtual void headers_done();

  virtual const char *get_container_name() {
    return m_container_name.c_str();
  };

  virtual void init_content_decoder(KaxTrackEntry &track);
  virtual memory_cptr decode_codec_private(KaxCodecPrivate *priv);

  static xtr_base_c *create_extractor(const std::string &new_codec_id, int64_t new_tid, track_spec_t &tspec);
};

class xtr_fullraw_c : public xtr_base_c {
public:
  xtr_fullraw_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec):
    xtr_base_c(codec_id, tid, tspec) {}
  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_codec_state(memory_cptr &codec_state);
};

#endif
