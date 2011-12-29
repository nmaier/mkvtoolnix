/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for DTS data

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_DTSCOMMON_H
#define __MTX_COMMON_DTSCOMMON_H

#define DTS_HEADER_MAGIC    0x7ffe8001
#define DTS_HD_HEADER_MAGIC 0x64582025

static const int64_t max_dts_packet_size = 15384;

/* The following code looks a little odd as it was written in C++
   but with the possibility in mind to make this structure and the
   functions below it later available in C
*/

typedef struct dts_header_s {

  // ---------------------------------------------------

  // ---------------------------------------------------

  enum frametype_e {
    // Used to extremely precisely specify the end-of-stream (single PCM
    // sample resolution).
    FRAMETYPE_TERMINATION = 0,

    FRAMETYPE_NORMAL
  } frametype;

  // 0 for normal frames, 1 to 30 for termination frames. Number of PCM
  // samples the frame is shorter than normal.
  unsigned int deficit_sample_count;

  // If true, a CRC-sum is included in the data.
  bool crc_present;

  // number of PCM core sample blocks in this frame. Each PCM core sample block
  // consists of 32 samples. Notice that "core samples" means "samples
  // after the input decimator", so at sampling frequencies >48kHz, one core
  // sample represents 2 (or 4 for frequencies >96kHz) output samples.
  unsigned int num_pcm_sample_blocks;

  // Number of bytes this frame occupies (range: 95 to 16 383).
  unsigned int frame_byte_size;

  // Number of audio channels, -1 for "unknown".
  int audio_channels;

  // String describing the audio channel arrangement
  const char *audio_channel_arrangement;

  // -1 for "invalid"
  unsigned int core_sampling_frequency;

  // in bit per second, or -1 == "open", -2 == "variable", -3 == "lossless"
  int transmission_bitrate;

  // if true, sub-frames contain coefficients for downmixing to stereo
  bool embedded_down_mix;

  // if true, sub-frames contain coefficients for dynamic range correction
  bool embedded_dynamic_range;

  // if true, a time stamp is embedded at the end of the core audio data
  bool embedded_time_stamp;

  // if true, auxiliary data is appended at the end of the core audio data
  bool auxiliary_data;

  // if true, the source material was mastered in HDCD format
  bool hdcd_master;

  enum extension_audio_descriptor_e {
    EXTENSION_XCH = 0,          // channel extension
    EXTENSION_UNKNOWN1,
    EXTENSION_X96K,             // frequency extension
    EXTENSION_XCH_X96K,         // both channel and frequency extension
    EXTENSION_UNKNOWN4,
    EXTENSION_UNKNOWN5,
    EXTENSION_UNKNOWN6,
    EXTENSION_UNKNOWN7
  } extension_audio_descriptor; // significant only if extended_coding == true

  // if true, extended coding data is placed after the core audio data
  bool extended_coding;

  // if true, audio data check words are placed in each sub-sub-frame
  // rather than in each sub-frame, only
  bool audio_sync_word_in_sub_sub;

  enum lfe_type_e {
    LFE_NONE,
    LFE_128, // 128 indicates the interpolation factor to reconstruct the
             // LFE channel
    LFE_64,  //  64 indicates the interpolation factor to reconstruct the
             // LFE channel
    LFE_INVALID
  } lfe_type;

  // if true, past frames will be used to predict ADPCM values for the
  // current one. This means, if this flag is false, the current frame is
  // better suited as an audio-jump-point (like an "I-frame" in video-coding).
  bool predictor_history_flag;

  // which FIR coefficients to use for sub-band reconstruction
  enum multirate_interpolator_e {
    MI_NON_PERFECT,
    MI_PERFECT
  } multirate_interpolator;

  // 0 to 15
  unsigned int encoder_software_revision;

  // 0 to 3 - "top-secret" bits indicating the "copy history" of the material
  unsigned int copy_history;

  // 16, 20 or 24 bits per sample, or -1 == invalid
  int source_pcm_resolution;

  // if true, source surround channels are mastered in DTS-ES
  bool source_surround_in_es;

  // if true, left and right front channels are encoded as
  // sum and difference (L = L + R, R = L - R)
  bool front_sum_difference;

  // same as front_sum_difference for surround left and right channels
  bool surround_sum_difference;

  // gain in dB to apply for dialog normalization
  int dialog_normalization_gain;

  bool dts_hd;
  enum dts_hd_type_e {
    DTSHD_NONE,
    DTSHD_HIGH_RESOLUTION,
    DTSHD_MASTER_AUDIO,
  } hd_type;
  int hd_part_size;

} dts_header_t;

int find_dts_sync_word(const unsigned char *buf, unsigned int size);
int find_dts_header(const unsigned char *buf, unsigned int size, struct dts_header_s *dts_header, bool allow_no_hd_search = false);
int find_consecutive_dts_headers(const unsigned char *buf, unsigned int size, unsigned int num);
void print_dts_header(const struct dts_header_s *dts_header);

bool operator ==(const dts_header_s &h1, const dts_header_s &h2);

inline int
get_dts_packet_length_in_core_samples(const struct dts_header_s *dts_header) {
  // computes the length (in time, not size) of the packet in "samples".
  int r = dts_header->num_pcm_sample_blocks * 32;
  if (dts_header_s::FRAMETYPE_TERMINATION == dts_header->frametype)
    r -= dts_header->deficit_sample_count;

  return r;
}

inline double
get_dts_packet_length_in_nanoseconds(const struct dts_header_s *dts_header) {
  // computes the length (in time, not size) of the packet in "samples".
  int samples = get_dts_packet_length_in_core_samples(dts_header);

  return static_cast<double>(samples) * 1000000000.0 / dts_header->core_sampling_frequency;
}

void dts_14_to_dts_16(const unsigned short *src, unsigned long srcwords, unsigned short *dst);

bool detect_dts(const void *src_buf, int len, bool &dts14_to_16, bool &swap_bytes);

bool operator!=(const dts_header_t &l, const dts_header_t &r);

#endif // __MTX_COMMON_DTSCOMMON_H
