/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the AVI demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_AVI_H
#define __R_AVI_H

#include "os.h"

#include <stdio.h>

#include <vector>

extern "C" {
#include <avilib.h>
}

#include "mm_io.h"
#include "pr_generic.h"
#include "common.h"
#include "error.h"
#include "subtitles.h"

typedef struct avi_demuxer_t {
  int ptzr;
  int channels, bits_per_sample, samples_per_second, aid;
  int64_t bytes_processed;
} avi_demuxer_t;

struct avi_subs_demuxer_t {
  enum {
    TYPE_UNKNOWN,
    TYPE_SRT,
    TYPE_SSA,
  } type;

  int ptzr;

  string sub_language;
  memory_cptr subtitles;

  mm_text_io_cptr text_io;
  subtitles_cptr subs;
};

class avi_reader_c: public generic_reader_c {
private:
  enum divx_type_e {
    DIVX_TYPE_NONE,
    DIVX_TYPE_V3,
    DIVX_TYPE_MPEG4
  } divx_type;

  avi_t *avi;
  int vptzr;
  vector<avi_demuxer_t> ademuxers;
  vector<avi_subs_demuxer_t> sdemuxers;
  double fps;
  int video_frames_read, max_video_frames, dropped_video_frames, act_wchar;
  bool is_divx, rederive_keyframes;

  int64_t bytes_to_process, bytes_processed;

public:
  avi_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~avi_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *io, int64_t size);

protected:
  virtual void add_audio_demuxer(int aid);
  virtual int is_keyframe(unsigned char *data, long size, int suggestion);
  virtual file_status_e read_video();
  virtual file_status_e read_audio(avi_demuxer_t &demuxer);
  virtual file_status_e read_subtitles(avi_subs_demuxer_t &demuxer);
  virtual memory_cptr extract_avcc();

  virtual generic_packetizer_c *create_aac_packetizer(int aid, avi_demuxer_t &demuxer);
  virtual generic_packetizer_c *create_vorbis_packetizer(int aid);
  virtual void create_subs_packetizer(int idx);
  virtual void create_srt_packetizer(int idx);
  virtual void create_ssa_packetizer(int idx);

  void extended_identify_mpeg4_l2(vector<string> &extended_info);

  void parse_subtitle_chunks();

  virtual void identify_video();
  virtual void identify_audio();
  virtual void identify_subtitles();
};

#endif  // __R_AVI_H
