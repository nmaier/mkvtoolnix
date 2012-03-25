/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for TRUEHD data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TRUEHD_COMMON_H
#define __MTX_COMMON_TRUEHD_COMMON_H

#include "common/common_pch.h"

#include <deque>

#include "common/byte_buffer.h"

#define TRUEHD_SYNC_WORD 0xf8726fba
#define MLP_SYNC_WORD    0xf8726fbb

struct truehd_frame_t {
  enum codec_e {
    truehd,
    mlp,
  } m_codec;

  enum frame_type_e {
    invalid,
    normal,
    sync,
    ac3,
  } m_type;

  int m_size;
  int m_sampling_rate;
  int m_channels;
  int m_samples_per_frame;

  memory_cptr m_data;

  truehd_frame_t()
    : m_codec(truehd)
    , m_type(invalid)
    , m_size(0)
    , m_sampling_rate(0)
    , m_channels(0)
    , m_samples_per_frame(0)
  { };

  bool is_truehd() {
    return truehd == m_codec;
  }

  bool is_sync() {
    return sync == m_type;
  }

  bool is_normal() {
    return sync == m_type;
  }

  bool is_ac3() {
    return ac3 == m_type;
  }
};
typedef std::shared_ptr<truehd_frame_t> truehd_frame_cptr;

class truehd_parser_c {
protected:
  enum {
    state_unsynced,
    state_synced,
  } m_sync_state;

  byte_buffer_c m_buffer;
  std::deque<truehd_frame_cptr> m_frames;

public:
  truehd_parser_c();
  virtual ~truehd_parser_c();

  virtual void add_data(const unsigned char *new_data, unsigned int new_size);
  virtual void parse(bool end_of_stream = false);
  virtual bool frame_available();
  virtual truehd_frame_cptr get_next_frame();

protected:
  virtual unsigned int resync(unsigned int offset);
  virtual int decode_channel_map(int channel_map);
};
typedef std::shared_ptr<truehd_parser_c> truehd_parser_cptr;

#endif // __MTX_COMMON_TRUEHD_COMMON_H
