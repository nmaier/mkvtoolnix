/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper function for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cstring>

#include "common/ac3.h"
#include "common/bswap.h"
#include "common/checksums.h"
#include "common/endian.h"

ac3_header_t::ac3_header_t() {
  memset(this, 0, sizeof(ac3_header_t));
}

/*
  EAC3 Header:
  AAAAAAAA AAAAAAAA BBCCCDDD DDDDDDDD EEFFGGGH IIIII...
  A = sync, always 0x0B77
  B = frame type
  C = sub stream id
  D = frame size - 1 in words
  E = fscod
  F = fscod2 if fscod == 3, else numblkscod
  G = acmod
  H = LEF on
  I = bitstream ID, 16 for EAC3 (same position and length as for AC3)
*/

static bool
parse_eac3_header(const unsigned char *buf,
                  ac3_header_t &header) {
  static const int sample_rates[] = { 48000, 44100, 32000, 24000, 22050, 16000 };
  static const int channels[]     = {     2,     1,     2,     3,     3,     4,     4,     5 };
  static const int samples[]      = {   256,   512,   768,  1536 };

  int fscod  =  buf[4] >> 6;
  int fscod2 = (buf[4] >> 4) & 0x03;

  if ((0x03 == fscod) && (0x03 == fscod2))
    return false;

  int acmod                   = (buf[4] >> 1) & 0x07;
  int lfeon                   =  buf[4]       & 0x01;

  header.frame_type           = (buf[2] >> 6) & 0x03;
  header.sub_stream_id        = (buf[2] >> 3) & 0x07;
  header.sample_rate          = sample_rates[0x03 == fscod ? 3 + fscod2 : fscod];
  header.bytes                = (((buf[2] & 0x03) << 8) + buf[3] + 1) * 2;
  header.channels             = channels[acmod] + lfeon;
  header.samples              = (0x03 == fscod) ? 1536 : samples[fscod2];

  header.has_dependent_frames = false;

  if (EAC3_FRAME_TYPE_RESERVED == header.frame_type)
    return false;

  return true;
}

static bool
parse_eac3_header_full(const unsigned char *buf,
                       size_t size,
                       ac3_header_t &header,
                       bool look_for_second_header) {
  if (!parse_eac3_header(buf, header))
    return false;

  if (!look_for_second_header)
    return true;

  if (((header.bytes + 5) > size) || (0x0b != buf[header.bytes]) || (0x77 != buf[header.bytes + 1]))
    return false;

  ac3_header_t second_header;
  if (!parse_eac3_header(buf + header.bytes, second_header))
    return false;

  if (EAC3_FRAME_TYPE_DEPENDENT == second_header.frame_type) {
    header.has_dependent_frames  = true;
    header.channels              = 8; // TODO: Don't hardcode 7.1, but get the values from the frames
    header.bytes                += second_header.bytes;
  }

  return true;
}

/*
  <S_O> AC3 Header:
  <S_O> AAAAAAAA AAAAAAAA BBBBBBBB BBBBBBBB CCDDDDDD EEEEEFFF
  <S_O> A = sync, always 0x0B77
  <S_O> B = CRC 16 of 5/8 frame
  <S_O> C = samplerate:
  <S_O>  if E <= 8: 00 = 48kHz; 01 = 44,1kHz; 10 = 32kHz; 11 = reserved
  <S_O>  if E = 9:  00 = 24kHz; 01 = 22,05kHz; 10 = 16kHz; 11 = reserved
  <S_O>  if E = 10: 00 = 12kHz; 01 = 11,025kHz; 10 = 8KHz; 11 = reserved
  <S_O> D = framesize code, 12/24KHz is like 48kHz, 8/16kHz like 32kHz etc.
  <S_O> E = bitstream ID, if <=8 compatible to all standard decoders
  <S_O>  9 and 10 = low samplerate additions
  <S_O> F = bitstream mode
*/

bool
parse_ac3_header(const unsigned char *buf,
                 ac3_header_t &header) {
  static const int rate[]                 = {   32,   40,   48,   56,   64,   80,   96,  112,  128,  160,  192,  224,  256,  320,  384,  448,  512,  576,  640 };
  static const unsigned char lfeon[8]     = { 0x10, 0x10, 0x04, 0x04, 0x04, 0x01, 0x04, 0x01 };
  static const unsigned char halfrate[12] = {    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    2,    3 };

  int frmsizecod = buf[4] & 63;
  if (38 <= frmsizecod)
    return false;

  int half        = halfrate[buf[5] >> 3];
  int bitrate     = rate[frmsizecod >> 1];
  header.bit_rate = (bitrate * 1000) >> half;

  switch (buf[4] & 0xc0) {
    case 0:
      header.sample_rate = 48000 >> half;
      header.bytes       = 4 * bitrate;
      break;

    case 0x40:
      header.sample_rate = 44100 >> half;
      header.bytes       = 2 * (320 * bitrate / 147 + (frmsizecod & 1));
      break;

    case 0x80:
      header.sample_rate = 32000 >> half;
      header.bytes       = 6 * bitrate;
      break;

    default:
      return false;
  }

  int acmod    = buf[6] >> 5;
  header.flags = ((((buf[6] & 0xf8) == 0x50) ? AC3_DOLBY : acmod) | ((buf[6] & lfeon[acmod]) ? AC3_LFE : 0));

  switch (header.flags & AC3_CHANNEL_MASK) {
    case AC3_MONO:
      header.channels = 1;
      break;

    case AC3_CHANNEL:
    case AC3_STEREO:
    case AC3_CHANNEL1:
    case AC3_CHANNEL2:
    case AC3_DOLBY:
      header.channels = 2;
      break;

    case AC3_2F1R:
    case AC3_3F:
      header.channels = 3;
      break;

    case AC3_3F1R:
    case AC3_2F2R:
      header.channels = 4;
      break;

    case AC3_3F2R:
      header.channels = 5;
      break;
  }

  if (header.flags & AC3_LFE)
    header.channels++;

  header.samples              = 1536;
  header.has_dependent_frames = false;

  return true;
}

int
find_ac3_header(const unsigned char *buf,
                size_t size,
                ac3_header_t *ac3_header,
                bool look_for_second_header) {
  ac3_header_t header;
  size_t i;

  for (i = 0; (size - 7) > i; ++i) {
    if ((0x0b != buf[i]) || (0x77 != buf[i + 1]))
      continue;

    header.bsid = (buf[i + 5] >> 3);
    bool found  = false;

    if (0x10 == header.bsid)
      found = parse_eac3_header_full(&buf[i], size, header, look_for_second_header);

    else if (0x0c <= header.bsid)
      continue;

    else
      found = parse_ac3_header(&buf[i], header);

    if (found) {
      memcpy(ac3_header, &header, sizeof(ac3_header_t));
      return i;
    }
  }

  return -1;
}

int
find_consecutive_ac3_headers(const unsigned char *buf,
                             size_t size,
                             unsigned int num) {
  ac3_header_t ac3header, new_header;

  int pos = find_ac3_header(buf, size, &ac3header, true);
  if (0 > pos)
    return -1;

  mxverb(4, boost::format("ac3_reader: Found tag at %1% size %2%\n") % pos % ac3header.bytes);

  if (1 == num)
    return pos;

  unsigned int base = pos;
  do {
    mxverb(4, boost::format("find_cons_ac3_h: starting with base at %1%\n") % base);

    unsigned int offset = ac3header.bytes;
    size_t i;
    for (i = 0; (num - 1) > i; ++i) {
      if ((size - base - offset) < 4)
        break;

      pos = find_ac3_header(&buf[base + offset], size - base - offset, &new_header, ac3header.has_dependent_frames);

      if (0 == pos) {
        if (   (new_header.bsid        == ac3header.bsid)
            && (new_header.channels    == ac3header.channels)
            && (new_header.sample_rate == ac3header.sample_rate)) {
          mxverb(4, boost::format("find_cons_ac3_h: found good header %1%\n") % i);
          offset += new_header.bytes;
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
    pos    = find_ac3_header(&buf[base], size - base, &ac3header, true);

    if (-1 == pos)
      return -1;

    base += pos;

  } while ((size - 5) > base);

  return -1;
}

/*
   The functions mul_poly, pow_poly and verify_ac3_crc were taken
   or adopted from the ffmpeg project, file "libavcodec/ac3enc.c".

   The license here is the GPL.
 */

#define CRC16_POLY ((1 << 0) | (1 << 2) | (1 << 15) | (1 << 16))

static unsigned int
mul_poly(unsigned int a,
         unsigned int b,
         unsigned int poly) {
  unsigned int c;

  c = 0;
  while (a) {
    if (a & 1)
      c ^= b;
    a = a >> 1;
    b = b << 1;
    if (b & (1 << 16))
      b ^= poly;
  }
  return c;
}

static unsigned int
pow_poly(unsigned int a,
         unsigned int n,
         unsigned int poly) {
  unsigned int r;
  r = 1;
  while (n) {
    if (n & 1)
      r = mul_poly(r, a, poly);
    a = mul_poly(a, a, poly);
    n >>= 1;
  }
  return r;
}

bool
verify_ac3_checksum(const unsigned char *buf,
                    size_t size) {
  ac3_header_t ac3_header;

  if (0 != find_ac3_header(buf, size, &ac3_header, false))
    return false;

  if (size < ac3_header.bytes)
    return false;

  uint16_t expected_crc = get_uint16_be(&buf[2]);

  int frame_size_words  = ac3_header.bytes >> 1;
  int frame_size_58     = (frame_size_words >> 1) + (frame_size_words >> 3);

  uint16_t actual_crc   = bswap_16(crc_calc(crc_get_table(CRC_16_ANSI), 0, buf + 4, 2 * frame_size_58 - 4));
  unsigned int crc_inv  = pow_poly((CRC16_POLY >> 1), (16 * frame_size_58) - 16, CRC16_POLY);
  actual_crc            = mul_poly(crc_inv, actual_crc, CRC16_POLY);

  return expected_crc == actual_crc;
}
