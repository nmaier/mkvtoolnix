/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id$
    \brief MP3 reader module
    \author Moritz Bunkus <moritz@bunkus.org>
    \author Peter Niemayer <niemayer@isg.de>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef __CYGWIN__
#include <sys/unistd.h>         // Needed for swab()
#elif defined LIBEBML_GCC2
#define __USE_XOPEN
#include <unistd.h>
#endif

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_wav.h"
#include "p_pcm.h"
#include "p_dts.h"
#include "dts_common.h"

extern "C" {
#include <avilib.h> // for wave_header
}

void dts_14_to_dts_16(unsigned short * src, const unsigned long srcwords,
                      unsigned short * dst) {
  // srcwords has to be a multiple of 8!
  // you will get (srcbytes >> 3)*7 destination words!

  const unsigned long l = srcwords >> 3;

  for (unsigned long b = 0; b < l; b++) {
    unsigned short src_0 = (src[0]>>8) | (src[0]<<8);
    unsigned short src_1 = (src[1]>>8) | (src[1]<<8);
    // 14 + 2
    unsigned short dst_0 = (src_0 << 2)   | ((src_1 & 0x3fff) >> 12);
    dst[0] = (dst_0>>8) | (dst_0<<8);
    // 12 + 4
    unsigned short src_2 = (src[2]>>8) | (src[2]<<8);
    unsigned short dst_1 = (src_1 << 4)  | ((src_2 & 0x3fff) >> 10);
    dst[1] = (dst_1>>8) | (dst_1<<8);
    // 10 + 6
    unsigned short src_3 = (src[3]>>8) | (src[3]<<8);
    unsigned short dst_2 = (src_2 << 6)  | ((src_3 & 0x3fff) >> 8);
    dst[2] = (dst_2>>8) | (dst_2<<8);
    // 8  + 8
    unsigned short src_4 = (src[4]>>8) | (src[4]<<8);
    unsigned short dst_3 = (src_3 << 8)  | ((src_4 & 0x3fff) >> 6);
    dst[3] = (dst_3>>8) | (dst_3<<8);
    // 6  + 10
    unsigned short src_5 = (src[5]>>8) | (src[5]<<8);
    unsigned short dst_4 = (src_4 << 10) | ((src_5 & 0x3fff) >> 4);
    dst[4] = (dst_4>>8) | (dst_4<<8);
    // 4  + 12
    unsigned short src_6 = (src[6]>>8) | (src[6]<<8);
    unsigned short dst_5 = (src_5 << 12) | ((src_6 & 0x3fff) >> 2);
    dst[5] = (dst_5>>8) | (dst_5<<8);
    // 2  + 14
    unsigned short src_7 = (src[7]>>8) | (src[7]<<8);
    unsigned short dst_6 = (src_6 << 14) | ((src_7 & 0x3fff) >> 2);
    dst[6] = (dst_6>>8) | (dst_6<<8);

    dst += 7;
    src += 8;
  }
}

int wav_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  wave_header wheader;

  if (size < sizeof(wave_header))
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read((char *)&wheader, sizeof(wheader)) != sizeof(wheader))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (strncmp((char *)wheader.riff.id, "RIFF", 4) ||
      strncmp((char *)wheader.riff.wave_id, "WAVE", 4) ||
      strncmp((char *)wheader.data.id, "data", 4))
    return 0;

  return 1;
}

wav_reader_c::wav_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int64_t size;

  pcmpacketizer = 0;
  dtspacketizer = 0;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("wav_reader: Could not open the source file.");
  }
  if (!wav_reader_c::probe_file(mm_io, size))
    throw error_c("wav_reader: Source is not a valid WAVE file.");
  if (mm_io->read(&wheader, sizeof(wheader)) != sizeof(wheader))
    throw error_c("wav_reader: could not read WAVE header.");
  bps = wheader.common.wChannels * wheader.common.wBitsPerSample *
    wheader.common.dwSamplesPerSec / 8;
  chunk = (unsigned char *)safemalloc(bps + 1);
  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.

  {
    // check wether .wav file contains DTS data...
    unsigned short obuf[max_dts_packet_size/2];
    unsigned short buf[2][max_dts_packet_size/2];
    int cur_buf = 0;

    long rlen = mm_io->read(obuf, max_dts_packet_size);
    mm_io->setFilePointer(sizeof(wheader), seek_beginning);

    for (dts_swap_bytes = 0; dts_swap_bytes < 2; dts_swap_bytes++) {
      memcpy(buf[cur_buf], obuf, rlen);

      if (dts_swap_bytes) {
        swab(buf[cur_buf], buf[cur_buf^1], rlen);
        cur_buf ^= 1;
      }

      for (dts_14_16 = 0; dts_14_16 < 2; dts_14_16++) {
        long erlen = rlen;
        if (dts_14_16) {
          unsigned long words = rlen / (8*sizeof(short));
          dts_14_to_dts_16(buf[cur_buf], words*8, buf[cur_buf^1]);
          cur_buf ^= 1;
        }

        dts_header_t dtsheader;
        int pos = find_dts_header((const unsigned char *)buf[cur_buf], erlen,
                                  &dtsheader);

        if (pos >= 0) {
          if (verbose) {
            fprintf(stderr,"Using WAV demultiplexer for %s.\n"
                    "+-> Using DTS output module for audio stream. %s %s\n",
                    ti->fname, (dts_swap_bytes)? "(bytes swapped)" : "",
                    (dts_14_16)? "(DTS14 encoded)" : "(DTS16 encoded)");
            print_dts_header(&dtsheader);
            is_dts = true;
          }

          dtspacketizer = new dts_packetizer_c(this, dtsheader, ti);
          // .wav's with DTS are always filled up with other stuff to match
          // the bitrate...
          dtspacketizer->skipping_is_normal = true;
          break;
        }

      }

      if (dtspacketizer)
        break;
    }
  }

  if (!dtspacketizer) {
    pcmpacketizer = new pcm_packetizer_c(this, wheader.common.dwSamplesPerSec,
                                         wheader.common.wChannels,
                                         wheader.common.wBitsPerSample, ti);

    if (verbose)
      fprintf(stdout, "Using WAV demultiplexer for %s.\n+-> Using "
              "PCM output module for audio stream.\n", ti->fname);
    is_dts = false;
  }
}

wav_reader_c::~wav_reader_c() {
  delete mm_io;
  if (chunk != NULL)
    safefree(chunk);
  if (pcmpacketizer != NULL)
    delete pcmpacketizer;
  if (dtspacketizer != NULL)
    delete dtspacketizer;
}

int wav_reader_c::read() {
  if (pcmpacketizer) {
    int nread;

    nread = mm_io->read(chunk, bps);
    if (nread <= 0)
      return 0;

    pcmpacketizer->process(chunk, nread);

    bytes_processed += nread;

    if (nread != bps)
      return 0;
    else
      return EMOREDATA;
  }

  if (dtspacketizer) {
    unsigned short buf[2][max_dts_packet_size/2];
    int cur_buf = 0;
    long rlen = mm_io->read(buf[cur_buf], max_dts_packet_size);

    if (rlen <= 0)
      return 0;

    if (dts_swap_bytes) {
      swab(buf[cur_buf], buf[cur_buf^1], rlen);
      cur_buf ^= 1;
    }

    long erlen = rlen;
    if (dts_14_16) {
      unsigned long words = rlen / (8*sizeof(short));
      //if (words*8*sizeof(short) != rlen) {
      // unaligned problem, should not happen...
      //}
      dts_14_to_dts_16(buf[cur_buf], words*8, buf[cur_buf^1]);
      cur_buf ^= 1;
      erlen = words * 7 * sizeof(short);
    }

    dtspacketizer->process((unsigned char *) (buf[cur_buf]), erlen);

    bytes_processed += rlen;

    if (rlen != max_dts_packet_size)
      return 0;
    else
      return EMOREDATA;
  }

  return 0;
}

packet_t *wav_reader_c::get_packet() {
  if (pcmpacketizer)
    return pcmpacketizer->get_packet();
  else if (dtspacketizer)
    return dtspacketizer->get_packet();

  return NULL;
}

int wav_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void wav_reader_c::display_progress() {
  int samples = (wheader.riff.len - sizeof(wheader) + 8) / bps;
  fprintf(stdout, "progress: %d/%d seconds (%d%%)\r",
          (int)(bytes_processed / bps), (int)samples,
          (int)(bytes_processed * 100L / bps / samples));
  fflush(stdout);
}

void wav_reader_c::set_headers() {
  if (pcmpacketizer)
    pcmpacketizer->set_headers();
  if (dtspacketizer)
    dtspacketizer->set_headers();
}

void wav_reader_c::identify() {
  fprintf(stdout, "File '%s': container: WAV\nTrack ID 0: audio (%s)\n",
          ti->fname, is_dts ? "DTS" : "PCM");
}
