/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_flac.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief OGG media stream reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#if defined(HAVE_FLAC_FORMAT_H)
#include <FLAC/stream_decoder.h>
#endif

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "matroska.h"
#include "r_flac.h"

#define BUFFER_SIZE 4096

#if defined(HAVE_FLAC_FORMAT_H)
#define FPFX "flac_reader: "

static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder *, FLAC__byte buffer[],
            unsigned *bytes, void *client_data) {
  return ((flac_reader_c *)client_data)->read_cb(buffer, bytes);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder *, const FLAC__Frame *frame,
             const FLAC__int32 * const data[], void *client_data) {
  return ((flac_reader_c *)client_data)->write_cb(frame, data);
}

static void flac_metadata_cb(const FLAC__StreamDecoder *,
                            const FLAC__StreamMetadata *metadata,
                            void *client_data) {
  ((flac_reader_c *)client_data)->metadata_cb(metadata);
}

static void flac_error_cb(const FLAC__StreamDecoder *,
                         FLAC__StreamDecoderErrorStatus status,
                         void *client_data) {
  ((flac_reader_c *)client_data)->error_cb(status);
}

int flac_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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

flac_reader_c::flac_reader_c(track_info_c *nti) throw (error_c):
  generic_reader_c(nti) {
  unsigned char *buf;
  uint32_t size;

  samples = 0;
  try {
    file = new mm_io_c(ti->fname, MODE_READ);
    file_size = file->get_size();
  } catch (exception &ex) {
    throw error_c(FPFX "Could not open the source file.");
  }
  if (identifying) {
    read_buffer = NULL;
    packetizer = NULL;
    return;
  }
  if (verbose)
    mxinfo("Using the FLAC demultiplexer for %s.\n", ti->fname);

  read_buffer = (unsigned char *)safemalloc(BUFFER_SIZE);
  if (!parse_file())
    throw error_c(FPFX "Could not read all header packets.");

  try {
    size = 0;
    for (current_block = blocks.begin();
         (current_block != blocks.end()) &&
           (current_block->type == FLAC_BLOCK_TYPE_HEADERS); current_block++)
      size += current_block->len;
    buf = (unsigned char *)safemalloc(size);
    size = 0;
    for (current_block = blocks.begin();
         (current_block != blocks.end()) &&
           (current_block->type == FLAC_BLOCK_TYPE_HEADERS); current_block++) {
      file->setFilePointer(current_block->filepos);
      if (file->read(&buf[size], current_block->len) != current_block->len)
        mxerror(FPFX "Could not read a header packet.\n");
      size += current_block->len;
    }
    packetizer = new flac_packetizer_c(this, sample_rate, channels,
                                       bits_per_sample, buf, size, ti);
    packetizer->duplicate_data_on_add(false);
    safefree(buf);
  } catch (error_c &error) {
    mxerror(FPFX "could not initialize the FLAC packetizer.\n");
  }

  if (verbose)
    mxinfo("+-> Using the FLAC output module.\n");
}

flac_reader_c::~flac_reader_c() {
  if (packetizer != NULL)
    delete packetizer;
  delete file;
  safefree(read_buffer);
}

bool flac_reader_c::parse_file() {
  FLAC__StreamDecoder *decoder;
  int result, offset, pd_size;
  uint32_t i;
  unsigned char *pass_data;
  flac_block_t block;

  if ((pass == 2) &&
      ((pass_data = retrieve_pass_data(ti, pd_size)) != NULL)) {
    offset = 20;
    metadata_parsed = get_uint32(&pass_data[0]);
    channels = get_uint32(&pass_data[4]);
    sample_rate = get_uint32(&pass_data[8]);
    bits_per_sample = get_uint32(&pass_data[12]);
    pd_size = get_uint32(&pass_data[16]);
    for (i = 0; i < pd_size; i++) {
      memcpy(&block, &pass_data[offset + i * sizeof(flac_block_t)],
             sizeof(flac_block_t));
      blocks.push_back(block);
    }
    safefree(pass_data);

    return metadata_parsed;
  }

  done = false;
  file->setFilePointer(0);
  pos = 0;
  size = 0;
  metadata_parsed = false;
  packet_start = 0;

  mxinfo("+-> Parsing the FLAC file. This can take a LONG time.\n");
  decoder = FLAC__stream_decoder_new();
  if (decoder == NULL)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");
  FLAC__stream_decoder_set_client_data(decoder, this);
  if (!FLAC__stream_decoder_set_read_callback(decoder, flac_read_cb))
    mxerror(FPFX "Could not set the read callback.\n");
  if (!FLAC__stream_decoder_set_write_callback(decoder, flac_write_cb))
    mxerror(FPFX "Could not set the write callback.\n");
  if (!FLAC__stream_decoder_set_metadata_callback(decoder, flac_metadata_cb))
    mxerror(FPFX "Could not set the metadata callback.\n");
  if (!FLAC__stream_decoder_set_error_callback(decoder, flac_error_cb))
    mxerror(FPFX "Could not set the error callback.\n");
  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");
  if (FLAC__stream_decoder_init(decoder) !=
      FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");

  old_progress = -5;
  result = (int)FLAC__stream_decoder_process_until_end_of_stream(decoder);
  mxinfo("+-> Pre-parsing FLAC file: 100%%\n");
  mxverb(2, FPFX "extract, result: %d, mdp: %d, num blocks: %u\n",
         result, metadata_parsed, blocks.size());

  if ((blocks.size() < 3) || (blocks[1].type != FLAC_BLOCK_TYPE_HEADERS))
    mxerror(FPFX "Could not read all header packets.\n");

  FLAC__stream_decoder_reset(decoder);
  FLAC__stream_decoder_delete(decoder);

  file->setFilePointer(0);
  blocks[0].len -= 4;
  blocks[0].filepos = 4;

  if (pass == 1) {
    offset = 20;
    pass_data = (unsigned char *)safemalloc(offset + blocks.size() *
                                            sizeof(flac_block_t));
    put_uint32(&pass_data[0], (uint32_t)metadata_parsed);
    put_uint32(&pass_data[4], channels);
    put_uint32(&pass_data[8], sample_rate);
    put_uint32(&pass_data[12], bits_per_sample);
    put_uint32(&pass_data[16], blocks.size());
    for (i = 0; i < blocks.size(); i++)
      memcpy(&pass_data[offset + i * sizeof(flac_block_t)], &blocks[i],
             sizeof(flac_block_t));
    set_pass_data(ti, pass_data, offset + blocks.size() *
                  sizeof(flac_block_t));
    safefree(pass_data);
  }

  return metadata_parsed;
}

int flac_reader_c::read(generic_packetizer_c *) {
  unsigned char *buf;

  if (current_block == blocks.end())
    return 0;
  buf = (unsigned char *)safemalloc(current_block->len);
  file->setFilePointer(current_block->filepos);
  if (file->read(buf, current_block->len) != current_block->len) {
    safefree(buf);
    return 0;
  }
  packetizer->process(buf, current_block->len, samples * 1000 / sample_rate,
                      current_block->samples * 1000 / sample_rate);
  samples += current_block->samples;
  current_block++;

  if (current_block == blocks.end())
    return 0;
  return EMOREDATA;
}

bool flac_reader_c::fill_buffer() {
  int progress;

  if (pos == size) {
    size = file->read(read_buffer, BUFFER_SIZE);
    progress = (file->getFilePointer() * 100) / file_size;
    if (progress >= (old_progress + 5)) {
      mxinfo("+-> Pre-parsing FLAC file: %d%%\r", progress);
      old_progress = progress;
    }
    if (size == 0)
      return false;
    pos = 0;
    return true;
  }

  return true;
}

FLAC__StreamDecoderReadStatus
flac_reader_c::read_cb(FLAC__byte buffer[], unsigned *bytes) {
  if (done)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  if (!fill_buffer())
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

  buffer[0] = read_buffer[pos];
  pos++;
  *bytes = 1;

  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus
flac_reader_c::write_cb(const FLAC__Frame *frame,
                        const FLAC__int32 * const []) {
  flac_block_t block;

  block.type = FLAC_BLOCK_TYPE_DATA;
  block.filepos = packet_start;
  block.len = (file->getFilePointer() - size + pos) - packet_start;
  block.samples = frame->header.blocksize;
  packet_start = file->getFilePointer() - size + pos;
  blocks.push_back(block);

  mxverb(2, FPFX "write cb, block at %lld with size %d\n", block.filepos,
         block.len);

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void flac_reader_c::metadata_cb(const FLAC__StreamMetadata *metadata) {
  flac_block_t block;

  block.type = FLAC_BLOCK_TYPE_HEADERS;
  block.filepos = packet_start;
  block.len = (file->getFilePointer() - size + pos) - packet_start;
  packet_start = file->getFilePointer() - size + pos;
  blocks.push_back(block);

  mxverb(2, FPFX "metadata cb, block at %lld with size %d\n", block.filepos,
         block.len);

  switch (metadata->type) {
    case FLAC__METADATA_TYPE_STREAMINFO:
      mxverb(2, FPFX "STREAMINFO block (%u bytes):\n", metadata->length);
      mxverb(2, FPFX "  sample_rate: %u Hz\n",
             metadata->data.stream_info.sample_rate);
      sample_rate = metadata->data.stream_info.sample_rate;
      mxverb(2, FPFX "  channels: %u\n", metadata->data.stream_info.channels);
      channels = metadata->data.stream_info.channels;
      mxverb(2, FPFX "  bits_per_sample: %u\n",
             metadata->data.stream_info.bits_per_sample);
      bits_per_sample = metadata->data.stream_info.bits_per_sample;
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

void flac_reader_c::error_cb(FLAC__StreamDecoderErrorStatus status) {
  mxerror(FPFX "Error parsing the file: %d\n", (int)status);
}

int flac_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void flac_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %u/%u blocks (100%%)\r", blocks.size(), blocks.size());
  else
    mxinfo("progress: %d/%u blocks (%d%%)\r",
           distance(blocks.begin(), current_block),
           blocks.size(), distance(blocks.begin(), current_block) * 100 /
           blocks.size());
}

void flac_reader_c::set_headers() {
  packetizer->set_headers();
}

void flac_reader_c::identify() {
  mxinfo("File '%s': container: FLAC\nTrack ID 0: audio (FLAC)\n", ti->fname);
}

#else  // HAVE_FLAC_FORMAT_H

int flac_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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
