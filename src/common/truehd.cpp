/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper function for TrueHD data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/endian.h"
#include "common/memory.h"
#include "common/truehd.h"

truehd_parser_c::truehd_parser_c()
  : m_sync_state(state_unsynced)
{
}

truehd_parser_c::~truehd_parser_c()
{
}

// Code for truehd_parser_c::decode_channel_map was taken from
// the ffmpeg project, source file "libavcodec/mlp_parser.c".
int
truehd_parser_c::decode_channel_map(int channel_map) {
  static const int channel_count[13] = {
    //  LR    C   LFE  LRs LRvh  LRc LRrs  Cs   Ts  LRsd  LRw  Cvh  LFE2
         2,   1,   1,   2,   2,   2,   2,   1,   1,   2,   2,   1,   1
  };

  int channels = 0, i;

  for (i = 0; 13 > i; ++i)
    channels += channel_count[i] * ((channel_map >> i) & 1);

  return channels;
}

void
truehd_parser_c::add_data(const unsigned char *new_data,
                          unsigned int new_size) {
  if (!new_data || (0 == new_size))
    return;

  m_buffer.add(new_data, new_size);

  parse(false);
}

void
truehd_parser_c::parse(bool end_of_stream) {
  unsigned char *data = m_buffer.get_buffer();
  unsigned int size   = m_buffer.get_size();
  unsigned int offset = 0;

  if (10 > size)
    return;

  if (state_unsynced == m_sync_state) {
    offset = resync(offset);
    if (state_unsynced == m_sync_state)
      return;
  }

  static const int sampling_rates[]     = { 48000, 96000, 192000, 0, 0, 0, 0, 0, 44100, 88200, 176400, 0, 0, 0, 0, 0 };
  static const uint8_t mlp_channels[32] = {     1,     2,      3, 4, 3, 4, 5, 3,     4,     5,      4, 5, 6, 4, 5, 4,
                                                5,     6,      5, 5, 6, 0, 0, 0,     0,     0,      0, 0, 0, 0, 0, 0 };

  while ((size - offset) >= 10) {
    truehd_frame_cptr frame(new truehd_frame_t);

    uint32_t sync_word = get_uint32_be(&data[offset + 4]);
    if ((TRUEHD_SYNC_WORD == sync_word) || (MLP_SYNC_WORD == sync_word)) {
      frame->m_codec = TRUEHD_SYNC_WORD == sync_word ? truehd_frame_t::truehd : truehd_frame_t::mlp;
      frame->m_type  = truehd_frame_t::sync;
      frame->m_size  = (((data[offset] << 8) | data[offset + 1]) & 0xfff) * 2;

      if (frame->is_truehd()) {
        frame->m_sampling_rate     = sampling_rates[data[offset + 8] >> 4];
        frame->m_samples_per_frame = 40 << ((data[offset + 8] >> 4) & 0x07);
        int chanmap_substream_1    = ((data[offset +  9] & 0x0f) << 1) | (data[offset + 10] >> 7);
        int chanmap_substream_2    = ((data[offset + 10] & 0x1f) << 8) |  data[offset + 11];
        frame->m_channels          = decode_channel_map(chanmap_substream_2 ? chanmap_substream_2 : chanmap_substream_1);

      } else {
        frame->m_sampling_rate     = sampling_rates[data[offset + 9] >> 4];
        frame->m_samples_per_frame = 40 << ((data[offset + 9] >> 4) & 0x07);
        frame->m_channels          = mlp_channels[data[offset + 11] & 0x1f];
      }

    } else if (get_uint16_be(&data[offset]) == 0x0b77) {
      ac3::frame_c ac3_frame;
      if (ac3_frame.decode_header(&data[offset], size - offset)) {
        if (((size - offset) < ac3_frame.m_bytes) && !end_of_stream)
          break;

        if (((size - offset) >= ac3_frame.m_bytes) && verify_ac3_checksum(&data[offset], size - offset)) {
          frame->m_type = truehd_frame_t::ac3;
          frame->m_size = ac3_frame.m_bytes;
        }
      }
    }

    if (truehd_frame_t::invalid == frame->m_type) {
      frame->m_type = truehd_frame_t::normal;
      frame->m_size = (((data[offset] << 8) | data[offset + 1]) & 0xfff) * 2;
    }

    if (8 > frame->m_size) {
      unsigned int synced_at = resync(offset + 1);
      if (state_unsynced == m_sync_state)
        break;

      offset = synced_at;
    }

    if ((frame->m_size + offset) > size)
      break;

    frame->m_data = memory_c::clone(&data[offset], frame->m_size);

    mxverb(3,
           boost::format("codec %7% type %1% offset %2% size %3% channels %4% sampling_rate %5% samples_per_frame %6%\n")
           % (  frame->is_sync()   ? "S"
              : frame->is_normal() ? "n"
              : frame->is_ac3()    ? "A"
              :                      "x")
           % offset % frame->m_size % frame->m_channels % frame->m_sampling_rate % frame->m_samples_per_frame
           % (  frame->is_truehd() ? "TrueHD"
              :                      "MLP"));

    m_frames.push_back(frame);

    offset += frame->m_size;
  }

  m_buffer.remove(offset);
}

bool
truehd_parser_c::frame_available() {
  return !m_frames.empty();
}

truehd_frame_cptr
truehd_parser_c::get_next_frame() {
  if (m_frames.empty())
    return truehd_frame_cptr{};

  truehd_frame_cptr frame = *m_frames.begin();
  m_frames.pop_front();

  return frame;
}

unsigned int
truehd_parser_c::resync(unsigned int offset) {
  const unsigned char *data = m_buffer.get_buffer();
  unsigned int size         = m_buffer.get_size();

  m_sync_state              = state_unsynced;

  for (offset = offset + 4; (offset + 4) < size; ++offset) {
    uint32_t sync_word = get_uint32_be(&data[offset]);
    if ((TRUEHD_SYNC_WORD == sync_word) || (MLP_SYNC_WORD == sync_word)) {
      m_sync_state  = state_synced;
      return offset - 4;
    }
  }

  return 0;
}
