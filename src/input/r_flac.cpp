/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   OGG media stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common/matroska.h"
#include "input/flac_common.h"
#include "input/r_flac.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"

#define BUFFER_SIZE 4096

#if defined(HAVE_FLAC_FORMAT_H)

static FLAC__StreamDecoderReadStatus
flac_read_cb(const FLAC__StreamDecoder *,
             FLAC__byte buffer[],
             size_t *bytes,
             void *client_data) {
  return ((flac_reader_c *)client_data)->read_cb(buffer, bytes);
}

static FLAC__StreamDecoderWriteStatus
flac_write_cb(const FLAC__StreamDecoder *,
              const FLAC__Frame *frame,
              const FLAC__int32 * const data[],
              void *client_data) {
  return ((flac_reader_c *)client_data)->write_cb(frame, data);
}

static void
flac_metadata_cb(const FLAC__StreamDecoder *,
                 const FLAC__StreamMetadata *metadata,
                 void *client_data) {
  ((flac_reader_c *)client_data)->metadata_cb(metadata);
}

static void
flac_error_cb(const FLAC__StreamDecoder *,
              FLAC__StreamDecoderErrorStatus status,
              void *client_data) {
  ((flac_reader_c *)client_data)->error_cb(status);
}

static FLAC__StreamDecoderSeekStatus
flac_seek_cb(const FLAC__StreamDecoder *,
             FLAC__uint64 absolute_byte_offset,
             void *client_data) {
  return ((flac_reader_c *)client_data)->seek_cb(absolute_byte_offset);
}

static FLAC__StreamDecoderTellStatus
flac_tell_cb(const FLAC__StreamDecoder *,
             FLAC__uint64 *absolute_byte_offset,
             void *client_data) {
  return ((flac_reader_c *)client_data)->tell_cb(*absolute_byte_offset);
}

static FLAC__StreamDecoderLengthStatus
flac_length_cb(const FLAC__StreamDecoder *,
               FLAC__uint64 *stream_length,
               void *client_data) {
  return ((flac_reader_c *)client_data)->length_cb(*stream_length);
}

static FLAC__bool
flac_eof_cb(const FLAC__StreamDecoder *,
            void *client_data) {
  return ((flac_reader_c *)client_data)->eof_cb();
}

int
flac_reader_c::probe_file(mm_io_c *io,
                          uint64_t size) {
  unsigned char data[4];

  if (4 > size)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(data, 4) != 4)
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }

  if (strncmp((char *)data, "fLaC", 4))
    return 0;

  return 1;
}

flac_reader_c::flac_reader_c(const track_info_c &ti,
                             const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , samples(0)
{
}

void
flac_reader_c::read_headers() {
  if (g_identifying)
    return;

  show_demuxer_info();

  if (!parse_file())
    throw mtx::input::header_parsing_x();

  try {
    uint32_t block_size = 0;

    for (current_block = blocks.begin(); (current_block != blocks.end()) && (FLAC_BLOCK_TYPE_HEADERS == current_block->type); current_block++)
      block_size += current_block->len;

    m_header = memory_c::alloc(block_size);

    block_size         = 0;
    for (current_block = blocks.begin(); (current_block != blocks.end()) && (FLAC_BLOCK_TYPE_HEADERS == current_block->type); current_block++) {
      m_in->setFilePointer(current_block->filepos);
      if (m_in->read(m_header->get_buffer() + block_size, current_block->len) != current_block->len)
        mxerror(Y("flac_reader: Could not read a header packet.\n"));
      block_size += current_block->len;
    }

  } catch (mtx::exception &) {
    mxerror(Y("flac_reader: could not initialize the FLAC packetizer.\n"));
  }
}

flac_reader_c::~flac_reader_c() {
}

void
flac_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new flac_packetizer_c(this, m_ti, m_header->get_buffer(), m_header->get_size()));
  show_packetizer_info(0, PTZR0);
}

bool
flac_reader_c::parse_file() {
  FLAC__StreamDecoder *decoder;
  FLAC__StreamDecoderState state;
  flac_block_t block;
  uint64_t u, old_pos;
  int result, progress, old_progress;
  bool ok;

  m_in->setFilePointer(0);
  metadata_parsed = false;

  mxinfo(Y("+-> Parsing the FLAC file. This can take a LONG time.\n"));
  decoder = FLAC__stream_decoder_new();
  if (decoder == NULL)
    mxerror(Y("flac_reader: FLAC__stream_decoder_new() failed.\n"));
  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(Y("flac_reader: Could not set metadata_respond_all.\n"));
  if (FLAC__stream_decoder_init_stream(decoder, flac_read_cb, flac_seek_cb, flac_tell_cb, flac_length_cb, flac_eof_cb, flac_write_cb,
                                       flac_metadata_cb, flac_error_cb, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    mxerror(Y("flac_reader: Could not initialize the FLAC decoder.\n"));

  result = (int)FLAC__stream_decoder_process_until_end_of_metadata(decoder);

  mxverb(2, boost::format("flac_reader: extract->metadata, result: %1%, mdp: %2%, num blocks: %3%\n") % result % metadata_parsed % blocks.size());

  if (!metadata_parsed)
    mxerror_fn(m_ti.m_fname, Y("No metadata block found. This file is broken.\n"));

  FLAC__stream_decoder_get_decode_position(decoder, &u);

  block.type    = FLAC_BLOCK_TYPE_HEADERS;
  block.filepos = 0;
  block.len     = u;
  old_pos       = u;

  blocks.push_back(block);

  mxverb(2, boost::format("flac_reader: headers: block at %1% with size %2%\n") % block.filepos % block.len);

  old_progress = -5;
  ok = FLAC__stream_decoder_skip_single_frame(decoder);
  while (ok) {
    state = FLAC__stream_decoder_get_state(decoder);

    progress = (int)(m_in->getFilePointer() * 100 / m_size);
    if ((progress - old_progress) >= 5) {
      mxinfo(boost::format(Y("+-> Pre-parsing FLAC file: %1%%%%2%")) % progress % "\r");
      old_progress = progress;
    }

    if (FLAC__stream_decoder_get_decode_position(decoder, &u) && (u != old_pos)) {
      block.type    = FLAC_BLOCK_TYPE_DATA;
      block.filepos = old_pos;
      block.len     = u - old_pos;
      old_pos       = u;
      blocks.push_back(block);

      mxverb(2, boost::format("flac_reader: skip/decode frame, block at %1% with size %2%\n") % block.filepos % block.len);
    }

    if (state > FLAC__STREAM_DECODER_READ_FRAME)
      break;

    ok = FLAC__stream_decoder_skip_single_frame(decoder);
  }

  if (100 != old_progress)
    mxinfo(Y("+-> Pre-parsing FLAC file: 100%\n"));
  else
    mxinfo("\n");

  if ((blocks.size() == 0) || (blocks[0].type != FLAC_BLOCK_TYPE_HEADERS))
    mxerror(Y("flac_reader: Could not read all header packets.\n"));

  FLAC__stream_decoder_reset(decoder);
  FLAC__stream_decoder_delete(decoder);

  m_in->setFilePointer(0);
  blocks[0].len     -= 4;
  blocks[0].filepos  = 4;

  return metadata_parsed;
}

file_status_e
flac_reader_c::read(generic_packetizer_c *,
                    bool) {
  if (current_block == blocks.end())
    return flush_packetizers();

  memory_cptr buf = memory_c::alloc(current_block->len);
  m_in->setFilePointer(current_block->filepos);
  if (m_in->read(buf, current_block->len) != current_block->len)
    return flush_packetizers();

  unsigned int samples_here = flac_get_num_samples(buf->get_buffer(), current_block->len, stream_info);
  PTZR0->process(new packet_t(buf, samples * 1000000000 / sample_rate));

  samples += samples_here;
  current_block++;

  return (current_block == blocks.end()) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

FLAC__StreamDecoderReadStatus
flac_reader_c::read_cb(FLAC__byte buffer[],
                       size_t *bytes)
{
  unsigned bytes_read, wanted_bytes;

  wanted_bytes = *bytes;
  bytes_read   = m_in->read((unsigned char *)buffer, wanted_bytes);
  *bytes       = bytes_read;

  return bytes_read == wanted_bytes ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
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
      memcpy(&stream_info, &metadata->data.stream_info, sizeof(FLAC__StreamMetadata_StreamInfo));
      sample_rate     = metadata->data.stream_info.sample_rate;
      metadata_parsed = true;

      mxverb(2, boost::format("flac_reader: STREAMINFO block (%1% bytes):\n") % metadata->length);
      mxverb(2, boost::format("flac_reader:   sample_rate: %1% Hz\n")         % metadata->data.stream_info.sample_rate);
      mxverb(2, boost::format("flac_reader:   channels: %1%\n")               % metadata->data.stream_info.channels);
      mxverb(2, boost::format("flac_reader:   bits_per_sample: %1%\n")        % metadata->data.stream_info.bits_per_sample);

      break;
    default:
      mxverb(2,
             boost::format("%1% (%2%) block (%3% bytes)\n")
             % (  metadata->type == FLAC__METADATA_TYPE_PADDING        ? "PADDING"
                : metadata->type == FLAC__METADATA_TYPE_APPLICATION    ? "APPLICATION"
                : metadata->type == FLAC__METADATA_TYPE_SEEKTABLE      ? "SEEKTABLE"
                : metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT ? "VORBIS COMMENT"
                : metadata->type == FLAC__METADATA_TYPE_CUESHEET       ? "CUESHEET"
                :                                                        "UNDEFINED")
             % metadata->type % metadata->length);
      break;
  }
}

void
flac_reader_c::error_cb(FLAC__StreamDecoderErrorStatus status) {
  mxerror(boost::format(Y("flac_reader: Error parsing the file: %1%\n")) % (int)status);
}

FLAC__StreamDecoderSeekStatus
flac_reader_c::seek_cb(uint64_t new_pos) {
  m_in->setFilePointer(new_pos, seek_beginning);
  if (m_in->getFilePointer() == new_pos)
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
  return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

FLAC__StreamDecoderTellStatus
flac_reader_c::tell_cb(uint64_t &absolute_byte_offset) {
  absolute_byte_offset = m_in->getFilePointer();
  return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus
flac_reader_c::length_cb(uint64_t &stream_length) {
  stream_length = m_size;
  return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

FLAC__bool
flac_reader_c::eof_cb() {
  return m_in->getFilePointer() >= m_size;
}

void
flac_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "FLAC");
}

#else  // HAVE_FLAC_FORMAT_H

int
flac_reader_c::probe_file(mm_io_c *in,
                          uint64_t size) {
  unsigned char data[4];

  if (4 > size)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);
    if (in->read(data, 4) != 4)
      return 0;
    in->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (strncmp((char *)data, "fLaC", 4))
    return 0;
  id_result_container_unsupported(in->get_file_name(), "FLAC");
  return 1;
}

#endif // HAVE_FLAC_FORMAT_H
