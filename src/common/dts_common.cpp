/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * helper function for DTS data
 *
 * Written by Peter Niemayer <niemayer@isg.de>.
 * Modified by Moritz Bunkus <moritz@bunkus.org>.
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

int
find_dts_header(const unsigned char *buf,
                unsigned int size,
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
    mxwarn("DTS_Header problem: invalid number of blocks in frame\n");
    //return -1;
  }
  dts_header->num_pcm_sample_blocks = t + 1;

  bc.get_bits(14, t);
  if (t < 95) {
    mxwarn("DTS_Header problem: invalid frame bytes size\n");
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
    mxwarn("DTS_Header problem: invalid core sampling frequency\n");
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
    mxwarn("DTS_Header problem: encoded with an incompatible new "
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
      mxwarn("DTS_Header problem: invalid source PCM resolution\n");
      return -1;
  }

  bc.get_bit(dts_header->front_sum_difference);

  bc.get_bit(dts_header->surround_sum_difference);

  bool out_of_data = !bc.get_bits(4, t);
  if (dts_header->encoder_software_revision == 7) {
    dts_header->dialog_normalization_gain = -((int)t);
  } else if (dts_header->encoder_software_revision == 6) {
    dts_header->dialog_normalization_gain = -16-((int)t);
  } else {
    dts_header->dialog_normalization_gain = 0;
  }

  if (out_of_data) {
    mxwarn("DTS_Header problem: not enough data to read header\n");
    return -1;
  }

  return offset;

}

// ============================================================================

void
print_dts_header(const struct dts_header_s *h) {
  mxinfo("DTS Frame Header Information:\n");

  mxinfo("Frame Type             : ");
  if (h->frametype == dts_header_s::frametype_normal) {
    mxinfo("normal");
  } else {
    mxinfo("termination, deficit sample count = %u",
           h->deficit_sample_count);
  }
  mxinfo("\n");

  mxinfo("CRC available          : %s\n", (h->crc_present)? "yes" : "no");

  mxinfo("Frame Size             : PCM core samples=32*%u=%u, %f "
         "milliseconds, %u byte\n",
         h->num_pcm_sample_blocks, h->num_pcm_sample_blocks * 32,
         (h->num_pcm_sample_blocks * 32000.0) / h->core_sampling_frequency,
         h->frame_byte_size);

  mxinfo("Audio Channels         : %d%s, arrangement: %s\n",
         h->audio_channels, (h->source_surround_in_es)? " ES" : "" ,
         h->audio_channel_arrangement);
  
  mxinfo("Core sampling frequency: %u\n", h->core_sampling_frequency);

  mxinfo("Transmission_bitrate   : ");
  if (h->transmission_bitrate == -1)
    mxinfo("open");
  else if (h->transmission_bitrate == -2)
    mxinfo("variable");
  else if (h->transmission_bitrate == -3)
    mxinfo("lossless");
  else
    mxinfo("%d", h->transmission_bitrate);
  mxinfo("\n");


  mxinfo("Embedded Down Mix      : %s\n",
         (h->embedded_down_mix)? "yes" : "no");
  mxinfo("Embedded Dynamic Range : %s\n",
         (h->embedded_dynamic_range)? "yes" : "no");
  mxinfo("Embedded Time Stamp    : %s\n",
         (h->embedded_time_stamp)? "yes" : "no");
  mxinfo("Embedded Auxiliary Data: %s\n",
         (h->auxiliary_data)? "yes" : "no");
  mxinfo("HDCD Master            : %s\n",
         (h->hdcd_master)? "yes" : "no");

  mxinfo("Extended Coding        : ");
  if (h->extended_coding) {
    switch (h->extension_audio_descriptor) {
      case dts_header_s::extension_xch:
        mxinfo("Extra Channels");
        break;
      case dts_header_s::extension_x96k:
        mxinfo("Extended frequency (x96k)");
        break;
      case dts_header_s::extension_xch_x96k:
        mxinfo("Extra Channels and Extended frequency (x96k)");
        break;
      default:
        mxinfo("yes, but unknown");
        break;
    }
  } else
    mxinfo("no");
  mxinfo("\n");

  mxinfo("Audio Sync in sub-subs : %s\n",
         (h->audio_sync_word_in_sub_sub)? "yes" : "no");

  mxinfo("Low Frequency Effects  : ");
  switch (h->lfe_type) {
    case dts_header_s::lfe_none:
      mxinfo("none");
      break;
    case dts_header_s::lfe_128:
      mxinfo("yes, interpolation factor 128");
      break;
    case dts_header_s::lfe_64:
      mxinfo("yes, interpolation factor 64");
      break;
    case dts_header_s::lfe_invalid:
      mxinfo("Invalid");
      break;
  }
  mxinfo("\n");

  mxinfo("Predictor History used : %s\n",
         (h->predictor_history_flag)? "yes" : "no");

  mxinfo("Multirate Interpolator : %s\n",
         (h->multirate_interpolator == dts_header_s::mi_non_perfect)?
         "non perfect" : "perfect");

  mxinfo("Encoder Software Vers. : %u\n",
         h->encoder_software_revision);
  mxinfo("Copy History Bits      : %u\n", h->copy_history);
  mxinfo("Source PCM Resolution  : %d\n", h->source_pcm_resolution);

  mxinfo("Front Encoded as Diff. : %s\n",
         (h->front_sum_difference)? "yes" : "no");
  mxinfo("Surr. Encoded as Diff. : %s\n",
         (h->surround_sum_difference)? "yes" : "no");

  mxinfo("Dialog Normaliz. Gain  : %d\n",
         h->dialog_normalization_gain);
}

void
dts_14_to_dts_16(const unsigned short *src,
                 unsigned long srcwords,
                 unsigned short *dst) {
  // srcwords has to be a multiple of 8!
  // you will get (srcbytes >> 3)*7 destination words!

  const unsigned long l = srcwords >> 3;

  for (unsigned long b = 0; b < l; b++) {
    unsigned short src_0 = (src[0]>>8) | (src[0]<<8);
    unsigned short src_1 = (src[1]>>8) | (src[1]<<8);
    // 14 + 2
    unsigned short dst_0 = (src_0 << 2)   | ((src_1 & 0x3fff) >> 12);
    dst[0] = (dst_0>>8) | (dst_0<<8);
    // 12 + 4
    unsigned short src_2 = (src[2]>>8) | (src[2]<<8);
    unsigned short dst_1 = (src_1 << 4)  | ((src_2 & 0x3fff) >> 10);
    dst[1] = (dst_1>>8) | (dst_1<<8);
    // 10 + 6
    unsigned short src_3 = (src[3]>>8) | (src[3]<<8);
    unsigned short dst_2 = (src_2 << 6)  | ((src_3 & 0x3fff) >> 8);
    dst[2] = (dst_2>>8) | (dst_2<<8);
    // 8  + 8
    unsigned short src_4 = (src[4]>>8) | (src[4]<<8);
    unsigned short dst_3 = (src_3 << 8)  | ((src_4 & 0x3fff) >> 6);
    dst[3] = (dst_3>>8) | (dst_3<<8);
    // 6  + 10
    unsigned short src_5 = (src[5]>>8) | (src[5]<<8);
    unsigned short dst_4 = (src_4 << 10) | ((src_5 & 0x3fff) >> 4);
    dst[4] = (dst_4>>8) | (dst_4<<8);
    // 4  + 12
    unsigned short src_6 = (src[6]>>8) | (src[6]<<8);
    unsigned short dst_5 = (src_5 << 12) | ((src_6 & 0x3fff) >> 2);
    dst[5] = (dst_5>>8) | (dst_5<<8);
    // 2  + 14
    unsigned short src_7 = (src[7]>>8) | (src[7]<<8);
    unsigned short dst_6 = (src_6 << 14) | ((src_7 & 0x3fff) >> 2);
    dst[6] = (dst_6>>8) | (dst_6<<8);

    dst += 7;
    src += 8;
  }
}
