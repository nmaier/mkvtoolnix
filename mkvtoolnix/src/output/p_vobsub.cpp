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
                                         track_info_t *nti) throw (error_c):
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

int vobsub_packetizer_c::extract_duration(unsigned char *data, int buf_size) {
  const int lengths[7] = {0, 0, 0, 2, 2, 6, 4};
  int i, next_ctrlblk, packet_size, data_size, t, len;

  packet_size = (data[0] << 8) | data[1];
  data_size = (data[2] << 8) | data[3];
  next_ctrlblk = data_size;

  do {
    i = next_ctrlblk;
    t = (data[i] << 8) | data[i + 1];
    i += 2;
    next_ctrlblk = (data[i] << 8) | data[i + 1];
    i += 2;

    if((next_ctrlblk > packet_size) || (next_ctrlblk < data_size))
      mxerror(PFX "Inconsistent data in the SPU packets (next_ctrlblk: %d, "
              "packet_size: %d, data_size: %d)\n", next_ctrlblk, packet_size,
              data_size);

    if (data[i] <= 0x06)
      len = lengths[data[i]];
    else
      len = 0;

    if ((i + len) > packet_size) {
      mxwarn(PFX "Warning: Wrong subpicture parameter blockending (i: %d, "
             "len: %d, packet_size: %d)\n", i, len, packet_size);
      break;
    }
    if (data[i] == 0x02)
      return 1024 * t / 90;
    i++;
  }
  while ((i <= next_ctrlblk) && (i < packet_size));

  return -1;
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

  duration = extract_duration(buf, size);
  if (duration == -1) {
    mxwarn(PFX "Could not extract the duration for a SPU packet.\n");
    duration = default_duration;
  }
  add_packet(buf, size, timecode, duration, true);
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

  packet_num++;

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
  add_packet(srcbuf, size, timecode, duration, true);

  return EMOREDATA;
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}
