/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the AVI demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_AVI_H
#define __R_AVI_H

#include "common/common_pch.h"

#include <stdio.h>

#include <avilib.h>

#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/error.h"
#include "input/subtitles.h"
#include "output/p_avc.h"

typedef struct avi_demuxer_t {
  int m_ptzr;
  int m_channels, m_bits_per_sample, m_samples_per_second, m_aid;
  int64_t m_bytes_processed;

  avi_demuxer_t()
    : m_ptzr(-1)
    , m_channels(0)
    , m_bits_per_sample(0)
    , m_samples_per_second(0)
    , m_aid(0)
    , m_bytes_processed(0)
  {
  }
} avi_demuxer_t;

struct avi_subs_demuxer_t {
  enum {
    TYPE_UNKNOWN,
    TYPE_SRT,
    TYPE_SSA,
  } m_type;

  int m_ptzr;

  std::string m_sub_language;
  memory_cptr m_subtitles;

  mm_text_io_cptr m_text_io;
  subtitles_cptr m_subs;

  avi_subs_demuxer_t();
};

class avi_reader_c: public generic_reader_c {
private:
  enum divx_type_e {
    DIVX_TYPE_NONE,
    DIVX_TYPE_V3,
    DIVX_TYPE_MPEG4
  } m_divx_type;

  avi_t *m_avi;
  int m_vptzr;
  std::vector<avi_demuxer_t> m_audio_demuxers;
  std::vector<avi_subs_demuxer_t> m_subtitle_demuxers;
  double m_fps;
  unsigned int m_video_frames_read, m_max_video_frames, m_dropped_video_frames;
  int m_avc_nal_size_size;

  uint64_t m_bytes_to_process, m_bytes_processed;
  bool m_video_track_ok;

public:
  avi_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~avi_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("AVI") : "AVI";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static bool probe_file(mm_io_c *in, uint64_t size);

protected:
  virtual void add_audio_demuxer(int aid);
  virtual file_status_e read_video();
  virtual file_status_e read_audio(avi_demuxer_t &demuxer);
  virtual file_status_e read_subtitles(avi_subs_demuxer_t &demuxer);

  virtual generic_packetizer_c *create_aac_packetizer(int aid, avi_demuxer_t &demuxer);
  virtual generic_packetizer_c *create_dts_packetizer(int aid);
  virtual generic_packetizer_c *create_vorbis_packetizer(int aid);
  virtual void create_subs_packetizer(int idx);
  virtual void create_srt_packetizer(int idx);
  virtual void create_ssa_packetizer(int idx);
  virtual void create_standard_video_packetizer();
  virtual void create_mpeg1_2_packetizer();
  virtual void create_mpeg4_p2_packetizer();
  virtual void create_mpeg4_p10_packetizer();
  virtual void create_vp8_packetizer();
  virtual void create_video_packetizer();

  virtual void set_avc_nal_size_size(mpeg4_p10_es_video_packetizer_c *ptzr);

  void extended_identify_mpeg4_l2(std::vector<std::string> &extended_info);

  void parse_subtitle_chunks();
  void verify_video_track();

  virtual void identify_video();
  virtual void identify_audio();
  virtual void identify_subtitles();
  virtual void identify_attachments();

  virtual void debug_dump_video_index();
};

#endif  // __R_AVI_H
