/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   WAV reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Initial DTS support by Peter Niemayer <niemayer@isg.de> and
     modified by Moritz Bunkus.
*/

#include "os.h"

#if defined(COMP_CYGWIN)
#include <sys/unistd.h>         // Needed for swab()
#elif __GNUC__ == 2
#define __USE_XOPEN
#include <unistd.h>
#elif defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "r_wav.h"
#include "p_pcm.h"
#include "p_dts.h"
#include "dts_common.h"

extern "C" {
#include <avilib.h> // for wave_header
}

int
wav_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  wave_header wheader;

  if (size < sizeof(wave_header))
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(&wheader.riff, sizeof(wheader.riff)) != sizeof(wheader.riff))
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (strncmp((char *)wheader.riff.id, "RIFF", 4) ||
      strncmp((char *)wheader.riff.wave_id, "WAVE", 4))
    return 0;

  return 1;
}

wav_reader_c::wav_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  int64_t size;

  try {
    io = new mm_file_io_c(ti.fname);
    io->setFilePointer(0, seek_end);
    size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    throw error_c("wav_reader: Could not open the source file.");
  }
  if (!wav_reader_c::probe_file(io, size))
    throw error_c("wav_reader: Source is not a valid WAVE file.");

  if (io->read(&wheader.riff, sizeof(wheader.riff)) != sizeof(wheader.riff))
    throw error_c("wav_reader: could not read WAVE header.");

  if (!skip_to_chunk("fmt "))
    throw error_c("wav_reader: No format chunk found.");
  if ((io->read(&wheader.format, sizeof(wheader.format)) !=
       sizeof(wheader.format)) ||
      (io->read(&wheader.common, sizeof(wheader.common)) !=
       sizeof(wheader.common)))
    throw error_c("wav_reader: The format chunk could not be read.");

  bps = get_uint16_le(&wheader.common.wChannels) *
    get_uint16_le(&wheader.common.wBitsPerSample) *
    get_uint32_le(&wheader.common.dwSamplesPerSec) / 8;
  chunk = (unsigned char *)safemalloc(bps + 1);
  bytes_processed = 0;
  ti.id = 0;                   // ID for this track.
  is_dts = false;

  if (!skip_to_chunk("data"))
    throw error_c("wav_reader: No data chunk was found.");

  if (io->read(&wheader.data, sizeof(wheader.data)) != sizeof(wheader.data))
    throw error_c("wav_reader: Could not read the data chunk header.");

  if (verbose)
    mxinfo(FMT_FN "Using the WAV demultiplexer.\n", ti.fname.c_str());

  {
    // check wether .wav file contains DTS data...
    unsigned short obuf[max_dts_packet_size/2];
    unsigned short buf[2][max_dts_packet_size/2];
    int cur_buf = 0;

    io->save_pos();
    long rlen = io->read(obuf, max_dts_packet_size);
    io->restore_pos();

    for (dts_swap_bytes = 0; dts_swap_bytes < 2; dts_swap_bytes++) {
      memcpy(buf[cur_buf], obuf, rlen);

      if (dts_swap_bytes) {
        swab((char *)buf[cur_buf], (char *)buf[cur_buf^1], rlen);
        cur_buf ^= 1;
      }

      for (dts_14_16 = 0; dts_14_16 < 2; dts_14_16++) {
        long erlen = rlen;
        if (dts_14_16) {
          unsigned long words = rlen / (8*sizeof(short));
          dts_14_to_dts_16(buf[cur_buf], words*8, buf[cur_buf^1]);
          cur_buf ^= 1;
        }

        if (find_dts_header((const unsigned char *)buf[cur_buf], erlen,
                            &dtsheader) >= 0)
          is_dts = true;
      }

      if (is_dts)
        break;
    }
  }
}

wav_reader_c::~wav_reader_c() {
  delete io;
  safefree(chunk);
}

void
wav_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  if (!is_dts) {
    generic_packetizer_c *ptzr;
    ptzr =
      new pcm_packetizer_c(this,
                           get_uint32_le(&wheader.common.dwSamplesPerSec),
                           get_uint16_le(&wheader.common.wChannels),
                           get_uint16_le(&wheader.common.wBitsPerSample), ti);
    add_packetizer(ptzr);
    mxinfo(FMT_TID "Using the PCM output module.\n", ti.fname.c_str(),
           (int64_t)0);

  } else {
    add_packetizer(new dts_packetizer_c(this, dtsheader, ti));
    // .wav's with DTS are always filled up with other stuff to match
    // the bitrate...
    ((dts_packetizer_c *)PTZR0)->skipping_is_normal = true;
    mxinfo(FMT_TID "Using the DTS output module. %s %s\n",
           ti.fname.c_str(), (int64_t)0, (dts_swap_bytes)? "(bytes swapped)" :
           "", (dts_14_16)? "(DTS14 encoded)" : "(DTS16 encoded)");
    if (verbose > 1)
      print_dts_header(&dtsheader);
  }
}

file_status_e
wav_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (!is_dts) {
    int nread;

    nread = io->read(chunk, bps);
    if (nread <= 0) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    }

    PTZR0->process(new packet_t(new memory_c(chunk, nread, false)));

    bytes_processed += nread;

    if (nread != bps) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    } else if (io->eof())
      return FILE_STATUS_DONE;
    else
      return FILE_STATUS_MOREDATA;
  }

  if (is_dts) {
    unsigned short buf[2][max_dts_packet_size/2];
    int cur_buf = 0;
    long rlen = io->read(buf[cur_buf], max_dts_packet_size);

    if (rlen <= 0) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    }

    if (dts_swap_bytes) {
      swab((char *)buf[cur_buf], (char *)buf[cur_buf^1], rlen);
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

    PTZR0->process(new packet_t(new memory_c((unsigned char *)buf[cur_buf],
                                             erlen, false)));

    bytes_processed += rlen;

    if (rlen != max_dts_packet_size) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    } else
      return FILE_STATUS_MOREDATA;
  }

  return FILE_STATUS_DONE;
}

bool
wav_reader_c::skip_to_chunk(const char *id) {
  chunk_struct chunk;

  while (true) {
    if (io->read(&chunk, sizeof(chunk)) != sizeof(chunk))
      return false;

    if (!memcmp(&chunk.id, id, 4)) {
      io->setFilePointer(-(int)sizeof(chunk), seek_current);
      break;
    }

    if (!io->setFilePointer2(get_uint32_le(&chunk.len), seek_current))
      return false;
  }

  return true;
}

int
wav_reader_c::get_progress() {
  return 100 * bytes_processed / (get_uint32_le(&wheader.riff.len) -
                                  sizeof(wheader) + 8);
}

void
wav_reader_c::identify() {
  mxinfo("File '%s': container: WAV\nTrack ID 0: audio (%s)\n",
         ti.fname.c_str(), is_dts ? "DTS" : "PCM");
}
