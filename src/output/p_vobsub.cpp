/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vobsub.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief VobSub packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <matroska/KaxContentEncoding.h>

#include "common.h"
#include "compression.h"
#include "matroska.h"
#include "mm_io.h"
#include "p_vobsub.h"

using namespace libmatroska;

#define PFX "vobsub_packetizer: "

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *nreader,
                                         const void *nidx_data,
                                         int nidx_data_size,
                                         bool nextract_from_mpeg,
                                         track_info_c *nti) throw (error_c):
  generic_packetizer_c(nreader, nti) {

  idx_data = (unsigned char *)safememdup(nidx_data, nidx_data_size);
  idx_data_size = nidx_data_size;
  extract_from_mpeg = nextract_from_mpeg;
  packet_num = 0;
  spu_size = 0;
  overhead = 0;

  set_track_type(track_subtitle);
#ifdef HAVE_ZLIB_H
  set_default_compression_method(COMPRESSION_ZLIB);
#endif
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
  mxverb(2, PFX "SPU size: %lld, overall size: %lld, "
         "overhead: %lld (%.3f%%)\n", spu_size, spu_size + overhead,
         overhead, (float)(100.0 * overhead / (overhead + spu_size)));
  safefree(idx_data);
}

void vobsub_packetizer_c::set_headers() {
  set_codec_id(MKV_S_VOBSUB);
  set_codec_private(idx_data, idx_data_size);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

#define TIMECODE (timecode - initial_displacement) / 60 / 60 / 1000, \
                 ((timecode - initial_displacement) / 60 / 1000) % 60, \
                 ((timecode - initial_displacement) / 1000) % 60, \
                 (timecode - initial_displacement) % 1000
#define FMT_TIMECODE "%02lld:%02lld:%02lld.%03lld"

int vobsub_packetizer_c::extract_duration(unsigned char *data, int buf_size,
                                          int64_t timecode) {
  uint32_t date, control_start, next_off, start_off, off;
  unsigned char type;
  int duration;
  bool unknown;

  control_start = get_uint16_be(data + 2);
  next_off = control_start;
  duration = -1;

  while ((start_off != next_off) && (next_off < buf_size)) {
    start_off = next_off;
    date = get_uint16_be(data + start_off) * 1024;
    next_off = get_uint16_be(data + start_off + 2);
    if (next_off < start_off) {
      mxwarn(PFX "Encountered broken SPU packet (next_off < start_off) at "
             "timecode " FMT_TIMECODE ". This packet might be displayed "
             "incorrectly or not at all.\n", TIMECODE);
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
          return date / 90;
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

#define deliver() deliver_packet(dst_buf, dst_size, timecode, duration);
int vobsub_packetizer_c::deliver_packet(unsigned char *buf, int size,
                                        int64_t timecode,
                                        int64_t default_duration) {
  int64_t duration;

  if ((buf == NULL) || (size == 0)) {
    safefree(buf);
    return -1;
  }

  duration = extract_duration(buf, size, timecode);
  if (duration == -1) {
    mxverb(3, PFX "Could not extract the duration for a SPU packet (timecode: "
           FMT_TIMECODE ").\n", TIMECODE);
    duration = default_duration;
  }
  if (duration != -2) {
    timecode = (int64_t)((float)timecode * ti->async.linear);
    add_packet(buf, size, timecode, duration, true);
  }
  safefree(buf);

  return -1;
}

// Adopted from mplayer's vobsub.c
int vobsub_packetizer_c::process(unsigned char *srcbuf, int size,
                                int64_t timecode, int64_t duration,
                                int64_t, int64_t) {
  unsigned char *dst_buf;
  uint32_t len, idx, version, packet_size, dst_size;
  int c, aid;
  float pts;
  /* Goto start of a packet, it starts with 0x000001?? */
  const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];

  pts = 0.0;
  packet_num++;

  timecode += initial_displacement;
  if (timecode < 0)
    return EMOREDATA;

  if (extract_from_mpeg) {
    mm_mem_io_c in(srcbuf, size);

    dst_buf = NULL;
    dst_size = 0;
    packet_size = 0;
    while (1) {
      if (in.read(buf, 4) != 4)
        return deliver();
      while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
        c = in.getch();
        if (c < 0)
          return deliver();
        memmove(buf, buf + 1, 3);
        buf[3] = c;
      }
      switch (buf[3]) {
        case 0xb9:			/* System End Code */
          return deliver();
          break;

        case 0xba:			/* Packet start code */
          c = in.getch();
          if (c < 0)
            return deliver();
          if ((c & 0xc0) == 0x40)
            version = 4;
          else if ((c & 0xf0) == 0x20)
            version = 2;
          else {
            mxwarn(PFX "Unsupported MPEG version: 0x%02x in packet %lld\n", c,
                   packet_num);
            return deliver();
          }

          if (version == 4) {
            if (!in.setFilePointer2(9, seek_current))
              return deliver();
          } else if (version == 2) {
            if (!in.setFilePointer2(7, seek_current))
              return deliver();
          } else
            abort();
          break;

        case 0xbd:			/* packet */
          if (in.read(buf, 2) != 2)
            return deliver();
          len = buf[0] << 8 | buf[1];
          idx = in.getFilePointer();
          c = in.getch();
          if (c < 0)
            return deliver();
          if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
            if (in.getch() < 0)
              return deliver();
            c = in.getch();
            if (c < 0)
              return deliver();
          }
          if ((c & 0xf0) == 0x20) { /* System-1 stream timestamp */
            /* Do we need this? */
            abort();
          } else if ((c & 0xf0) == 0x30) {
            /* Do we need this? */
            abort();
          } else if ((c & 0xc0) == 0x80) { /* System-2 (.VOB) stream */
            uint32_t pts_flags, hdrlen, dataidx;
            c = in.getch();
            if (c < 0)
              return deliver();
            pts_flags = c;
            c = in.getch();
            if (c < 0)
              return deliver();
            hdrlen = c;
            dataidx = in.getFilePointer() + hdrlen;
            if (dataidx > idx + len) {
              mxwarn(PFX "Invalid header length: %d (total length: %d, "
                     "idx: %d, dataidx: %d)\n", hdrlen, len, idx, dataidx);
              return deliver();
            }
            if ((pts_flags & 0xc0) == 0x80) {
              if (in.read(buf, 5) != 5)
                return deliver();
              if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) &&
                    (buf[2] & 1) && (buf[4] & 1))) {
                mxwarn(PFX "PTS error: 0x%02x %02x%02x %02x%02x \n",
                       buf[0], buf[1], buf[2], buf[3], buf[4]);
                pts = 0;
              } else
                pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 |
                       (buf[2] & 0xfe) << 14 | buf[3] << 7 | (buf[4] >> 1));
            } else /* if ((pts_flags & 0xc0) == 0xc0) */ {
              /* what's this? */
              /* abort(); */
            }
            in.setFilePointer2(dataidx, seek_beginning);
            aid = in.getch();
            if (aid < 0) {
              mxwarn(PFX "Bogus aid %d\n", aid);
              return deliver();
            }
            packet_size = len - ((unsigned int)in.getFilePointer() - idx);
            dst_buf = (unsigned char *)saferealloc(dst_buf, dst_size +
                                                   packet_size);
            mxverb(3, PFX "sub packet data: aid: %d, pts: %.3fs, packet_size: "
                   "%u\n", aid, pts, packet_size);
            if (in.read(&dst_buf[dst_size], packet_size) != packet_size) {
              mxwarn(PFX "in.read failure");
              return deliver();
            }
            dst_size += packet_size;
            spu_size += packet_size;
            overhead += size - packet_size;
          
            idx = len;
          }
          break;

        case 0xbe:			/* Padding */
          if (in.read(buf, 2) != 2)
            return deliver();
          len = buf[0] << 8 | buf[1];
          if ((len > 0) && !in.setFilePointer2(len, seek_current))
            return deliver();
          break;

        default:
          if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
            /* MPEG audio or video */
            if (in.read(buf, 2) != 2)
              return deliver();
            len = (buf[0] << 8) | buf[1];
            if ((len > 0) && !in.setFilePointer2(len, seek_current))
              return deliver();

          } else {
            mxwarn(PFX "unknown header 0x%02X%02X%02X%02X\n", buf[0], buf[1],
                   buf[2], buf[3]);
            return deliver();
          }
      }
    }

    return deliver();
  }

  spu_size += size;
  timecode = (int64_t)((float)timecode * ti->async.linear);
  add_packet(srcbuf, size, timecode, duration, true);

  return EMOREDATA;
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}
