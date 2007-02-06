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

#define DTS_READ_SIZE 16384

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
  generic_reader_c(_ti),
  is_dts(false), dts_swap_bytes(false), dts_14_16(false), cur_buf(0) {
  int64_t size;

  buf[0] = NULL;
  buf[1] = NULL;

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

  if (!skip_to_chunk("data"))
    throw error_c("wav_reader: No data chunk was found.");

  if (io->read(&wheader.data, sizeof(wheader.data)) != sizeof(wheader.data))
    throw error_c("wav_reader: Could not read the data chunk header.");

  // check wether .wav file contains DTS data...
  buf[0] = (unsigned short *)safemalloc(DTS_READ_SIZE);
  buf[1] = (unsigned short *)safemalloc(DTS_READ_SIZE);

  try {
    io->save_pos();
    int len = io->read(buf[0], DTS_READ_SIZE);
    io->restore_pos();

    if (detect_dts(buf[0], len, dts_14_16, dts_swap_bytes)) {
      len = decode_buffer(len);
      if (find_dts_header((const unsigned char *)buf[cur_buf], len,
                          &dtsheader) >= 0) {
        is_dts = true;
        mxverb(3, "DTSinWAV: 14->16 %d swap %d\n", dts_14_16, dts_swap_bytes);
      }
    }
  } catch(...) {
  }

  if (!is_dts) {
    safefree(buf[0]);
    safefree(buf[1]);
    buf[0] = NULL;
    buf[1] = NULL;
  }

  if (verbose)
    mxinfo(FMT_FN "Using the WAV demultiplexer.\n", ti.fname.c_str());
}

wav_reader_c::~wav_reader_c() {
  delete io;
  safefree(chunk);
  safefree(buf[0]);
  safefree(buf[1]);
}

int
wav_reader_c::decode_buffer(int len) {
  if (dts_swap_bytes) {
    swab((char *)buf[cur_buf], (char *)buf[cur_buf^1], len);
    cur_buf ^= 1;
  }

  if (dts_14_16) {
    dts_14_to_dts_16(buf[cur_buf], len / 2, buf[cur_buf^1]);
    cur_buf ^= 1;
    len = len * 7 / 8;
  }

  return len;
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
    mxinfo(FMT_TID "Using the DTS output module.\n",
           ti.fname.c_str(), (int64_t)0);
    if (1 < verbose)
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

    if ((nread != bps) || io->eof()) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    } else
      return FILE_STATUS_MOREDATA;
  }

  if (is_dts) {
    long rlen = io->read(buf[cur_buf], DTS_READ_SIZE);

    if (rlen <= 0) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    }

    long dec_len = decode_buffer(rlen);
    PTZR0->process(new packet_t(new memory_c((unsigned char *)buf[cur_buf],
                                             dec_len, false)));

    bytes_processed += rlen;

    if (rlen != DTS_READ_SIZE) {
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
