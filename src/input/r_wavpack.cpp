/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
   
   WAVPACK demultiplexer module
  
   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "output_control.h"
#include "p_wavpack.h"
#include "r_wavpack.h"

int
wavpack_reader_c::probe_file(mm_io_c *mm_io,
                             int64_t size) {
  wavpack_header_t header;

  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(&header, sizeof(wavpack_header_t)) !=
        sizeof(wavpack_header_t))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (!strncmp(header.ck_id, "wvpk", 4)) {
    header.version = get_uint16_le(&header.version);
    if ((header.version >> 8) == 4)
      return 1;
  }
  return 0;
}

wavpack_reader_c::wavpack_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  int32_t packet_size;

  try {
    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();

    packet_size = wv_parse_frame(mm_io, header, meta, true, true);
    if (packet_size < 0)
      mxerror(FMT_FN "The file header was not read correctly.\n",
              ti->fname.c_str());
  } catch (exception &ex) {
    throw error_c("wavpack_reader: Could not open the file.");
  }

  mm_io->setFilePointer(mm_io->getFilePointer() - sizeof(wavpack_header_t),
                        seek_beginning);

  // correction file if applies
  mm_io_correc = NULL;
  meta.has_correction = false;
  try {
    if (header.flags & WV_HYBRID_FLAG) {
      mm_io_correc = new mm_file_io_c(ti->fname + "c");
      packet_size = wv_parse_frame(mm_io_correc, header_correc, meta_correc,
                                   true, true);
      if (packet_size < 0)
        mxerror(FMT_FN "The correction file header was not read correctly.\n",
                ti->fname.c_str());
      mm_io_correc->setFilePointer(mm_io_correc->getFilePointer() -
                                   sizeof(wavpack_header_t), seek_beginning);
      meta.has_correction = true;
    }
  } catch (exception &ex) {
    if (verbose)
      mxinfo(FMT_FN "Could not open the corresponding correction file '%s'.\n",
             ti->fname.c_str(), (ti->fname + "c").c_str());
  }

  if (verbose)
	  mxinfo(FMT_FN "Using the WAVPACK demultiplexer%s.\n", ti->fname.c_str(),
           meta.has_correction ? " with a correction file" : "");
}

wavpack_reader_c::~wavpack_reader_c() {
  delete mm_io_correc;
  delete mm_io;
}

void
wavpack_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new wavpack_packetizer_c(this, meta, ti));

  mxinfo(FMT_TID "Using the WAVPACK output module.\n", ti->fname.c_str(),
         (int64_t)0);
}

file_status_e
wavpack_reader_c::read(generic_packetizer_c *ptzr,
                       bool force) {
  wavpack_header_t dummy_header, dummy_header_correc;
  wavpack_meta_t dummy_meta;
  uint64_t initial_position = mm_io->getFilePointer();
  uint8_t *chunk, *databuffer;

  // determine the final data size
  int32_t data_size = 0, block_size;
  int extra_frames_number = -1;

  dummy_meta.channel_count = 0;
  while (dummy_meta.channel_count < meta.channel_count) {
    extra_frames_number++;
    block_size = wv_parse_frame(mm_io, dummy_header, dummy_meta, false, false);
    if (block_size == -1)
      return FILE_STATUS_DONE;
    data_size += block_size;
    mm_io->skip(block_size);
  }

  if (data_size < 0)
    return FILE_STATUS_DONE;

  data_size += sizeof(wavpack_header_t) - WV_KEEP_HEADER_POSITION;
  if (extra_frames_number)
    data_size += (extra_frames_number * 12) + 4;
  chunk = (uint8_t *)safemalloc(data_size);

  // keep the header minus the ID & size (all found in Matroska)
  put_uint16_le(chunk, dummy_header.version);
  chunk[2] = dummy_header.track_no;
  chunk[3] = dummy_header.index_no;
  put_uint32_le(&chunk[4], dummy_header.total_samples);
  put_uint32_le(&chunk[8], dummy_header.block_index);
  put_uint32_le(&chunk[12], dummy_header.block_samples);

  mm_io->setFilePointer(initial_position);

  dummy_meta.channel_count = 0;
  databuffer = &chunk[16];
	while (dummy_meta.channel_count < meta.channel_count) {
		block_size = wv_parse_frame(mm_io, dummy_header, dummy_meta, false, false);
		put_uint32_le(databuffer, dummy_header.flags);
		databuffer += 4;
		put_uint32_le(databuffer, dummy_header.crc);
		databuffer += 4;
		if (meta.channel_count > 2) {
			// not stored for the last block
			put_uint32_le(databuffer, block_size);
			databuffer += 4;
		}
		if (mm_io->read(databuffer, block_size) < 0)
			return FILE_STATUS_DONE;
		databuffer += block_size;
	}

  memory_c mem(chunk, data_size, true);

  // find the if there is a correction file data corresponding
  memories_c mems;
  mems.push_back(&mem);

  if (mm_io_correc) {
    do {
      initial_position = mm_io_correc->getFilePointer();
      // determine the final data size
      data_size = 0;
      extra_frames_number = 0;
      dummy_meta.channel_count = 0;
      while (dummy_meta.channel_count < meta_correc.channel_count) {
        extra_frames_number++;
        block_size = wv_parse_frame(mm_io_correc, dummy_header_correc,
                                    dummy_meta, false, false);
        if (block_size == -1)
          return FILE_STATUS_DONE;
        data_size += block_size;
        mm_io_correc->skip(block_size);
      }

      // no more correction to be found
      if (data_size < 0) {
        delete mm_io_correc;
        mm_io_correc = NULL;
        dummy_header_correc.block_samples = dummy_header.block_samples + 1;
        break;
      }
    } while (dummy_header_correc.block_samples < dummy_header.block_samples);

    if (dummy_header_correc.block_samples == dummy_header.block_samples) {
      uint8_t *chunk_correc;

      mm_io_correc->setFilePointer(initial_position);

      if (meta.channel_count > 2)
        data_size += extra_frames_number * 8;
      else
        data_size += 4;
      chunk_correc = (uint8_t *)safemalloc(data_size);

      // only keep the CRC in the header
      dummy_meta.channel_count = 0;
      databuffer = chunk_correc;
      while (dummy_meta.channel_count < meta_correc.channel_count) {
        block_size = wv_parse_frame(mm_io_correc, dummy_header_correc,
                                    dummy_meta, false, false);
        put_uint32_le(databuffer, dummy_header_correc.crc);
        databuffer += 4;
        if (meta_correc.channel_count > 2) {
          // not stored for the last block
          put_uint32_le(databuffer, block_size);
          databuffer += 4;
        }
        if (mm_io_correc->read(databuffer, block_size) < 0) {
          delete mm_io_correc;
          mm_io_correc = NULL;
        }
        databuffer += block_size;
      }

      memory_c mem_correc(chunk_correc, data_size, true);
      mems.push_back(&mem_correc);
      PTZR0->process(mems);
    }
  }

  if (mems.size() == 1)
    PTZR0->process(mem);

  return FILE_STATUS_MOREDATA;
}

int
wavpack_reader_c::get_progress() {
  return 100 * mm_io->getFilePointer() / size;
}

void
wavpack_reader_c::identify() {
  mxinfo("File '%s': container: WAVPACK\nTrack ID 0: audio (WAVPACK)\n",
         ti->fname.c_str());
}
