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
 * helper function for AC3 data
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <string.h>

#include "ac3_common.h"
#include "common.h"

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

int
find_ac3_header(const unsigned char *buf,
                int size,
                ac3_header_t *ac3_header) {
  static int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112, 128, 160,
                       192, 224, 256, 320, 384, 448, 512, 576, 640};
  static unsigned char lfeon[8] = {0x10, 0x10, 0x04, 0x04, 0x04, 0x01,
                                   0x04, 0x01};
  static unsigned char halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
  ac3_header_t header;
  int i;
  int frmsizecod;
  int bitrate;
  int half;
  int acmod;

  for (i = 0; i < (size - 7); i++) {
    if ((buf[i] != 0x0b) || (buf[i + 1] != 0x77))
      continue;
    if (buf[i + 5] >= 0x60)
      continue;
    half = halfrate[buf[i + 5] >> 3];
    acmod = buf[i + 6] >> 5;
    header.flags = ((((buf[i + 6] & 0xf8) == 0x50) ? A52_DOLBY : acmod) |
                     ((buf[i + 6] & lfeon[acmod]) ? A52_LFE : 0));
    frmsizecod = buf[i + 4] & 63;
    if (frmsizecod >= 38)
      return -1;
    bitrate = rate[frmsizecod >> 1];
    header.bit_rate = (bitrate * 1000) >> half;
    switch (buf[i + 4] & 0xc0) {
      case 0:
        header.sample_rate = 48000 >> half;
        header.bytes = 4 * bitrate;
        break;
      case 0x40:
        header.sample_rate = 44100 >> half;
        header.bytes = 2 * (320 * bitrate / 147 + (frmsizecod & 1));
        break;
      case 0x80:
        header.sample_rate = 32000 >> half;
        header.bytes = 6 * bitrate;
        break;
      default:
        return -1;
    }
    switch (header.flags & A52_CHANNEL_MASK) {
      case A52_MONO:
        header.channels=1;
        break;
      case A52_CHANNEL:
      case A52_STEREO:
      case A52_CHANNEL1:
      case A52_CHANNEL2:
      case A52_DOLBY:
        header.channels=2;
        break;
      case A52_2F1R:
      case A52_3F:
        header.channels=3;
        break;
      case A52_3F1R:
      case A52_2F2R:
        header.channels=4;
        break;
      case A52_3F2R:
        header.channels=5;
        break;
    }
    if (header.flags & A52_LFE)
      header.channels++;
    header.bsid = (buf[i + 5] >> 3);
    memcpy(ac3_header, &header, sizeof(ac3_header_t));

    return i;
  }

  return -1;
}

int
find_consecutive_ac3_headers(const unsigned char *buf,
                             int size,
                             int num) {
  int i, pos, base, offset;
  ac3_header_t ac3header, new_header;

  pos = find_ac3_header(buf, size, &ac3header);
  if (pos < 0)
    return -1;
  mxverb(2, "ac3_reader: Found tag at %d size %d\n", pos, ac3header.bytes);
  base = pos + 1;

  if (num == 1)
    return pos;

  do {
    mxverb(2, "find_cons_ac3_h: starting with base at %d\n", base);
    offset = ac3header.bytes;
    for (i = 0; i < (num - 1); i++) {
      if ((size - base - offset) < 4)
        break;
      pos = find_ac3_header(&buf[base + offset], size - base - offset,
                            &new_header);
      if (pos == 0) {
        if ((new_header.bsid == ac3header.bsid) &&
            (new_header.channels == ac3header.channels) &&
            (new_header.sample_rate == ac3header.sample_rate)) {
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
    base++;
    offset = 0;
    pos = find_ac3_header(&buf[base], size - base, &ac3header);
    if (pos == -1)
      return -1;
    base += pos;
  } while (base < (size - 5));

  return -1;
}
