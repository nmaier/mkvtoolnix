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

// Adopted from mplayer's vobsub.c
int vobsub_packetizer_c::process(unsigned char *srcbuf, int size,
                                int64_t timecode, int64_t duration,
                                int64_t, int64_t) {
  unsigned char *data;
  uint32_t len, idx, version, packet_size;
  int c, aid;
  float pts;
  /* Goto start of a packet, it starts with 0x000001?? */
  const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];

  packet_num++;

  if (extract_from_mpeg) {
    mm_mem_io_c in(srcbuf, size);

    packet_size = 0;
    while (1) {
      if (in.read(buf, 4) != 4)
        return -1;
      while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
        c = in.getch();
        if (c < 0)
          return -1;
        memmove(buf, buf + 1, 3);
        buf[3] = c;
      }
      switch (buf[3]) {
        case 0xb9:			/* System End Code */
          return 0;
          break;

        case 0xba:			/* Packet start code */
          c = in.getch();
          if (c < 0)
            return -1;
          if ((c & 0xc0) == 0x40)
            version = 4;
          else if ((c & 0xf0) == 0x20)
            version = 2;
          else {
            mxwarn(PFX "Unsupported MPEG version: 0x%02x in packet %lld\n", c,
                   packet_num);
            return -1;
          }

          if (version == 4) {
            if (!in.setFilePointer2(9, seek_current))
              return -1;
          } else if (version == 2) {
            if (!in.setFilePointer2(7, seek_current))
              return -1;
          } else
            abort();
          break;

        case 0xbd:			/* packet */
          if (in.read(buf, 2) != 2)
            return -1;
          len = buf[0] << 8 | buf[1];
          idx = in.getFilePointer();
          c = in.getch();
          if (c < 0)
            return -1;
          if ((c & 0xC0) == 0x40) { /* skip STD scale & size */
            if (in.getch() < 0)
              return -1;
            c = in.getch();
            if (c < 0)
              return -1;
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
              return -1;
            pts_flags = c;
            c = in.getch();
            if (c < 0)
              return -1;
            hdrlen = c;
            dataidx = in.getFilePointer() + hdrlen;
            if (dataidx > idx + len) {
              mxwarn(PFX "Invalid header length: %d (total length: %d, "
                     "idx: %d, dataidx: %d)\n", hdrlen, len, idx, dataidx);
              return -1;
            }
            if ((pts_flags & 0xc0) == 0x80) {
              if (in.read(buf, 5) != 5)
                return -1;
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
              return -1;
            }
            packet_size = len - ((unsigned int)in.getFilePointer() - idx);
            data = (unsigned char *)safemalloc(packet_size);
            if (in.read(data, packet_size) != packet_size) {
              mxwarn(PFX "in.read failure");
              safefree(data);
              return -1;
            }

            spu_size += packet_size;
            overhead += size - packet_size;
            add_packet(data, packet_size, timecode, duration, true);
            safefree(data);
          
            idx = len;
          }
          break;

        case 0xbe:			/* Padding */
          if (in.read(buf, 2) != 2)
            return -1;
          len = buf[0] << 8 | buf[1];
          if ((len > 0) && !in.setFilePointer2(len, seek_current))
            return -1;
          break;

        default:
          if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
            /* MPEG audio or video */
            if (in.read(buf, 2) != 2)
              return -1;
            len = (buf[0] << 8) | buf[1];
            if ((len > 0) && !in.setFilePointer2(len, seek_current))
              return -1;

          } else {
            mxwarn(PFX "unknown header 0x%02X%02X%02X%02X\n", buf[0], buf[1],
                   buf[2], buf[3]);
            return -1;
          }
      }
    }

    return EMOREDATA;
  }

  spu_size += size;
  add_packet(buf, size, timecode, duration, true);

  return EMOREDATA;
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}
