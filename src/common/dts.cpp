/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper function for DTS data

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <stdio.h>

#include "common/bit_cursor.h"
#include "common/dts.h"
#include "common/endian.h"

// ---------------------------------------------------------------------------

struct channel_arrangement {
  int num_channels;
  const char * description;
};

static const channel_arrangement channel_arrangements[16] = {
  { 1, "A (mono)"                                                                                                                                    },
  { 2, "A, B (dual mono)"                                                                                                                            },
  { 2, "L, R (left, right)"                                                                                                                          },
  { 2, "L+R, L-R (sum, difference)"                                                                                                                  },
  { 2, "LT, RT (left and right total)"                                                                                                               },
  { 3, "C, L, R (center, left, right)"                                                                                                               },
  { 3, "L, R, S (left, right, surround)"                                                                                                             },
  { 4, "C, L, R, S (center, left, right, surround)"                                                                                                  },
  { 4, "L, R, SL, SR (left, right, surround-left, surround-right)"                                                                                   },
  { 5, "C, L, R, SL, SR (center, left, right, surround-left, surround-right)"                                                                        },
  { 6, "CL, CR, L, R, SL, SR (center-left, center-right, left, right, surround-left, surround-right)"                                                },
  { 6, "C, L, R, LR, RR, OV (center, left, right, left-rear, right-rear, overhead)"                                                                  },
  { 6, "CF, CR, LF, RF, LR, RR  (center-front, center-rear, left-front, right-front, left-rear, right-rear)"                                         },
  { 7, "CL, C, CR, L, R, SL, SR  (center-left, center, center-right, left, right, surround-left, surround-right)"                                    },
  { 8, "CL, CR, L, R, SL1, SL2, SR1, SR2 (center-left, center-right, left, right, surround-left1, surround-left2, surround-right1, surround-right2)" },
  { 8, "CL, C, CR, L, R, SL, S, SR (center-left, center, center-right, left, right, surround-left, surround, surround-right)"                        },
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
    spr_16 = 0
  , spr_16_ES  //_ES means: surround channels mastered in DTS-ES
  , spr_20
  , spr_20_ES
  , spr_invalid4
  , spr_24_ES
  , spr_24
  , spr_invalid7
};

int
find_dts_sync_word(const unsigned char *buf,
                   unsigned int size) {
  if (4 > size)
    // not enough data for one header
    return -1;

  unsigned int offset = 0;
  uint32_t sync_word  = get_uint32_be(buf);
  while ((DTS_HEADER_MAGIC != sync_word) && ((offset + 4) < size)) {
    sync_word = (sync_word << 8) | buf[offset + 4];
    ++offset;
  }

  if (DTS_HEADER_MAGIC != sync_word)
    // no header found
    return -1;

  return offset;
}

int
find_dts_header_internal(const unsigned char *buf,
                         unsigned int size,
                         struct dts_header_s *dts_header,
                         bool allow_no_hd_search) {
  unsigned int size_to_search = size - 15;
  if (size_to_search > size) {
    // not enough data for one header
    return -1;
  }

  int offset = find_dts_sync_word(buf, size);
  if (0 > offset)
    // no header found
    return -1;

  bit_reader_c bc(buf + offset + 4, size - offset - 4);

  dts_header->frametype             = bc.get_bit() ? dts_header_s::FRAMETYPE_NORMAL : dts_header_s::FRAMETYPE_TERMINATION;
  dts_header->deficit_sample_count  = (bc.get_bits(5) + 1) % 32;
  dts_header->crc_present           = bc.get_bit();
  dts_header->num_pcm_sample_blocks = bc.get_bits(7) + 1;
  dts_header->frame_byte_size       = bc.get_bits(14) + 1;

  if (96 > dts_header->frame_byte_size) {
    mxwarn(Y("DTS_Header problem: invalid frame bytes size\n"));
    return -1;
  }

  int t = bc.get_bits(6);
  if (16 <= t) {
    dts_header->audio_channels            = -1;
    dts_header->audio_channel_arrangement = "unknown (user defined)";
  } else {
    dts_header->audio_channels            = channel_arrangements[t].num_channels;
    dts_header->audio_channel_arrangement = channel_arrangements[t].description;
  }

  dts_header->core_sampling_frequency    = core_samplefreqs[bc.get_bits(4)];
  dts_header->transmission_bitrate       = transmission_bitrates[bc.get_bits(5)];
  dts_header->embedded_down_mix          = bc.get_bit();
  dts_header->embedded_dynamic_range     = bc.get_bit();
  dts_header->embedded_time_stamp        = bc.get_bit();
  dts_header->auxiliary_data             = bc.get_bit();
  dts_header->hdcd_master                = bc.get_bit();
  dts_header->extension_audio_descriptor = static_cast<dts_header_s::extension_audio_descriptor_e>(bc.get_bits(3));
  dts_header->extended_coding            = bc.get_bit();
  dts_header->audio_sync_word_in_sub_sub = bc.get_bit();
  dts_header->lfe_type                   = static_cast<dts_header_s::lfe_type_e>(bc.get_bits(2));
  dts_header->predictor_history_flag     = bc.get_bit();

  if (dts_header->crc_present)
     bc.skip_bits(16);

  dts_header->multirate_interpolator     = static_cast<dts_header_s::multirate_interpolator_e>(bc.get_bit());
  dts_header->encoder_software_revision  = bc.get_bits(4);
  dts_header->copy_history               = bc.get_bits(2);

  switch (bc.get_bits(3)) {
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
      mxwarn(Y("DTS_Header problem: invalid source PCM resolution\n"));
      return -1;
  }

  dts_header->front_sum_difference      = bc.get_bit();
  dts_header->surround_sum_difference   = bc.get_bit();
  t                                     = bc.get_bits(4);
  dts_header->dialog_normalization_gain = 7 == dts_header->encoder_software_revision ? -t
                                        : 6 == dts_header->encoder_software_revision ? -16 - t
                                        :                                              0;

  // Detect DTS HD master audio / high resolution part
  dts_header->dts_hd       = false;
  dts_header->hd_type      = dts_header_t::DTSHD_NONE;
  dts_header->hd_part_size = 0;

  size_t hd_offset         = offset + dts_header->frame_byte_size;

  if ((hd_offset + 9) > size)
    return allow_no_hd_search ? offset : -1;

  if (get_uint32_be(buf + hd_offset) != DTS_HD_HEADER_MAGIC)
    return offset;

  dts_header->dts_hd = true;

  bc.init(buf + hd_offset, size - hd_offset);

  bc.skip_bits(32);             // DTS_HD_HEADER_MAGIC
  bc.skip_bits(8 + 2);          // ??
  if (bc.get_bit()) {           // Blown-up header bit
    bc.skip_bits(12);
    dts_header->hd_part_size = bc.get_bits(20) + 1;

  } else {
    bc.skip_bits(8);
    dts_header->hd_part_size = bc.get_bits(16) + 1;
  }

  dts_header->frame_byte_size += dts_header->hd_part_size;

  return offset;
}

int
find_dts_header(const unsigned char *buf,
                unsigned int size,
                struct dts_header_s *dts_header,
                bool allow_no_hd_search) {
  try {
    return find_dts_header_internal(buf, size, dts_header, allow_no_hd_search);
  } catch (...) {
    mxwarn(Y("DTS_Header problem: not enough data to read header\n"));
    return -1;
  }
}

int
find_consecutive_dts_headers(const unsigned char *buf,
                             unsigned int size,
                             unsigned int num) {
  static auto s_debug = debugging_option_c{"dts_detection"};

  dts_header_s dts_header, new_header;

  int pos = find_dts_header(buf, size, &dts_header, false);

  if (0 > pos)
    return -1;

  if (1 == num)
    return pos;

  unsigned int base = pos;

  do {
    mxdebug_if(s_debug, boost::format("find_cons_dts_h: starting with base at %1%\n") % base);

    int offset = dts_header.frame_byte_size;
    unsigned int i;
    for (i = 0; (num - 1) > i; ++i) {
      if (size < (2 + base + offset))
        break;

      pos = find_dts_header(&buf[base + offset], size - base - offset, &new_header, false);
      if (0 == pos) {
        if (new_header == dts_header) {
          mxdebug_if(s_debug, boost::format("find_cons_dts_h: found good header %1%\n") % i);
          offset += new_header.frame_byte_size;
          continue;
        } else
          break;
      } else
        break;
    }

    if (i == (num - 1))
      return base;

    ++base;
    offset = 0;
    pos    = find_dts_header(&buf[base], size - base, &dts_header, false);

    if (-1 == pos)
      return -1;

    base += pos;
  } while (base < (size - 5));

  return -1;
}

bool
operator ==(const dts_header_s &h1,
            const dts_header_s &h2) {
  return (h1.core_sampling_frequency                == h2.core_sampling_frequency)
      && (h1.lfe_type                               == h2.lfe_type)
      && (h1.audio_channels                         == h2.audio_channels)
      && (get_dts_packet_length_in_nanoseconds(&h1) == get_dts_packet_length_in_nanoseconds(&h2))
    ;
}

// ============================================================================

void
print_dts_header(const struct dts_header_s *h) {
  mxinfo("DTS Frame Header Information:\n");

  mxinfo("Frame Type             : ");
  if (h->frametype == dts_header_s::FRAMETYPE_NORMAL) {
    mxinfo("normal");
  } else {
    mxinfo(boost::format("termination, deficit sample count = %1%") % h->deficit_sample_count);
  }
  mxinfo("\n");

  mxinfo(boost::format("CRC available          : %1%\n") % (h->crc_present ? "yes" : "no"));

  mxinfo(boost::format("Frame Size             : PCM core samples=32*%1%=%2%, %3% milliseconds, %4% byte\n")
         % h->num_pcm_sample_blocks % (h->num_pcm_sample_blocks * 32) % ((h->num_pcm_sample_blocks * 32000.0) / h->core_sampling_frequency) % h->frame_byte_size);

  mxinfo(boost::format("Audio Channels         : %1%%2%, arrangement: %3%\n")
         % h->audio_channels % (h->source_surround_in_es ? " ES" : "") % h->audio_channel_arrangement);

  mxinfo(boost::format("Core sampling frequency: %1%\n") % h->core_sampling_frequency);

  if ((-1 < h->transmission_bitrate) || (-3 > h->transmission_bitrate))
    mxinfo(boost::format("Transmission bitrate   : %1%\n") % h->transmission_bitrate);
  else
    mxinfo(boost::format("Transmission_bitrate   : %1%\n")
           % (  h->transmission_bitrate == -1 ? "open"
              : h->transmission_bitrate == -2 ? "variable"
              :                                 "lossless"));

  mxinfo(boost::format("Embedded Down Mix      : %1%\n") % (h->embedded_down_mix      ? "yes" : "no"));
  mxinfo(boost::format("Embedded Dynamic Range : %1%\n") % (h->embedded_dynamic_range ? "yes" : "no"));
  mxinfo(boost::format("Embedded Time Stamp    : %1%\n") % (h->embedded_time_stamp    ? "yes" : "no"));
  mxinfo(boost::format("Embedded Auxiliary Data: %1%\n") % (h->auxiliary_data         ? "yes" : "no"));
  mxinfo(boost::format("HDCD Master            : %1%\n") % (h->hdcd_master            ? "yes" : "no"));

  mxinfo("Extended Coding        : ");
  if (h->extended_coding) {
    switch (h->extension_audio_descriptor) {
      case dts_header_s::EXTENSION_XCH:
        mxinfo("Extra Channels");
        break;
      case dts_header_s::EXTENSION_X96K:
        mxinfo("Extended frequency (x96k)");
        break;
      case dts_header_s::EXTENSION_XCH_X96K:
        mxinfo("Extra Channels and Extended frequency (x96k)");
        break;
      default:
        mxinfo("yes, but unknown");
        break;
    }
  } else
    mxinfo("no");
  mxinfo("\n");

  mxinfo(boost::format("Audio Sync in sub-subs : %1%\n") % (h->audio_sync_word_in_sub_sub ? "yes" : "no"));

  mxinfo("Low Frequency Effects  : ");
  switch (h->lfe_type) {
    case dts_header_s::LFE_NONE:
      mxinfo("none");
      break;
    case dts_header_s::LFE_128:
      mxinfo("yes, interpolation factor 128");
      break;
    case dts_header_s::LFE_64:
      mxinfo("yes, interpolation factor 64");
      break;
    case dts_header_s::LFE_INVALID:
      mxinfo("Invalid");
      break;
  }
  mxinfo("\n");

  mxinfo(boost::format("Predictor History used : %1%\n") % (h->predictor_history_flag ? "yes" : "no"));

  mxinfo(boost::format("Multirate Interpolator : %1%\n") % (h->multirate_interpolator == dts_header_s::MI_NON_PERFECT ? "non perfect" : "perfect"));

  mxinfo(boost::format("Encoder Software Vers. : %1%\n") % h->encoder_software_revision);
  mxinfo(boost::format("Copy History Bits      : %1%\n") % h->copy_history);
  mxinfo(boost::format("Source PCM Resolution  : %1%\n") % h->source_pcm_resolution);
  mxinfo(boost::format("Front Encoded as Diff. : %1%\n") % (h->front_sum_difference    ? "yes" : "no"));
  mxinfo(boost::format("Surr. Encoded as Diff. : %1%\n") % (h->surround_sum_difference ? "yes" : "no"));
  mxinfo(boost::format("Dialog Normaliz. Gain  : %1%\n") % h->dialog_normalization_gain);

  if (!h->dts_hd)
    mxinfo("DTS HD                 : no\n");
  else
    mxinfo(boost::format("DTS HD                 : %1%, size %2%\n")
           % (h->hd_type == dts_header_t::DTSHD_MASTER_AUDIO ? "master audio" : "high resolution") % h->hd_part_size);
}

void
dts_14_to_dts_16(const unsigned short *src,
                 unsigned long srcwords,
                 unsigned short *dst) {
  // srcwords has to be a multiple of 8!
  // you will get (srcbytes >> 3)*7 destination words!

  const unsigned long l = srcwords >> 3;

  for (unsigned long b = 0; b < l; b++) {
    unsigned short src_0 = (src[0] >>  8) | (src[0] << 8);
    unsigned short src_1 = (src[1] >>  8) | (src[1] << 8);
    // 14 + 2
    unsigned short dst_0 = (src_0  <<  2) | ((src_1 & 0x3fff) >> 12);
    dst[0]               = (dst_0  >>  8) | (dst_0            <<  8);
    // 12 + 4
    unsigned short src_2 = (src[2] >>  8) | (src[2]           <<  8);
    unsigned short dst_1 = (src_1  <<  4) | ((src_2 & 0x3fff) >> 10);
    dst[1]               = (dst_1  >>  8) | (dst_1            <<  8);
    // 10 + 6
    unsigned short src_3 = (src[3] >>  8) | (src[3]           <<  8);
    unsigned short dst_2 = (src_2  <<  6) | ((src_3 & 0x3fff) >>  8);
    dst[2]               = (dst_2  >>  8) | (dst_2            <<  8);
    // 8  + 8
    unsigned short src_4 = (src[4] >>  8) | (src[4]           <<  8);
    unsigned short dst_3 = (src_3  <<  8) | ((src_4 & 0x3fff) >>  6);
    dst[3]               = (dst_3  >>  8) | (dst_3            <<  8);
    // 6  + 10
    unsigned short src_5 = (src[5] >>  8) | (src[5]           <<  8);
    unsigned short dst_4 = (src_4  << 10) | ((src_5 & 0x3fff) >>  4);
    dst[4]               = (dst_4  >>  8) | (dst_4            <<  8);
    // 4  + 12
    unsigned short src_6 = (src[6] >>  8) | (src[6]           <<  8);
    unsigned short dst_5 = (src_5  << 12) | ((src_6 & 0x3fff) >>  2);
    dst[5]               = (dst_5  >>  8) | (dst_5            <<  8);
    // 2  + 14
    unsigned short src_7 = (src[7] >>  8) | (src[7]           <<  8);
    unsigned short dst_6 = (src_6  << 14) | (src_7 & 0x3fff);
    dst[6]               = (dst_6  >>  8) | (dst_6            <<  8);

    dst                 += 7;
    src                 += 8;
  }
}

bool
detect_dts(const void *src_buf,
           int len,
           bool &dts14_to_16,
           bool &swap_bytes) {
  dts_header_t dtsheader;
  int dts_swap_bytes     = 0;
  int dts_14_16          = 0;
  bool is_dts            = false;
  len                   &= ~0xf;
  int cur_buf            = 0;
  memory_cptr af_buf[2]  = { memory_c::alloc(len),                                        memory_c::alloc(len)                                        };
  unsigned short *buf[2] = { reinterpret_cast<unsigned short *>(af_buf[0]->get_buffer()), reinterpret_cast<unsigned short *>(af_buf[1]->get_buffer()) };

  for (dts_swap_bytes = 0; 2 > dts_swap_bytes; ++dts_swap_bytes) {
    memcpy(buf[cur_buf], src_buf, len);

    if (dts_swap_bytes) {
      swab((char *)buf[cur_buf], (char *)buf[cur_buf^1], len);
      cur_buf ^= 1;
    }

    for (dts_14_16 = 0; 2 > dts_14_16; ++dts_14_16) {
      if (dts_14_16) {
        dts_14_to_dts_16(buf[cur_buf], len / 2, buf[cur_buf^1]);
        cur_buf ^= 1;
      }

      int dst_buf_len = dts_14_16 ? (len * 7 / 8) : len;

      if (find_dts_header(af_buf[cur_buf]->get_buffer(), dst_buf_len, &dtsheader) >= 0) {
        is_dts = true;
        break;
      }
    }

    if (is_dts)
      break;
  }

  dts14_to_16 = dts_14_16      != 0;
  swap_bytes  = dts_swap_bytes != 0;

  return is_dts;
}

bool
operator!=(const dts_header_t &l,
           const dts_header_t &r) {
  //if (l.frametype != r.frametype) return true;
  //if (l.deficit_sample_count != r.deficit_sample_count) return true;
  if (   (l.crc_present                        != r.crc_present)
      || (l.num_pcm_sample_blocks              != r.num_pcm_sample_blocks)
      || ((l.frame_byte_size - l.hd_part_size) != (r.frame_byte_size - r.hd_part_size))
      || (l.audio_channels                     != r.audio_channels)
      || (l.core_sampling_frequency            != r.core_sampling_frequency)
      || (l.transmission_bitrate               != r.transmission_bitrate)
      || (l.embedded_down_mix                  != r.embedded_down_mix)
      || (l.embedded_dynamic_range             != r.embedded_dynamic_range)
      || (l.embedded_time_stamp                != r.embedded_time_stamp)
      || (l.auxiliary_data                     != r.auxiliary_data)
      || (l.hdcd_master                        != r.hdcd_master)
      || (l.extension_audio_descriptor         != r.extension_audio_descriptor)
      || (l.extended_coding                    != r.extended_coding)
      || (l.audio_sync_word_in_sub_sub         != r.audio_sync_word_in_sub_sub)
      || (l.lfe_type                           != r.lfe_type)
      || (l.predictor_history_flag             != r.predictor_history_flag)
      || (l.multirate_interpolator             != r.multirate_interpolator)
      || (l.encoder_software_revision          != r.encoder_software_revision)
      || (l.copy_history                       != r.copy_history)
      || (l.source_pcm_resolution              != r.source_pcm_resolution)
      || (l.source_surround_in_es              != r.source_surround_in_es)
      || (l.front_sum_difference               != r.front_sum_difference)
      || (l.surround_sum_difference            != r.surround_sum_difference)
      || (l.dialog_normalization_gain          != r.dialog_normalization_gain))
    return true;

  return false;
}
