/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * OGG media stream reader
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#if defined(HAVE_FLAC_FORMAT_H)
#include <FLAC/seekable_stream_decoder.h>
#endif

#include "common.h"
#include "flac_common.h"
#include "matroska.h"
#include "output_control.h"
#include "pr_generic.h"
#include "r_flac.h"

#define BUFFER_SIZE 4096

#if defined(HAVE_FLAC_FORMAT_H)
#define FPFX "flac_reader: "

static FLAC__SeekableStreamDecoderReadStatus
flac_read_cb(const FLAC__SeekableStreamDecoder *,
             FLAC__byte buffer[],
             unsigned *bytes,
             void *client_data) {
  return ((flac_reader_c *)client_data)->read_cb(buffer, bytes);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__SeekableStreamDecoder *,
              const FLAC__Frame *frame,
              const FLAC__int32 * const data[],
              void *client_data) {
  return ((flac_reader_c *)client_data)->write_cb(frame, data);
}

static void
flac_metadata_cb(const FLAC__SeekableStreamDecoder *,
                 const FLAC__StreamMetadata *metadata,
                 void *client_data) {
  ((flac_reader_c *)client_data)->metadata_cb(metadata);
}

static void
flac_error_cb(const FLAC__SeekableStreamDecoder *,
              FLAC__StreamDecoderErrorStatus status,
              void *client_data) {
  ((flac_reader_c *)client_data)->error_cb(status);
}

static FLAC__SeekableStreamDecoderSeekStatus
flac_seek_cb(const FLAC__SeekableStreamDecoder *,
             FLAC__uint64 absolute_byte_offset,
             void *client_data) {
  return ((flac_reader_c *)client_data)->seek_cb(absolute_byte_offset);
}

static FLAC__SeekableStreamDecoderTellStatus
flac_tell_cb(const FLAC__SeekableStreamDecoder *,
             FLAC__uint64 *absolute_byte_offset,
             void *client_data) {
  return ((flac_reader_c *)client_data)->tell_cb(*absolute_byte_offset);
}

static FLAC__SeekableStreamDecoderLengthStatus
flac_length_cb(const FLAC__SeekableStreamDecoder *,
               FLAC__uint64 *stream_length,
               void *client_data) {
  return ((flac_reader_c *)client_data)->length_cb(*stream_length);
}

static FLAC__bool
flac_eof_cb(const FLAC__SeekableStreamDecoder *,
            void *client_data) {
  return ((flac_reader_c *)client_data)->eof_cb();
}

int
flac_reader_c::probe_file(mm_io_c *mm_io,
                          int64_t size) {
  unsigned char data[4];

  if (size < 4)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(data, 4) != 4)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (strncmp((char *)data, "fLaC", 4))
    return 0;
  return 1;
}

flac_reader_c::flac_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  unsigned char *buf;
  uint32_t block_size;

  samples = 0;
  try {
    file = new mm_file_io_c(ti->fname);
    file_size = file->get_size();
  } catch (exception &ex) {
    throw error_c(FPFX "Could not open the source file.");
  }
  if (identifying)
    return;
  mxverb(1, FMT_FN "Using the FLAC demultiplexer.\n", ti->fname);

  if (!parse_file())
    throw error_c(FPFX "Could not read all header packets.");

  header = NULL;
  try {
    block_size = 0;
    for (current_block = blocks.begin();
         (current_block != blocks.end()) &&
           (current_block->type == FLAC_BLOCK_TYPE_HEADERS); current_block++)
      block_size += current_block->len;
    buf = (unsigned char *)safemalloc(block_size);
    block_size = 0;
    for (current_block = blocks.begin();
         (current_block != blocks.end()) &&
           (current_block->type == FLAC_BLOCK_TYPE_HEADERS); current_block++) {
      file->setFilePointer(current_block->filepos);
      if (file->read(&buf[block_size], current_block->len) !=
          current_block->len)
        mxerror(FPFX "Could not read a header packet.\n");
      block_size += current_block->len;
    }
    header = buf;
    header_size = block_size;
  } catch (error_c &error) {
    mxerror(FPFX "could not initialize the FLAC packetizer.\n");
  }
}

flac_reader_c::~flac_reader_c() {
  delete file;
  safefree(header);
}

void
flac_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new flac_packetizer_c(this, header, header_size, ti));
  mxinfo(FMT_TID "Using the FLAC output module.\n", ti->fname, (int64_t)0);
}

bool
flac_reader_c::parse_file() {
  FLAC__SeekableStreamDecoder *decoder;
  FLAC__SeekableStreamDecoderState state;
  flac_block_t block;
  uint64_t u, old_pos;
  int result, progress, old_progress;
  bool ok;

  file->setFilePointer(0);
  metadata_parsed = false;

  mxinfo("+-> Parsing the FLAC file. This can take a LONG time.\n");
  decoder = FLAC__seekable_stream_decoder_new();
  if (decoder == NULL)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");
  FLAC__seekable_stream_decoder_set_client_data(decoder, this);
  if (!FLAC__seekable_stream_decoder_set_read_callback(decoder, flac_read_cb))
    mxerror(FPFX "Could not set the read callback.\n");
  if (!FLAC__seekable_stream_decoder_set_write_callback(decoder,
                                                        flac_write_cb))
    mxerror(FPFX "Could not set the write callback.\n");
  if (!FLAC__seekable_stream_decoder_set_metadata_callback(decoder,
                                                           flac_metadata_cb))
    mxerror(FPFX "Could not set the metadata callback.\n");
  if (!FLAC__seekable_stream_decoder_set_error_callback(decoder,
                                                        flac_error_cb))
    mxerror(FPFX "Could not set the error callback.\n");
  if (!FLAC__seekable_stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");
  if (!FLAC__seekable_stream_decoder_set_seek_callback(decoder,
                                                       flac_seek_cb))
    mxerror(FPFX "Could not set the seek callback.\n");
  if (!FLAC__seekable_stream_decoder_set_tell_callback(decoder,
                                                       flac_tell_cb))
    mxerror(FPFX "Could not set the tell callback.\n");
  if (!FLAC__seekable_stream_decoder_set_length_callback(decoder,
                                                         flac_length_cb))
    mxerror(FPFX "Could not set the length callback.\n");
  if (!FLAC__seekable_stream_decoder_set_eof_callback(decoder,
                                                      flac_eof_cb))
    mxerror(FPFX "Could not set the eof callback.\n");
  if (FLAC__seekable_stream_decoder_init(decoder) !=
      FLAC__SEEKABLE_STREAM_DECODER_OK)
  mxerror(FPFX "Could not initialize the FLAC decoder.\n");

  result =
    (int)FLAC__seekable_stream_decoder_process_until_end_of_metadata(decoder);
  mxverb(2, FPFX "extract->metadata, result: %d, mdp: %d, num blocks: %u\n",
         result, metadata_parsed, blocks.size());

  if (!metadata_parsed)
    mxerror(FMT_FN "No metadata block found. This file is broken.\n",
            ti->fname);

  block.type = FLAC_BLOCK_TYPE_HEADERS;
  FLAC__seekable_stream_decoder_get_decode_position(decoder, &u);
  block.filepos = 0;
  block.len = u;
  old_pos = u;
  blocks.push_back(block);
  mxverb(2, FPFX "headers: block at %lld with size %d\n",
         block.filepos, block.len);

  old_progress = -5;
#if defined(HAVE_FLAC_DECODER_SKIP)
  ok = FLAC__seekable_stream_decoder_skip_single_frame(decoder);
#else
  ok = FLAC__seekable_stream_decoder_process_single(decoder);
#endif
  while (ok) {
    state = FLAC__seekable_stream_decoder_get_state(decoder);

    progress = (int)(file->getFilePointer() * 100 / file_size);
    if ((progress - old_progress) >= 5) {
      mxinfo("+-> Pre-parsing FLAC file: %d%%\r", progress);
      old_progress = progress;
    }

    if (FLAC__seekable_stream_decoder_get_decode_position(decoder, &u) &&
        (u != old_pos)) {
      block.type = FLAC_BLOCK_TYPE_DATA;
      block.filepos = old_pos;
      block.len = u - old_pos;
      old_pos = u;
      blocks.push_back(block);

      mxverb(2, FPFX "skip/decode frame, block at %lld with size %d\n",
             block.filepos, block.len);
    }

    if ((state == FLAC__SEEKABLE_STREAM_DECODER_END_OF_STREAM) ||
        (state == FLAC__SEEKABLE_STREAM_DECODER_STREAM_DECODER_ERROR) ||
        (state == FLAC__SEEKABLE_STREAM_DECODER_MEMORY_ALLOCATION_ERROR) ||
        (state == FLAC__SEEKABLE_STREAM_DECODER_READ_ERROR) ||
        (state == FLAC__SEEKABLE_STREAM_DECODER_SEEK_ERROR))
      break;

#if defined(HAVE_FLAC_DECODER_SKIP)
    ok = FLAC__seekable_stream_decoder_skip_single_frame(decoder);
#else
    ok = FLAC__seekable_stream_decoder_process_single(decoder);
#endif
  }

  mxinfo("+-> Pre-parsing FLAC file: 100%%\n");

  if ((blocks.size() == 0) || (blocks[0].type != FLAC_BLOCK_TYPE_HEADERS))
    mxerror(FPFX "Could not read all header packets.\n");

  FLAC__seekable_stream_decoder_reset(decoder);
  FLAC__seekable_stream_decoder_delete(decoder);

  file->setFilePointer(0);
  blocks[0].len -= 4;
  blocks[0].filepos = 4;

  return metadata_parsed;
}

file_status_t
flac_reader_c::read(generic_packetizer_c *,
                    bool) {
  unsigned char *buf;
  int samples_here;

  if (current_block == blocks.end())
    return file_status_done;
  buf = (unsigned char *)safemalloc(current_block->len);
  file->setFilePointer(current_block->filepos);
  if (file->read(buf, current_block->len) != current_block->len) {
    safefree(buf);
    PTZR0->flush();
    return file_status_done;
  }
  memory_c mem(buf, current_block->len, true);
  samples_here = flac_get_num_samples(buf, current_block->len, stream_info);
  PTZR0->process(mem, samples * 1000000000 / sample_rate);
  samples += samples_here;
  current_block++;

  if (current_block == blocks.end()) {
    PTZR0->flush();
    return file_status_done;
  }
  return file_status_moredata;
}

FLAC__SeekableStreamDecoderReadStatus
flac_reader_c::read_cb(FLAC__byte buffer[],
                       unsigned *bytes) {
  unsigned bytes_read, wanted_bytes;

  wanted_bytes = *bytes;
  bytes_read = file->read((unsigned char *)buffer, wanted_bytes);
  *bytes = bytes_read;
  return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK;
}

FLAC__StreamDecoderWriteStatus
flac_reader_c::write_cb(const FLAC__Frame *,
                        const FLAC__int32 * const []) {
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void
flac_reader_c::metadata_cb(const FLAC__StreamMetadata *metadata) {
  switch (metadata->type) {
    case FLAC__METADATA_TYPE_STREAMINFO:
      mxverb(2, FPFX "STREAMINFO block (%u bytes):\n", metadata->length);
      mxverb(2, FPFX "  sample_rate: %u Hz\n",
             metadata->data.stream_info.sample_rate);
      sample_rate = metadata->data.stream_info.sample_rate;
      mxverb(2, FPFX "  channels: %u\n", metadata->data.stream_info.channels);
      mxverb(2, FPFX "  bits_per_sample: %u\n",
             metadata->data.stream_info.bits_per_sample);
      memcpy(&stream_info, &metadata->data.stream_info,
             sizeof(FLAC__StreamMetadata_StreamInfo));
      metadata_parsed = true;
      break;
    default:
      mxverb(2, "%s (%u) block (%u bytes)\n",
             metadata->type == FLAC__METADATA_TYPE_PADDING ? "PADDING" :
             metadata->type == FLAC__METADATA_TYPE_APPLICATION ?
             "APPLICATION" :
             metadata->type == FLAC__METADATA_TYPE_SEEKTABLE ? "SEEKTABLE" :
             metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT ?
             "VORBIS COMMENT" :
             metadata->type == FLAC__METADATA_TYPE_CUESHEET ? "CUESHEET" :
             "UNDEFINED", metadata->type, metadata->length);
      break;
  }
}

void
flac_reader_c::error_cb(FLAC__StreamDecoderErrorStatus status) {
  mxerror(FPFX "Error parsing the file: %d\n", (int)status);
}

FLAC__SeekableStreamDecoderSeekStatus
flac_reader_c::seek_cb(uint64_t new_pos) {
  file->setFilePointer(new_pos, seek_beginning);
  if (file->getFilePointer() == new_pos)
    return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK;
  return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR;
}

FLAC__SeekableStreamDecoderTellStatus
flac_reader_c::tell_cb(uint64_t &absolute_byte_offset) {
  absolute_byte_offset = file->getFilePointer();
  return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__SeekableStreamDecoderLengthStatus
flac_reader_c::length_cb(uint64_t &stream_length) {
  stream_length = file_size;
  return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool
flac_reader_c::eof_cb() {
  return file->getFilePointer() >= file_size;
}

int
flac_reader_c::get_progress() {
  return 100 * distance(blocks.begin(), current_block) / blocks.size();
}

void
flac_reader_c::identify() {
  mxinfo("File '%s': container: FLAC\nTrack ID 0: audio (FLAC)\n", ti->fname);
}

#else  // HAVE_FLAC_FORMAT_H

int
flac_reader_c::probe_file(mm_io_c *mm_io,
                          int64_t size) {
  unsigned char data[4];

  if (size < 4)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(data, 4) != 4)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (strncmp((char *)data, "fLaC", 4))
    return 0;
  mxerror("mkvmerge has not been compiled with FLAC support.\n");
  return 1;
}

#endif // HAVE_FLAC_FORMAT_H
