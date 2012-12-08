/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_XTR_OGG_H
#define MTX_XTR_OGG_H

#include "common/common_pch.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common/kate.h"
#include "common/theora.h"
#include "extract/xtr_base.h"

class xtr_flac_c: public xtr_base_c {
public:
  xtr_flac_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
};

class xtr_oggbase_c: public xtr_base_c {
public:
  ogg_stream_state m_os;
  unsigned int m_packetno;
  memory_cptr m_queued_frame;
  int64_t m_queued_granulepos;
  int m_sfreq;
  int64_t m_previous_end;

public:
  xtr_oggbase_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);
  virtual ~xtr_oggbase_c();

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual void write_pages();
  virtual void flush_pages();

protected:
  virtual void create_standard_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void header_packets_unlaced(std::vector<memory_cptr> &header_packets);

  virtual void queue_frame(memory_cptr &frame, int64_t granulepos);
  virtual void write_queued_frame(bool eos);
};

class xtr_oggvorbis_c: public xtr_oggbase_c {
protected:
  vorbis_info m_vorbis_info;
  vorbis_comment m_vorbis_comment;
  int64_t m_previous_block_size, m_samples;

public:
  xtr_oggvorbis_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);
  virtual ~xtr_oggvorbis_c();

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void header_packets_unlaced(std::vector<memory_cptr> &header_packets);

  virtual const char *get_container_name() {
    return "Ogg (Vorbis in Ogg)";
  };
};

class xtr_oggkate_c: public xtr_oggbase_c {
private:
  kate_identification_header_t m_kate_id_header;

public:
  xtr_oggkate_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "Ogg (Kate in Ogg)";
  };

protected:
  virtual void header_packets_unlaced(std::vector<memory_cptr> &header_packets);
};

class xtr_oggtheora_c: public xtr_oggbase_c {
private:
  theora_identification_header_t m_theora_header;
  int64_t m_keyframe_number, m_non_keyframe_number;

public:
  xtr_oggtheora_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "Ogg (Theora in Ogg)";
  };

protected:
  virtual void header_packets_unlaced(std::vector<memory_cptr> &header_packets);
};

#endif
