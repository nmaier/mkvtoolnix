/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  dts_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
  \file
  \version \$Id: dts_common.h,v 1.5 2003/05/20 06:27:08 mosu Exp $
  \brief definitions and helper functions for DTS data
  \author Peter Niemayer <niemayer@isg.de>
  \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __DTSCOMMON_H
#define __DTSCOMMON_H

static const long long max_dts_packet_size = 15384;

/* The following code looks a little odd as it was written in C++
   but with the possibility in mind to make this structure and the
   functions below it later available in C
*/

typedef struct dts_header_s {
  
  // ---------------------------------------------------

  // ---------------------------------------------------
  
  enum {
    // Used to extremely precisely specify the end-of-stream (single PCM
    // sample resolution).
    frametype_termination = 0, 

    frametype_normal
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
  
  enum extension_audio_descriptor_enum {
    extension_xch = 0,          // channel extension
    extension_unknown1,
    extension_x96k,             // frequency extension
    extension_xch_x96k,         // both channel and frequency extension
    extension_unknown4,
    extension_unknown5,
    extension_unknown6,
    extension_unknown7
  } extension_audio_descriptor; // significant only if extended_coding == true

  // if true, extended coding data is placed after the core audio data
  bool extended_coding;
  
  // if true, audio data check words are placed in each sub-sub-frame
  // rather than in each sub-frame, only
  bool audio_sync_word_in_sub_sub;
  
  enum lfe_type_enum {
    lfe_none,
    lfe_128, // 128 indicates the interpolation factor to reconstruct the
             // LFE channel 
    lfe_64,  //  64 indicates the interpolation factor to reconstruct the
             // LFE channel 
    lfe_invalid
  } lfe_type;
  
  // if true, past frames will be used to predict ADPCM values for the
  // current one. This means, if this flag is false, the current frame is
  // better suited as an audio-jump-point (like an "I-frame" in video-coding).
  bool predictor_history_flag; 
  
  // which FIR coefficients to use for sub-band reconstruction
  enum multirate_interpolator_enum {
    mi_non_perfect,
    mi_perfect
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
} dts_header_t;


int find_dts_header(const unsigned char *buf, unsigned int size,
                    struct dts_header_s *dts_header);
void print_dts_header(const struct dts_header_s *dts_header);

inline int get_dts_packet_length_in_core_samples(const struct dts_header_s
                                                 *dts_header) {
  // computes the length (in time, not size) of the packet in "samples".
  int r;

  r = dts_header->num_pcm_sample_blocks * 32;
  if (dts_header->frametype == dts_header_s::frametype_termination)
    r -= dts_header->deficit_sample_count;

  return r;
}

inline double get_dts_packet_length_in_milliseconds(const struct dts_header_s
                                                    *dts_header) {
  // computes the length (in time, not size) of the packet in "samples".
  int samples = get_dts_packet_length_in_core_samples(dts_header);

  double t = ((double)samples*1000.0) /
    ((double)dts_header->core_sampling_frequency);

  return t;
}

#endif // __DTSCOMMON_H
