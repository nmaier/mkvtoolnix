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

class wav_dts_demuxer_c: public wav_demuxer_c {
private:
  bool dts_swap_bytes, dts_14_16;
  dts_header_t dtsheader;
  memory_cptr buf[2];
  int cur_buf;

public:
  wav_dts_demuxer_c(wav_reader_c *n_reader, wave_header *n_wheader);

  virtual ~wav_dts_demuxer_c();

  virtual bool probe(mm_io_cptr &io);

  virtual int64_t get_preferred_input_size() {
    return DTS_READ_SIZE;
  };

  virtual unsigned char *get_buffer() {
    return buf[cur_buf]->get();
  };

  virtual void process(int64_t len);
  virtual generic_packetizer_c *create_packetizer();
  virtual string get_codec() {
    return "DTS";
  };

private:
  virtual int decode_buffer(int len);
};

class wav_pcm_demuxer_c: public wav_demuxer_c {
private:
  int bps;
  memory_cptr buffer;

public:
  wav_pcm_demuxer_c(wav_reader_c *n_reader, wave_header *n_wheader);

  virtual ~wav_pcm_demuxer_c();

  virtual int64_t get_preferred_input_size() {
    return bps;
  };

  virtual unsigned char *get_buffer() {
    return buffer->get();
  };

  virtual void process(int64_t len);
  virtual generic_packetizer_c *create_packetizer();
  virtual string get_codec() {
    return "PCM";
  };

  virtual bool probe(mm_io_cptr &) {
    return true;
  };
};

// ----------------------------------------------------------

wav_dts_demuxer_c::wav_dts_demuxer_c(wav_reader_c *n_reader,
                                     wave_header  *n_wheader):
  wav_demuxer_c(n_reader, n_wheader),
  dts_swap_bytes(false), dts_14_16(false), cur_buf(0) {

  buf[0] = memory_c::alloc(DTS_READ_SIZE);
  buf[1] = memory_c::alloc(DTS_READ_SIZE);
}

wav_dts_demuxer_c::~wav_dts_demuxer_c() {
}

bool
wav_dts_demuxer_c::probe(mm_io_cptr &io) {
  bool is_dts = false;

  io->save_pos();
  int len = io->read(buf[cur_buf]->get(), DTS_READ_SIZE);
  io->restore_pos();

  if (detect_dts(buf[cur_buf]->get(), len, dts_14_16, dts_swap_bytes)) {
    len = decode_buffer(len);
    if (find_dts_header(buf[cur_buf]->get(), len, &dtsheader) >= 0) {
      is_dts = true;
      mxverb(3, "DTSinWAV: 14->16 %d swap %d\n", dts_14_16, dts_swap_bytes);
    }
  }

  return is_dts;
}

int
wav_dts_demuxer_c::decode_buffer(int len) {
  if (dts_swap_bytes) {
    swab((char *)buf[cur_buf]->get(), (char *)buf[cur_buf ^ 1]->get(), len);
    cur_buf ^= 1;
  }

  if (dts_14_16) {
    dts_14_to_dts_16((unsigned short *)buf[cur_buf]->get(), len / 2, (unsigned short *)buf[cur_buf ^ 1]->get());
    cur_buf ^= 1;
    len      = len * 7 / 8;
  }

  return len;
}

generic_packetizer_c *
wav_dts_demuxer_c::create_packetizer() {
  ptzr = new dts_packetizer_c(reader, dtsheader, reader->ti);

  // .wav with DTS are always filled up with other stuff to match the bitrate.
  ((dts_packetizer_c *)ptzr)->skipping_is_normal = true;

  mxinfo(FMT_TID "Using the DTS output module.\n", reader->ti.fname.c_str(), (int64_t)0);

  if (1 < verbose)
    print_dts_header(&dtsheader);

  return ptzr;
}

void
wav_dts_demuxer_c::process(int64_t size) {
  if (size <= 0)
    return;

  long dec_len = decode_buffer(size);
  ptzr->process(new packet_t(new memory_c(buf[cur_buf]->get(), dec_len, false)));
}

// ----------------------------------------------------------

wav_pcm_demuxer_c::wav_pcm_demuxer_c(wav_reader_c *n_reader,
                                     wave_header  *n_wheader):
  wav_demuxer_c(n_reader, n_wheader),
  bps(0) {

  bps    = get_uint16_le(&wheader->common.wChannels) * get_uint16_le(&wheader->common.wBitsPerSample) * get_uint32_le(&wheader->common.dwSamplesPerSec) / 8;
  buffer = memory_c::alloc(bps);
}

wav_pcm_demuxer_c::~wav_pcm_demuxer_c() {
}

generic_packetizer_c *
wav_pcm_demuxer_c::create_packetizer() {
  ptzr = new pcm_packetizer_c(reader,
                              get_uint32_le(&wheader->common.dwSamplesPerSec),
                              get_uint16_le(&wheader->common.wChannels),
                              get_uint16_le(&wheader->common.wBitsPerSample),
                              reader->ti);

  mxinfo(FMT_TID "Using the PCM output module.\n", reader->ti.fname.c_str(), (int64_t)0);

  return ptzr;
}

void
wav_pcm_demuxer_c::process(int64_t len) {
  if (len <= 0)
    return;

  ptzr->process(new packet_t(new memory_c(buffer->get(), len, false)));
}

// ----------------------------------------------------------

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

  if (strncmp((char *)wheader.riff.id,      "RIFF", 4) ||
      strncmp((char *)wheader.riff.wave_id, "WAVE", 4))
    return 0;

  return 1;
}

wav_reader_c::wav_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  bytes_processed(0), bytes_in_data_chunks(0), remaining_bytes_in_current_data_chunk(0),
  cur_data_chunk_idx(0) {

  int64_t size;

  try {
    io = mm_io_cptr(new mm_file_io_c(ti.fname));
    io->setFilePointer(0, seek_end);
    size = io->getFilePointer();
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    throw error_c("wav_reader: Could not open the source file.");
  }
  if (!wav_reader_c::probe_file(io.get(), size))
    throw error_c("wav_reader: Source is not a valid WAVE file.");

  parse_file();
  create_demuxer();
}

wav_reader_c::~wav_reader_c() {
}

void
wav_reader_c::parse_file() {
  int chunk_idx;

  if (io->read(&wheader.riff, sizeof(wheader.riff)) != sizeof(wheader.riff))
    throw error_c("wav_reader: could not read WAVE header.");

  scan_chunks();

//   mxinfo("Found %d chunks\n", chunks.size());
//   for (int wuff = 0; wuff < chunks.size(); wuff++) {
//     mxinfo("  %d pos %lld len %u id %4s\n", wuff, chunks[wuff].pos, chunks[wuff].len, chunks[wuff].id);
//   }

  if ((chunk_idx = find_chunk("fmt ")) == -1)
    throw error_c("wav_reader: No format chunk found.");

  io->setFilePointer(chunks[chunk_idx].pos, seek_beginning);

  if ((io->read(&wheader.format, sizeof(wheader.format)) != sizeof(wheader.format)) ||
      (io->read(&wheader.common, sizeof(wheader.common)) != sizeof(wheader.common)))
    throw error_c("wav_reader: The format chunk could not be read.");

  if ((cur_data_chunk_idx = find_chunk("data", 0, false)) == -1)
    throw error_c("wav_reader: No data chunk was found.");

  io->setFilePointer(chunks[cur_data_chunk_idx].pos + sizeof(struct chunk_struct), seek_beginning);

  remaining_bytes_in_current_data_chunk = chunks[cur_data_chunk_idx].len;
}

void
wav_reader_c::create_demuxer() {
  ti.id = 0;                    // ID for this track.

  demuxer = wav_demuxer_cptr(new wav_dts_demuxer_c(this, &wheader));
  if (!demuxer->probe(io))
    demuxer.clear();

  if (!demuxer.get())
    demuxer = wav_demuxer_cptr(new wav_pcm_demuxer_c(this, &wheader));

  if (verbose)
    mxinfo(FMT_FN "Using the WAV demultiplexer.\n", ti.fname.c_str());
}

void
wav_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(demuxer->create_packetizer());
}

file_status_e
wav_reader_c::read(generic_packetizer_c *,
                   bool) {
  int64_t        requested_bytes = MXMIN(remaining_bytes_in_current_data_chunk, demuxer->get_preferred_input_size());
  unsigned char *buffer          = demuxer->get_buffer();
  int64_t        num_read;

  num_read = io->read(buffer, requested_bytes);

  if (num_read <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  demuxer->process(num_read);

  bytes_processed                       += num_read;
  remaining_bytes_in_current_data_chunk -= num_read;

  if (!remaining_bytes_in_current_data_chunk) {
    cur_data_chunk_idx = find_chunk("data", cur_data_chunk_idx + 1, false);

    if (-1 == cur_data_chunk_idx) {
      PTZR0->flush();
      return FILE_STATUS_DONE;
    }

    io->setFilePointer(chunks[cur_data_chunk_idx].pos + sizeof(struct chunk_struct), seek_beginning);

    remaining_bytes_in_current_data_chunk = chunks[cur_data_chunk_idx].len;
  }

  return FILE_STATUS_MOREDATA;
}

void
wav_reader_c::scan_chunks() {
  wav_chunk_t new_chunk;

  try {
    while (true) {
      new_chunk.pos = io->getFilePointer();

      if (io->read(new_chunk.id, 4) != 4)
        return;

      new_chunk.len = io->read_uint32_le();

      chunks.push_back(new_chunk);

      if (!strncasecmp(new_chunk.id, "data", 4))
        bytes_in_data_chunks += new_chunk.len;

      io->setFilePointer(new_chunk.len, seek_current);

    }
  } catch (...) {
  }
}

int
wav_reader_c::find_chunk(const char *id,
                         int start_idx,
                         bool allow_empty) {
  int idx;

  for (idx = start_idx; idx < chunks.size(); ++idx)
    if (!strncasecmp(chunks[idx].id, id, 4) && (allow_empty || chunks[idx].len))
      return idx;

  return -1;
}

int
wav_reader_c::get_progress() {
  return bytes_in_data_chunks ? (100 * bytes_processed / bytes_in_data_chunks) : 100;
}

void
wav_reader_c::identify() {
  id_result_container("WAV");
  id_result_track(0, ID_RESULT_TRACK_AUDIO, demuxer->get_codec());
}
