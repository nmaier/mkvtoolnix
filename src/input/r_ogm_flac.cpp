/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id: r_ogm.cpp 3743 2008-01-02 12:09:14Z mosu $

   OGG media stream reader -- FLAC support

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#if defined(HAVE_FLAC_FORMAT_H)

#if defined(HAVE_FLAC_FORMAT_H)
# include <FLAC/stream_decoder.h>
# if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 8
#  define LEGACY_FLAC
# else
#  undef LEGACY_FLAC
# endif
#endif

#include "p_flac.h"
#include "r_ogm.h"
#include "r_ogm_flac.h"

#define FPFX        "flac_header_extraction: "
#define BUFFER_SIZE 4096

static FLAC__StreamDecoderReadStatus
fhe_read_cb(const FLAC__StreamDecoder *decoder,
            FLAC__byte buffer[],
#ifdef LEGACY_FLAC
            unsigned *bytes,
#else
            size_t *bytes,
#endif
            void *client_data) {
  ogg_packet op;

  flac_header_extractor_c *fhe = (flac_header_extractor_c *)client_data;
  if (fhe->done)
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

  if (ogg_stream_packetout(&fhe->os, &op) != 1)
    if (!fhe->read_page() || (ogg_stream_packetout(&fhe->os, &op) != 1))
      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

  if (*bytes < op.bytes)
    mxerror(FPFX "bytes (%u) < op.bytes (%ld). Could not read the FLAC headers.\n", (unsigned int)*bytes, (long)op.bytes);

  memcpy(buffer, op.packet, op.bytes);
  *bytes = op.bytes;
  fhe->num_packets++;
  mxverb(2, FPFX "read packet number " LLD " with %ld bytes\n", fhe->num_packets, op.bytes);

  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus
fhe_write_cb(const FLAC__StreamDecoder *,
             const FLAC__Frame *,
             const FLAC__int32 * const [],
             void *client_data) {
  mxverb(2, FPFX "write cb\n");

  ((flac_header_extractor_c *)client_data)->done = true;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void
fhe_metadata_cb(const FLAC__StreamDecoder *decoder,
                const FLAC__StreamMetadata *metadata,
                void *client_data) {

  flac_header_extractor_c *fhe = (flac_header_extractor_c *)client_data;
  fhe->num_header_packets      = fhe->num_packets;

  mxverb(2, FPFX "metadata cb\n");

  switch (metadata->type) {
    case FLAC__METADATA_TYPE_STREAMINFO:
      fhe->sample_rate     = metadata->data.stream_info.sample_rate;
      fhe->channels        = metadata->data.stream_info.channels;
      fhe->bits_per_sample = metadata->data.stream_info.bits_per_sample;
      fhe->metadata_parsed = true;

      mxverb(2, FPFX "STREAMINFO block (%u bytes):\n", metadata->length);
      mxverb(2, FPFX "  sample_rate: %u Hz\n",         metadata->data.stream_info.sample_rate);
      mxverb(2, FPFX "  channels: %u\n",               metadata->data.stream_info.channels);
      mxverb(2, FPFX "  bits_per_sample: %u\n",        metadata->data.stream_info.bits_per_sample);
      break;

    default:
      mxverb(2, "%s (%u) block (%u bytes)\n",
               metadata->type == FLAC__METADATA_TYPE_PADDING        ? "PADDING"
             : metadata->type == FLAC__METADATA_TYPE_APPLICATION    ? "APPLICATION"
             : metadata->type == FLAC__METADATA_TYPE_SEEKTABLE      ? "SEEKTABLE"
             : metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT ? "VORBIS COMMENT"
             : metadata->type == FLAC__METADATA_TYPE_CUESHEET       ? "CUESHEET"
             :                                                        "UNDEFINED",
             metadata->type, metadata->length);
      break;
  }
}

static void
fhe_error_cb(const FLAC__StreamDecoder *,
             FLAC__StreamDecoderErrorStatus status,
             void *client_data) {
  ((flac_header_extractor_c *)client_data)->done = true;
  mxverb(2, FPFX "error (%d)\n", (int)status);
}

flac_header_extractor_c::flac_header_extractor_c(const string &file_name,
                                                 int64_t _sid):
  metadata_parsed(false),
  sid(_sid),
  num_packets(0),
  num_header_packets(0),
  done(false) {

  file    = new mm_file_io_c(file_name);
  decoder = FLAC__stream_decoder_new();

  if (NULL == decoder)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");

#ifdef LEGACY_FLAC
  FLAC__stream_decoder_set_client_data(decoder, this);
  if (!FLAC__stream_decoder_set_read_callback(decoder, fhe_read_cb))
    mxerror(FPFX "Could not set the read callback.\n");
  if (!FLAC__stream_decoder_set_write_callback(decoder, fhe_write_cb))
    mxerror(FPFX "Could not set the write callback.\n");
  if (!FLAC__stream_decoder_set_metadata_callback(decoder, fhe_metadata_cb))
    mxerror(FPFX "Could not set the metadata callback.\n");
  if (!FLAC__stream_decoder_set_error_callback(decoder, fhe_error_cb))
    mxerror(FPFX "Could not set the error callback.\n");
#endif

  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");

#ifdef LEGACY_FLAC
  if (FLAC__stream_decoder_init(decoder) !=
      FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");
#else
  if (FLAC__stream_decoder_init_stream(decoder, fhe_read_cb, NULL, NULL, NULL, NULL, fhe_write_cb, fhe_metadata_cb, fhe_error_cb, this) !=
      FLAC__STREAM_DECODER_INIT_STATUS_OK)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");
#endif

  ogg_sync_init(&oy);
}

flac_header_extractor_c::~flac_header_extractor_c() {
  FLAC__stream_decoder_reset(decoder);
  FLAC__stream_decoder_delete(decoder);

  ogg_sync_clear(&oy);
  ogg_stream_clear(&os);

  delete file;
}

bool
flac_header_extractor_c::extract() {
  mxverb(2, FPFX "extract\n");
  if (!read_page()) {
    mxverb(2, FPFX "read_page() failed.\n");
    return false;
  }

  int result = (int)FLAC__stream_decoder_process_until_end_of_stream(decoder);

  mxverb(2, FPFX "extract, result: %d, mdp: %d, num_header_packets: " LLD "\n", result, metadata_parsed, num_header_packets);

  return metadata_parsed;
}

bool
flac_header_extractor_c::read_page() {
  while (1) {
    int np = ogg_sync_pageseek(&oy, &og);

    if (np <= 0) {
      if (np < 0)
        return false;

      unsigned char *buf = (unsigned char *)ogg_sync_buffer(&oy, BUFFER_SIZE);
      if (!buf)
        return false;

      int nread;
      if ((nread = file->read(buf, BUFFER_SIZE)) <= 0)
        return false;

      ogg_sync_wrote(&oy, nread);

    } else if (ogg_page_serialno(&og) == sid)
      break;
  }

  if (ogg_page_bos(&og))
    ogg_stream_init(&os, sid);
  ogg_stream_pagein(&os, &og);

  return true;
}

// ------------------------------------------

ogm_a_flac_demuxer_c::ogm_a_flac_demuxer_c(ogm_reader_c *p_reader):
  ogm_demuxer_c(p_reader),
  flac_header_packets(0),
  sample_rate(0),
  channels(0),
  bits_per_sample(0) {

  stype = OGM_STREAM_TYPE_A_FLAC;
}

void
ogm_a_flac_demuxer_c::process_page(int64_t granulepos) {
  ogg_packet op;

  while (ogg_stream_packetout(&os, &op) == 1) {
    eos |= op.e_o_s;

    units_processed++;
    if (units_processed <= flac_header_packets)
      continue;

    for (int i = 0; i < (int)nh_packet_data.size(); i++) {
      memory_c *mem = nh_packet_data[i]->clone();
      reader->reader_packetizers[ptzr]->process(new packet_t(mem, 0));
    }

    nh_packet_data.clear();

    if (-1 == last_granulepos)
      reader->reader_packetizers[ptzr]->process(new packet_t(new memory_c(op.packet, op.bytes, false), -1));
    else {
      reader->reader_packetizers[ptzr]->process(new packet_t(new memory_c(op.packet, op.bytes, false), last_granulepos * 1000000000 / sample_rate));
      last_granulepos = granulepos;
    }
  }
}

void
ogm_a_flac_demuxer_c::process_header_page() {
  ogg_packet op;

  while ((packet_data.size() < flac_header_packets) && (ogg_stream_packetout(&os, &op) == 1)) {
    eos |= op.e_o_s;
    packet_data.push_back(clone_memory(op.packet, op.bytes));
  }

  if (packet_data.size() >= flac_header_packets)
    headers_read = true;

  while (ogg_stream_packetout(&os, &op) == 1) {
    nh_packet_data.push_back(clone_memory(op.packet, op.bytes));
  }
}

void
ogm_a_flac_demuxer_c::initialize() {
  flac_header_extractor_c fhe(reader->ti.fname, serialno);

  if (!fhe.extract())
    mxerror("ogm_reader: Could not read the FLAC header packets for stream id " LLD ".\n", (int64_t)track_id);

  flac_header_packets = fhe.num_header_packets;
  sample_rate         = fhe.sample_rate;
  channels            = fhe.channels;
  bits_per_sample     = fhe.bits_per_sample;
  last_granulepos     = 0;
  units_processed     = 1;
}

generic_packetizer_c *
ogm_a_flac_demuxer_c::create_packetizer(track_info_c &ti) {
  int size = 0;
  int i;

  for (i = 1; i < (int)packet_data.size(); i++)
    size += packet_data[i]->get_size();

  unsigned char *buf = (unsigned char *)safemalloc(size);
  size               = 0;

  for (i = 1; i < (int)packet_data.size(); i++) {
    memcpy(&buf[size], packet_data[i]->get(), packet_data[i]->get_size());
    size += packet_data[i]->get_size();
  }

  generic_packetizer_c *ptzr_obj = new flac_packetizer_c(reader, buf, size, ti);
  safefree(buf);

  mxinfo(FMT_TID "Using the FLAC output module.\n", ti.fname.c_str(), (int64_t)ti.id);

  return ptzr_obj;
}

#endif // HAVE_FLAC_FORMAT_H
