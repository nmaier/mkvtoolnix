/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  dts_common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief helper function for DTS data
    \author Peter Niemayer <niemayer@isg.de>
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <string.h>
#include <stdio.h>

#include "common.h"
#include "dts_common.h"

// ---------------------------------------------------------------------------

struct channel_arrangement {
  int num_channels;
  const char * description;
};

static const channel_arrangement channel_arrangements[16] = {
  { 1, "A (mono)" },
  { 2, "A, B (dual mono)" },
  { 2, "L, R (left, right)" },
  { 2, "L+R, L-R (sum, difference)" },
  { 2, "LT, RT (left and right total)" },
  { 3, "C, L, R (center, left, right)" },
  { 3, "L, R, S (left, right, surround)" },
  { 4, "C, L, R, S (center, left, right, surround)" },
  { 4, "L, R, SL, SR (left, right, surround-left, surround-right)"},
  { 5, "C, L, R, SL, SR (center, left, right, surround-left, surround-right)"},
  { 6, "CL, CR, L, R, SL, SR (center-left, center-right, left, right, "
    "surround-left, surround-right)"},
  { 6, "C, L, R, LR, RR, OV (center, left, right, left-rear, right-rear, "
    "overhead)"},
  { 6, "CF, CR, LF, RF, LR, RR  (center-front, center-rear, left-front, "
    "right-front, left-rear, right-rear)"},
  { 7, "CL, C, CR, L, R, SL, SR  (center-left, center, center-right, left, "
    "right, surround-left, surround-right)"},
  { 8, "CL, CR, L, R, SL1, SL2, SR1, SR2 (center-left, center-right, left, "
    "right, surround-left1, surround-left2, surround-right1, "
    "surround-right2)"},
  { 8, "CL, C, CR, L, R, SL, S, SR (center-left, center, center-right, left, "
    "right, surround-left, surround, surround-right)"}
  // other modes are not defined as of yet
};

static const int core_samplefreqs[16] = {
     -1, 8000, 16000, 32000,    -1,    -1, 11025, 22050,
  44100,   -1,    -1, 12000, 24000, 48000,    -1,    -1
};

static const int transmission_bitrates[32] = {
     32000,     56000,     64000,     96000,
   112000,    128000,    192000,    224000,
   256000,    320000,    384000,    448000,
   512000,    576000,    640000,    768000,
   960000,   1024000,   1152000,   1280000,
  1344000,   1408000,   1411200,   1472000,
  1536000,   1920000,   2048000,   3072000,
  3840000,   -1 /*open*/, -2 /*variable*/, -3 /*lossless*/
  // [15]  768000 is actually 754500 for DVD
  // [24] 1536000 is actually 1509750 for DVD
  // [22] 1411200 is actually 1234800 for 14-bit DTS-CD audio
};

enum source_pcm_resolution {
  spr_16 = 0,
  spr_16_ES,  //_ES means: surround channels mastered in DTS-ES
  spr_20,
  spr_20_ES,
  spr_invalid4,
  spr_24_ES,
  spr_24,
  spr_invalid7
};

int find_dts_header(const unsigned char *buf, unsigned int size,
                    struct dts_header_s *dts_header) {

  unsigned int size_to_search = size-15;
  if (size_to_search > size) {
    // not enough data for one header
    return -1;
  }

  unsigned int offset;
  for (offset = 0; offset < size_to_search; offset++) {
     // sync words appear aligned in the bit stream
    if (buf[offset]     == 0x7f &&
        buf[offset + 1] == 0xfe &&
        buf[offset + 2] == 0x80 &&
        buf[offset + 3] == 0x01) {
      break;
    }
  }
  if (offset >= size_to_search) {
    // no header found
    return -1;
  }

  bit_cursor_c bc(buf+offset+4, size-offset-4);

  unsigned int t;

  bc.get_bits(1, t);
  dts_header->frametype = (t)? dts_header_s::frametype_normal :
    dts_header_s::frametype_termination;

  bc.get_bits(5, t);
  dts_header->deficit_sample_count = (t+1) % 32;

  bc.get_bits(1, t);
  dts_header->crc_present = (t)? true : false;

  bc.get_bits(7, t);
  if (t < 5)  {
    mxprint(stderr,"DTS_Header problem: invalid number of blocks in frame\n");
    //return -1;
  }
  dts_header->num_pcm_sample_blocks = t + 1;

  bc.get_bits(14, t);
  if (t < 95) {
    mxprint(stderr,"DTS_Header problem: invalid frame bytes size\n");
    return -1;
  }
  dts_header->frame_byte_size = t+1;

  bc.get_bits(6, t);
  if (t >= 16) {
    dts_header->audio_channels = -1;
    dts_header->audio_channel_arrangement = "unknown (user defined)";
  } else {
    dts_header->audio_channels = channel_arrangements[t].num_channels;
    dts_header->audio_channel_arrangement =
      channel_arrangements[t].description;
  }

  bc.get_bits(4, t);
  dts_header->core_sampling_frequency = core_samplefreqs[t];
  if (dts_header->core_sampling_frequency < 0) {
    mxprint(stderr,"DTS_Header problem: invalid core sampling frequency\n");
    return -1;
  }

  bc.get_bits(5, t);
  dts_header->transmission_bitrate = transmission_bitrates[t];

  bc.get_bit(dts_header->embedded_down_mix);
  bc.get_bit(dts_header->embedded_dynamic_range);
  bc.get_bit(dts_header->embedded_time_stamp);
  bc.get_bit(dts_header->auxiliary_data);
  bc.get_bit(dts_header->hdcd_master);

  bc.get_bits(3, t);
  dts_header->extension_audio_descriptor =
    (dts_header_s::extension_audio_descriptor_enum)t;

  bc.get_bit(dts_header->extended_coding);

  bc.get_bit(dts_header->audio_sync_word_in_sub_sub);

  bc.get_bits(2, t);
  dts_header->lfe_type = (dts_header_s::lfe_type_enum)t;

  bc.get_bit(dts_header->predictor_history_flag);

  if (dts_header->crc_present) {
    bc.get_bits(16, t);
    // unsigned short header_CRC_sum = t; // not used yet
  }

  bc.get_bits(1, t);
  dts_header->multirate_interpolator =
    (dts_header_s::multirate_interpolator_enum)t;

  bc.get_bits(4, t);
  dts_header->encoder_software_revision = t;
  if (t > 7) {
    mxprint(stderr,"DTS_Header problem: encoded with an incompatible new "
            "encoder version\n");
    return -1;
  }

  bc.get_bits(2, t);
  dts_header->copy_history = t;

  bc.get_bits(3, t);
  switch (t) {
    case spr_16:
      dts_header->source_pcm_resolution = 16;
      dts_header->source_surround_in_es = false;
      break;

    case spr_16_ES:
      dts_header->source_pcm_resolution = 16;
      dts_header->source_surround_in_es = true;
      break;

    case spr_20:
      dts_header->source_pcm_resolution = 20;
      dts_header->source_surround_in_es = false;
      break;

    case spr_20_ES:
      dts_header->source_pcm_resolution = 20;
      dts_header->source_surround_in_es = true;
      break;

    case spr_24:
      dts_header->source_pcm_resolution = 24;
      dts_header->source_surround_in_es = false;
      break;

    case spr_24_ES:
      dts_header->source_pcm_resolution = 24;
      dts_header->source_surround_in_es = true;
      break;

    default:
      mxprint(stderr,"DTS_Header problem: invalid source PCM resolution\n");
      return -1;
  }

  bc.get_bit(dts_header->front_sum_difference);

  bc.get_bit(dts_header->surround_sum_difference);

  bool out_of_data = bc.get_bits(4, t);
  if (dts_header->encoder_software_revision == 7) {
    dts_header->dialog_normalization_gain = -((int)t);
  } else if (dts_header->encoder_software_revision == 6) {
    dts_header->dialog_normalization_gain = -16-((int)t);
  } else {
    dts_header->dialog_normalization_gain = 0;
  }

  if (out_of_data) {
    mxprint(stderr,"DTS_Header problem: not enough data to read header\n");
    return -1;
  }

  return offset;

}

// ============================================================================

void print_dts_header(const struct dts_header_s *h) {

  mxprint(stderr,"DTS Frame Header Information:\n");

  mxprint(stderr,"Frame Type             : ");
  if (h->frametype == dts_header_s::frametype_normal) {
    mxprint(stderr,"normal");
  } else {
    mxprint(stderr,"termination, deficit sample count = %u",
            h->deficit_sample_count);
  }
  mxprint(stderr,"\n");

  mxprint(stderr,"CRC available          : %s\n", (h->crc_present)? "yes" :
          "no");

  mxprint(stderr,"Frame Size             : PCM core samples=32*%u=%u, %f "
          "milliseconds, %u byte\n",
          h->num_pcm_sample_blocks, h->num_pcm_sample_blocks * 32,
          (h->num_pcm_sample_blocks * 32000.0) / h->core_sampling_frequency,
          h->frame_byte_size);

  mxprint(stderr,"Audio Channels         : %d%s, arrangement: %s\n",
          h->audio_channels, (h->source_surround_in_es)? " ES" : "" ,
          h->audio_channel_arrangement);

  mxprint(stderr,"Core sampling frequency: %u\n", h->core_sampling_frequency);

  mxprint(stderr,"Transmission_bitrate   : ");
  if (h->transmission_bitrate == -1) {
    mxprint(stderr,"open");
  } else if (h->transmission_bitrate == -2) {
    mxprint(stderr,"variable");
  } else if (h->transmission_bitrate == -3) {
    mxprint(stderr,"lossless");
  } else {
    mxprint(stderr,"%d", h->transmission_bitrate);
  }
  mxprint(stderr,"\n");


  mxprint(stderr,"Embedded Down Mix      : %s\n",
          (h->embedded_down_mix)? "yes" : "no");
  mxprint(stderr,"Embedded Dynamic Range : %s\n",
          (h->embedded_dynamic_range)? "yes" : "no");
  mxprint(stderr,"Embedded Time Stamp    : %s\n",
          (h->embedded_time_stamp)? "yes" : "no");
  mxprint(stderr,"Embedded Auxiliary Data: %s\n",
          (h->auxiliary_data)? "yes" : "no");
  mxprint(stderr,"HDCD Master            : %s\n",
          (h->hdcd_master)? "yes" : "no");

  mxprint(stderr,"Extended Coding        : ");
  if (h->extended_coding) {
    switch (h->extension_audio_descriptor) {
      case dts_header_s::extension_xch:
        mxprint(stderr,"Extra Channels");
        break;
      case dts_header_s::extension_x96k:
        mxprint(stderr,"Extended frequency (x96k)");
        break;
      case dts_header_s::extension_xch_x96k:
        mxprint(stderr,"Extra Channels and Extended frequency (x96k)");
        break;
      default:
        mxprint(stderr,"yes, but unknown");
        break;
    }
  } else {
    mxprint(stderr,"no");
  }
  mxprint(stderr,"\n");

  mxprint(stderr,"Audio Sync in sub-subs : %s\n",
          (h->audio_sync_word_in_sub_sub)? "yes" : "no");

  mxprint(stderr,"Low Frequency Effects  : ");
  switch (h->lfe_type) {
    case dts_header_s::lfe_none:
      mxprint(stderr,"none");
      break;
    case dts_header_s::lfe_128:
      mxprint(stderr,"yes, interpolation factor 128");
      break;
    case dts_header_s::lfe_64:
      mxprint(stderr,"yes, interpolation factor 64");
      break;
    case dts_header_s::lfe_invalid:
      mxprint(stderr,"Invalid");
      break;
  }
  mxprint(stderr,"\n");

  mxprint(stderr,"Predictor History used : %s\n",
          (h->predictor_history_flag)? "yes" : "no");

  mxprint(stderr,"Multirate Interpolator : %s\n",
          (h->multirate_interpolator == dts_header_s::mi_non_perfect)?
          "non perfect" : "perfect");

  mxprint(stderr,"Encoder Software Vers. : %u\n",
          h->encoder_software_revision);
  mxprint(stderr,"Copy History Bits      : %u\n", h->copy_history);
  mxprint(stderr,"Source PCM Resolution  : %d\n", h->source_pcm_resolution);

  mxprint(stderr,"Front Encoded as Diff. : %s\n",
          (h->front_sum_difference)? "yes" : "no");
  mxprint(stderr,"Surr. Encoded as Diff. : %s\n",
          (h->surround_sum_difference)? "yes" : "no");

  mxprint(stderr,"Dialog Normaliz. Gain  : %d\n",
          h->dialog_normalization_gain);
}

