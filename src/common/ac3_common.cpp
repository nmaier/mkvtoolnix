/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   helper function for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <string.h>

#include "ac3_common.h"
#include "common.h"

/*
  EAC3 Header:
  AAAAAAAA AAAAAAAA BBCCCDDD DDDDDDDD EEFFGGGH IIIII...
  A = sync, always 0x0B77
  B = stream type
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

  int acmod          = (buf[4] >> 1) & 0x07;
  int lfeon          =  buf[4]       & 0x01;

  header.sample_rate = sample_rates[0x03 == fscod ? 3 + fscod2 : fscod];
  header.bytes       = (((buf[2] & 0x03) << 8) + buf[3] + 1) * 2;
  header.channels    = channels[acmod] + lfeon;
  header.samples     = (0x03 == fscod) ? 1536 : samples[fscod2];

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

static bool
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
  header.flags = ((((buf[6] & 0xf8) == 0x50) ? A52_DOLBY : acmod) | ((buf[6] & lfeon[acmod]) ? A52_LFE : 0));

  switch (header.flags & A52_CHANNEL_MASK) {
    case A52_MONO:
      header.channels = 1;
      break;

    case A52_CHANNEL:
    case A52_STEREO:
    case A52_CHANNEL1:
    case A52_CHANNEL2:
    case A52_DOLBY:
      header.channels = 2;
      break;

    case A52_2F1R:
    case A52_3F:
      header.channels = 3;
      break;

    case A52_3F1R:
    case A52_2F2R:
      header.channels = 4;
      break;

    case A52_3F2R:
      header.channels = 5;
      break;
  }

  if (header.flags & A52_LFE)
    header.channels++;

  header.samples = 1536;

  return true;
}

int
find_ac3_header(const unsigned char *buf,
                int size,
                ac3_header_t *ac3_header) {
  ac3_header_t header;
  int i;

  for (i = 0; (size - 7) > i; ++i) {
    if ((0x0b != buf[i]) || (0x77 != buf[i + 1]))
      continue;

    header.bsid = (buf[i + 5] >> 3);
    bool found  = false;

    if (0x10 == header.bsid)
      found = parse_eac3_header(&buf[i], header);

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
                             int size,
                             int num) {
  ac3_header_t ac3header, new_header;

  int pos = find_ac3_header(buf, size, &ac3header);
  if (0 > pos)
    return -1;

  mxverb(2, "ac3_reader: Found tag at %d size %d\n", pos, ac3header.bytes);

  if (1 == num)
    return pos;

  int base = pos;
  do {
    mxverb(2, "find_cons_ac3_h: starting with base at %d\n", base);

    int offset = ac3header.bytes;
    int i;
    for (i = 0; (num - 1) > i; ++i) {
      if ((size - base - offset) < 4)
        break;

      pos = find_ac3_header(&buf[base + offset], size - base - offset, &new_header);

      if (0 == pos) {
        if (   (new_header.bsid        == ac3header.bsid)
            && (new_header.channels    == ac3header.channels)
            && (new_header.sample_rate == ac3header.sample_rate)) {
          mxverb(2, "find_cons_ac3_h: found good header %d\n", i);
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
    pos    = find_ac3_header(&buf[base], size - base, &ac3header);

    if (-1 == pos)
      return -1;

    base += pos;

  } while ((size - 5) > base);

  return -1;
}
