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
 * MPEG4 video helper functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

#include "common.h"
#include "mpeg4_common.h"

bool
mpeg4_extract_par(const unsigned char *buffer,
                  int size,
                  uint32_t &par_num,
                  uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0,
                                0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1,
                                1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t marker, aspect_ratio_info, num, den;
  bool b;
  bit_cursor_c bits(buffer, size);

  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      break;

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    marker &= 0xff;
    if ((marker < 0x20) || (marker > 0x2f)) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(2, "mpeg4 AR: found VOL header at %u\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);

    // VOL header
    bits.skip_bits(1);          // random access
    bits.skip_bits(8);          // vo_type
    bits.get_bit(b);
    if (b != 0) {               // is_old_id
      bits.skip_bits(4);        // vo_ver_id
      bits.skip_bits(3);        // vo_priority
    }

    if (bits.get_bits(4, aspect_ratio_info)) {
      mxverb(2, "mpeg4 AR: aspect_ratio_info: %u\n", aspect_ratio_info);
      if (aspect_ratio_info == 15) { // ASPECT_EXTENDED
        bits.get_bits(8, num);
        bits.get_bits(8, den);
      } else {
        num = ar_nums[aspect_ratio_info];
        den = ar_dens[aspect_ratio_info];
      }
      mxverb(2, "mpeg4 AR: %u den: %u\n", num, den);

      if ((num != 0) && (den != 0) && ((num != 1) || (den != 1)) &&
          (((float)num / (float)den) != 1.0)) {
        par_num = num;
        par_den = den;
        return true;
      }
    }
    return false;
  }
  return false;
}

void
mpeg4_find_frame_types(unsigned char *buf,
                       int size,
                       vector<video_frame_t> &frames) {
  bit_cursor_c bits(buf, size);
  uint32_t marker, frame_type;
  bool first_frame;
  video_frame_t frame;
  vector<video_frame_t>::iterator fit;

  frame.pos = 0;
  frames.clear();
  first_frame = true;
  mxverb(3, "\nmpeg4_frames: start search in %d bytes\n", size);
  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      break;

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(3, "mpeg4_frames:   found start code at %d\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);
    if (marker == VOP_START_CODE) {
      if (!bits.get_bits(2, frame_type))
        break;
      if (!first_frame) {
        frame.size = (bits.get_bit_position() / 8) - 4 - frame.pos;
        frames.push_back(frame);
        frame.pos = (bits.get_bit_position() / 8) - 4;
      } else
        first_frame = false;
      frame.type = frame_type == 0 ? 'I' : frame_type == 1 ? 'P' :
        frame_type == 2 ? 'B' : 'S';
      bits.byte_align();
    }
  }

  if (!first_frame) {
    frame.size = size - frame.pos;
    frames.push_back(frame);
  }
  mxverb(2, "mpeg4_frames:   summary: found %d frames ", frames.size());
  for (fit = frames.begin(); fit < frames.end(); fit++)
    mxverb(2, "'%c' (%d at %d) ",
           fit->type, fit->size, fit->pos);
  mxverb(2, "\n");

  fit = frames.begin();
  while (fit < frames.end()) {
    if (fit->size < 10)      // dummy frame
      frames.erase(fit);
    else
      fit++;
  }
}
