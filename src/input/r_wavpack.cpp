/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   WAVPACK demultiplexer module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/error.h"
#include "input/r_wavpack.h"
#include "merge/output_control.h"
#include "output/p_wavpack.h"

int
wavpack_reader_c::probe_file(mm_io_c *io,
                             uint64_t size) {
  wavpack_header_t header;

  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(&header, sizeof(wavpack_header_t)) !=
        sizeof(wavpack_header_t))
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }

  if (!strncmp(header.ck_id, "wvpk", 4)) {
    header.version = get_uint16_le(&header.version);
    if ((header.version >> 8) == 4)
      return 1;
  }
  return 0;
}

wavpack_reader_c::wavpack_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {

  try {
    io              = new mm_file_io_c(m_ti.m_fname);
    size            = io->get_size();

    int packet_size = wv_parse_frame(io, header, meta, true, true);
    if (0 > packet_size)
      mxerror_fn(m_ti.m_fname, Y("The file header was not read correctly.\n"));
  } catch (...) {
    throw error_c(Y("wavpack_reader: Could not open the file."));
  }

  io->setFilePointer(io->getFilePointer() - sizeof(wavpack_header_t), seek_beginning);

  // correction file if applies
  io_correc           = NULL;
  meta.has_correction = false;

  try {
    if (header.flags & WV_HYBRID_FLAG) {
      io_correc       = new mm_file_io_c(m_ti.m_fname + "c");
      int packet_size = wv_parse_frame(io_correc, header_correc, meta_correc, true, true);
      if (0 > packet_size)
        mxerror_fn(m_ti.m_fname, Y("The correction file header was not read correctly.\n"));

      io_correc->setFilePointer(io_correc->getFilePointer() - sizeof(wavpack_header_t), seek_beginning);
      meta.has_correction = true;
    }
  } catch (...) {
    if (verbose)
      mxinfo_fn(m_ti.m_fname, boost::format(Y("Could not open the corresponding correction file '%1%c'.\n")) % m_ti.m_fname);
  }

  if (verbose) {
    if (meta.has_correction)
      mxinfo_fn(m_ti.m_fname, Y("Using the WAVPACK demultiplexer with a correction file.\n"));
    else
      show_demuxer_info();
  }
}

wavpack_reader_c::~wavpack_reader_c() {
  delete io_correc;
  delete io;
}

void
wavpack_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  uint16_t version_le;
  put_uint16_le(&version_le, header.version);
  m_ti.m_private_data = (unsigned char *)&version_le;
  m_ti.m_private_size = sizeof(uint16_t);
  add_packetizer(new wavpack_packetizer_c(this, m_ti, meta));
  m_ti.m_private_data = NULL;

  show_packetizer_info(0, PTZR0);
}

file_status_e
wavpack_reader_c::read(generic_packetizer_c *ptzr,
                       bool force) {
  wavpack_header_t dummy_header, dummy_header_correc;
  wavpack_meta_t dummy_meta;
  uint64_t initial_position = io->getFilePointer();
  uint8_t *chunk, *databuffer;

  // determine the final data size
  int32_t data_size = 0, block_size;
  int extra_frames_number = -1;

  dummy_meta.channel_count = 0;
  while (dummy_meta.channel_count < meta.channel_count) {
    extra_frames_number++;
    block_size = wv_parse_frame(io, dummy_header, dummy_meta, false, false);
    if (-1 == block_size)
      return flush_packetizers();
    data_size += block_size;
    io->skip(block_size);
  }

  if (0 > data_size)
    return flush_packetizers();

  data_size += 3 * sizeof(uint32_t);
  if (extra_frames_number)
    data_size += sizeof(uint32_t) + extra_frames_number * 3 * sizeof(uint32_t);
  chunk = (uint8_t *)safemalloc(data_size);

  // keep the header minus the ID & size (all found in Matroska)
  put_uint32_le(chunk, dummy_header.block_samples);

  io->setFilePointer(initial_position);

  dummy_meta.channel_count = 0;
  databuffer               = &chunk[4];
  while (dummy_meta.channel_count < meta.channel_count) {
    block_size = wv_parse_frame(io, dummy_header, dummy_meta, false, false);
    put_uint32_le(databuffer, dummy_header.flags);
    databuffer += 4;
    put_uint32_le(databuffer, dummy_header.crc);
    databuffer += 4;
    if (2 < meta.channel_count) {
      // not stored for the last block
      put_uint32_le(databuffer, block_size);
      databuffer += 4;
    }
    if (io->read(databuffer, block_size) != static_cast<size_t>(block_size))
      return flush_packetizers();
    databuffer += block_size;
  }

  packet_cptr packet(new packet_t(new memory_c(chunk, data_size, true)));

  // find the if there is a correction file data corresponding
  if (io_correc) {
    do {
      initial_position         = io_correc->getFilePointer();
      // determine the final data size
      data_size                = 0;
      extra_frames_number      = 0;
      dummy_meta.channel_count = 0;

      while (dummy_meta.channel_count < meta_correc.channel_count) {
        extra_frames_number++;
        block_size = wv_parse_frame(io_correc, dummy_header_correc, dummy_meta, false, false);
        if (-1 == block_size)
          return flush_packetizers();
        data_size += block_size;
        io_correc->skip(block_size);
      }

      // no more correction to be found
      if (0 > data_size) {
        delete io_correc;
        io_correc = NULL;
        dummy_header_correc.block_samples = dummy_header.block_samples + 1;
        break;
      }
    } while (dummy_header_correc.block_samples < dummy_header.block_samples);

    if (dummy_header_correc.block_samples == dummy_header.block_samples) {

      io_correc->setFilePointer(initial_position);

      if (meta.channel_count > 2)
        data_size += extra_frames_number * 2 * sizeof(uint32_t);
      else
        data_size += sizeof(uint32_t);

      uint8_t *chunk_correc    = (uint8_t *)safemalloc(data_size);
      // only keep the CRC in the header
      dummy_meta.channel_count = 0;
      databuffer               = chunk_correc;

      while (dummy_meta.channel_count < meta_correc.channel_count) {
        block_size = wv_parse_frame(io_correc, dummy_header_correc, dummy_meta, false, false);
        put_uint32_le(databuffer, dummy_header_correc.crc);
        databuffer += 4;
        if (2 < meta_correc.channel_count) {
          // not stored for the last block
          put_uint32_le(databuffer, block_size);
          databuffer += 4;
        }
        if (io_correc->read(databuffer, block_size) != static_cast<size_t>(block_size)) {
          delete io_correc;
          io_correc = NULL;
        }
        databuffer += block_size;
      }

      packet->data_adds.push_back(memory_cptr(new memory_c(chunk_correc, data_size, true)));
    }
  }

  PTZR0->process(packet);

  return FILE_STATUS_MOREDATA;
}

int
wavpack_reader_c::get_progress() {
  return 100 * io->getFilePointer() / size;
}

void
wavpack_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "WAVPACK");
}
