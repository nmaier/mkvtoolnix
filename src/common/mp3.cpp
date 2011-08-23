/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for MP3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"
#include "common/mp3.h"

// Synch word for a frame is 0xFFE0 (first 11 bits must be set)
// Frame valuable information (for parsing) are stored in the first 4 bytes :

//   AAAAAAAA AAABBCCD EEEEFFGH IIJJKLMM
// - B : Mpeg version
//       x 00 = MPEG Version 2.5 (later extension of MPEG 2)
//       x 01 = reserved
//       x 10 = MPEG Version 2 (ISO/IEC 13818-3)
//       x 11 = MPEG Version 1 (ISO/IEC 11172-3)

// - C : Layer version
//       x 00 = reserved
//       x 01 = Layer III
//       x 10 = Layer II
//       x 11 = Layer I

// - D : Protection bit

// - E : Bitrate index (kbps)
//             |           MPEG v1           |  MPEG v2 & v2.5   |
//             |         |         |         |         | Layer-2 |
//             | Layer-1 | Layer-2 | Layer-3 | Layer-1 | Layer-3 |
//        -----+---------+---------+---------+---------+---------+
//        0000 |    free |    free |    free |    free |    free |
//        0001 |      32 |      32 |      32 |      32 |       8 |
//        0010 |      64 |      48 |      40 |      48 |      16 |
//        0011 |      96 |      56 |      48 |      56 |      24 |
//        0100 |     128 |      64 |      56 |      64 |      32 |
//        0101 |     160 |      80 |      64 |      80 |      40 |
//        0110 |     192 |      96 |      80 |      96 |      48 |
//        0111 |     224 |     112 |      96 |     112 |      56 |
//        1000 |     256 |     128 |     112 |     128 |      64 |
//        1001 |     288 |     160 |     128 |     144 |      80 |
//        1010 |     320 |     192 |     160 |     160 |      96 |
//        1011 |     352 |     224 |     192 |     176 |     112 |
//        1100 |     384 |     256 |     224 |     192 |     128 |
//        1101 |     416 |     320 |     256 |     224 |     144 |
//        1110 |     448 |     384 |     320 |     256 |     160 |
//        1111 |     bad |     bad |     bad |     bad |     bad |

// - F : Sampling rate frequency index
//             | MPEG v1 | MPEG v2 | MPEG v2.5
//          ---+---------+---------+----------
//          00 | 44100Hz | 22050Hz | 11025Hz
//          01 | 48000Hz | 24000Hz | 12000Hz
//          10 | 32000Hz | 16000Hz |  8000Hz
//          11 | Reserv. | Reserv. | Reserv.

// - G : Padding bit

// - H : Private bit

// - I : Channel mode

// - J : Mode extension

// - K : Copyright

// - L : Original

// - M : Emphasis

// Length (in bytes) of a frame :

// - Layer I :
//   Length = (12 * BitRate / SampleRate + Padding) * 4

// - Layer II :
//   Length = 144 * BitRate / SampleRate + Padding

// - Layer III :
//  x Mpeg v1 :
//   Length = 144 * BitRate / SampleRate + Padding
//  x Mpeg v2 & v2.5 :
//   Length = 72 * BitRate / SampleRate + Padding

// Size (number of samples - for each channel) of a frame :

//           |        MPEG        |
//           |  v1  |  v2  | v2.5 |
//   --------+------+------+------+
//   Layer-1 |  384 |  384 |  384 |
//   Layer-2 | 1152 | 1152 | 1152 |
//   Layer-3 | 1152 |  576 |  576 |
//   --------+------+------+------+

static int mp3_bitrates_mpeg1[3][16] = {
  {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
  {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
  {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}
};

static int mp3_bitrates_mpeg2[3][16] = {
  {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
  {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
  {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
};

static int mp3_sampling_freqs[3][4] = {
  {44100, 48000, 32000, 0},
  {22050, 24000, 16000, 0},
  {11025, 12000, 8000, 0}
};

static int mp3_samples_per_channel[3][3] = {
  {384, 1152, 1152},
  {384, 1152, 576},
  {384, 1152, 576}
};

int
find_mp3_header(const unsigned char *buf,
                int size) {
  int i, pos;
  unsigned long header;

  if (size < 4)
    return -1;

  for (pos = 0; pos < (size - 4); pos++) {
    if ((buf[pos] == 'I') && (buf[pos + 1] == 'D') && (buf[pos + 2] == '3')) {
      if ((pos + 10) >= size)
        return -1;

      return pos;
    }
    if ((buf[pos] == 'T') && (buf[pos + 1] == 'A') && (buf[pos + 2] == 'G'))
      return pos;

    for (i = 0, header = 0; i < 4; i++) {
      header <<= 8;
      header |= buf[i + pos];
    }

    if (header == FOURCC('R', 'I', 'F', 'F'))
      continue;

    if ((header & 0xffe00000) != 0xffe00000)
      continue;
    if (((header >> 17) & 3) == 0)
      continue;
    if (((header >> 12) & 0xf) == 0xf)
      continue;
    if (((header >> 12) & 0xf) == 0)
      continue;
    if (((header >> 10) & 0x3) == 0x3)
      continue;
    if (((header >> 19) & 3) == 0x1)
      continue;
    if ((header & 0xffff0000) == 0xfffe0000)
      continue;
    return pos;
  }
  return -1;
}

bool
decode_mp3_header(const unsigned char *buf,
                  mp3_header_t *h) {
  unsigned long header;
  int i;

  if ((buf[0] == 'I') && (buf[1] == 'D') && (buf[2] == '3')) {
    h->is_tag = true;
    h->framesize = 0;
    for (i = 6; i < 10; i++) {
      h->framesize <<= 7;
      h->framesize |= ((uint32_t)buf[i]) & 0x7f;
    }
    h->framesize += 10;
    if ((buf[3] >= 4) && ((buf[5] & 0x10) == 0x10))
      h->framesize += 10;

    return true;
  }

  if ((buf[0] == 'T') && (buf[1] == 'A') && (buf[2] == 'G')) {
    h->is_tag    = true;
    h->framesize = 128;
    return true;
  }

  for (i = 0, header = 0; i < 4; i++) {
    header <<= 8;
    header  |= buf[i];
  }

  h->version = (header >> 19) & 3;
  h->layer   = 4 - ((header >> 17) & 3);

  if (h->version == 1)
    return false;
  if (h->layer == 4)
    return false;

  h->is_tag              = false;
  h->version             = 0 == h->version ? 3 : 3 == h->version ? 1 : h->version;
  h->protection          = ((header >> 16) & 1) ^ 1;
  h->bitrate_index       = (header >> 12) & 15;
  h->bitrate             = 1 == h->version ? mp3_bitrates_mpeg1[h->layer - 1][h->bitrate_index] : mp3_bitrates_mpeg2[h->layer - 1][h->bitrate_index];
  h->sampling_frequency  = mp3_sampling_freqs[h->version - 1][(header >> 10) & 3];
  h->padding             = (header >> 9) & 1;
  h->is_private          = (header >> 8) & 1;
  h->channel_mode        = (header >> 6) & 3;
  h->channels            = 3 == h->channel_mode ? 1 : 2;
  h->mode_extension      = (header >> 4) & 3;
  h->copyright           = (header >> 3) & 1;
  h->original            = (header >> 2) & 1;
  h->emphasis            = header & 3;
  h->samples_per_channel = mp3_samples_per_channel[h->version - 1][h->layer - 1];

  if (h->layer == 3)
    h->framesize = (1 == h->version ? 144000 : 72000) * h->bitrate / h->sampling_frequency + h->padding;
  else if (h->layer == 2)
    h->framesize = 144000 * h->bitrate / h->sampling_frequency + h->padding;
  else
    h->framesize = (12000 * h->bitrate / h->sampling_frequency + h->padding) * 4;

  return true;
}

int
find_consecutive_mp3_headers(const unsigned char *buf,
                             int size,
                             int num) {
  int i, pos, base, offset;
  mp3_header_t mp3header, new_header;

  base = 0;
  do {
    // Let's find the first non-tag header.
    pos = find_mp3_header(&buf[base], size - base);
    if (pos < 0)
      return -1;
    if (decode_mp3_header(&buf[base + pos], &mp3header) && !mp3header.is_tag)
      break;
    mxverb(4, boost::format("mp3_reader: Found tag at %1% size %2%\n") % (base + pos) % mp3header.framesize);
    base += pos + 1;
  } while (true);

  if (num == 1)
    return pos;
  base += pos;

  do {
    mxverb(4, boost::format("find_cons_mp3_h: starting with base at %1%\n") % base);
    offset = mp3header.framesize;
    for (i = 0; i < (num - 1); i++) {
      if ((size - base - offset) < 4)
        break;
      pos = find_mp3_header(&buf[base + offset], size - base - offset);
      if ((pos == 0) && decode_mp3_header(&buf[base + offset], &new_header)) {
        if (   (new_header.version            == mp3header.version)
            && (new_header.layer              == mp3header.layer)
            && (new_header.channels           == mp3header.channels)
            && (new_header.sampling_frequency == mp3header.sampling_frequency)) {
          mxverb(4, boost::format("find_cons_mp3_h: found good header %1%\n") % i);
          offset += new_header.framesize;
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
    pos    = find_mp3_header(&buf[base], size - base);
    if (pos == -1)
      return -1;
    decode_mp3_header(&buf[base + pos], &mp3header);
    base += pos;
  } while (base < (size - 5));

  return -1;
}
