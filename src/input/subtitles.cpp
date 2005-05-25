/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   subtitle helper

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>

#include "common.h"
#include "subtitles.h"

void
subtitles_c::process(generic_packetizer_c *p) {
  if (empty() || (current == entries.end()))
    return;

  p->process(new packet_t(new memory_c((unsigned char *)current->subs.c_str(),
                                       0, false),
                          current->start, current->end - current->start));
  current++;
}

#undef PFX
#define PFX "spu_extract_duration: "

int64_t
spu_extract_duration(unsigned char *data,
                     int buf_size,
                     int64_t timecode) {
  uint32_t date, control_start, next_off, start_off, off;
  unsigned char type;
  int duration;
  bool unknown;

  control_start = get_uint16_be(data + 2);
  next_off = control_start;
  duration = -1;
  start_off = 0;

  while ((start_off != next_off) && (next_off < buf_size)) {
    start_off = next_off;
    date = get_uint16_be(data + start_off) * 1024;
    next_off = get_uint16_be(data + start_off + 2);
    if (next_off < start_off) {
      mxwarn(PFX "Encountered broken SPU packet (next_off < start_off) at "
             "timecode " FMT_TIMECODE ". This packet might be displayed "
             "incorrectly or not at all.\n", ARG_TIMECODE_NS(timecode));
      return -1;
    }
    mxverb(4, PFX "date = %u\n", date);
    off = start_off + 4;
    for (type = data[off++]; type != 0xff; type = data[off++]) {
      mxverb(4, PFX "cmd = %d ", type);
      unknown = false;
      switch(type) {
        case 0x00:
          /* Menu ID, 1 byte */
          mxverb(4, "menu ID");
          break;
        case 0x01:
          /* Start display */
          mxverb(4, "start display");
          break;
        case 0x02:
          /* Stop display */
          mxverb(4, "stop display: %u", date / 90);
          return (int64_t)date * 1000000 / 90;
          break;
        case 0x03:
          /* Palette */
          mxverb(4, "palette");
          off+=2;
          break;
        case 0x04:
          /* Alpha */
          mxverb(4, "alpha");
          off+=2;
          break;
        case 0x05:
          mxverb(4, "coords");
          off+=6;
          break;
        case 0x06:
          mxverb(4, "graphic lines");
          off+=4;
          break;
        case 0xff:
          /* All done, bye-bye */
          mxverb(4, "done");
          return duration;
        default:
          mxverb(4, "unknown (0x%02x), skipping %d bytes.", type,
                 next_off - off);
          unknown = true;
      }
      mxverb(4, "\n");
      if (unknown)
        break;
    }
  }
  return duration;
}
