/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ogm.cpp

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

extern "C" {                    // for BITMAPINFOHEADER
#include "avilib.h"
}

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "ogmstreams.h"
#include "matroska.h"
#include "r_ogm.h"
#include "p_vorbis.h"
#include "p_video.h"
#include "p_pcm.h"
#include "p_textsubs.h"
#include "p_mp3.h"
#include "p_ac3.h"
#if defined(HAVE_FLAC_FORMAT_H)
#include "p_flac.h"
#endif

#define BUFFER_SIZE 4096

#if defined(HAVE_FLAC_FORMAT_H)
#define FPFX "flac_header_extraction: "

static FLAC__StreamDecoderReadStatus
fhe_read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
            unsigned *bytes, void *client_data) {
  flac_header_extractor_c *fhe;
  ogg_packet op;

  fhe = (flac_header_extractor_c *)client_data;
  if (fhe->done)
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  if (ogg_stream_packetout(&fhe->os, &op) != 1) {
    if (!fhe->read_page() || (ogg_stream_packetout(&fhe->os, &op) != 1))
      return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }

  if (*bytes < op.bytes)
    mxerror(FPFX "bytes (%u) < op.bytes (%u). Could not read the FLAC "
            "headers.\n", *bytes, op.bytes);
  memcpy(buffer, op.packet, op.bytes);
  *bytes = op.bytes;
  fhe->num_packets++;
  mxverb(2, FPFX "read packet number %lld with %u bytes\n", fhe->num_packets,
         op.bytes);

  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__StreamDecoderWriteStatus
fhe_write_cb(const FLAC__StreamDecoder *, const FLAC__Frame *,
             const FLAC__int32 * const [], void *client_data) {
  mxverb(2, FPFX "write cb\n");

  ((flac_header_extractor_c *)client_data)->done = true;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void fhe_metadata_cb(const FLAC__StreamDecoder *decoder,
                            const FLAC__StreamMetadata *metadata,
                            void *client_data) {
  flac_header_extractor_c *fhe;

  fhe = (flac_header_extractor_c *)client_data;
  fhe->num_header_packets = fhe->num_packets;
  mxverb(2, FPFX "metadata cb\n");
  switch (metadata->type) {
    case FLAC__METADATA_TYPE_STREAMINFO:
      mxverb(2, FPFX "STREAMINFO block (%u bytes):\n", metadata->length);
      mxverb(2, FPFX "  sample_rate: %u Hz\n",
             metadata->data.stream_info.sample_rate);
      fhe->sample_rate = metadata->data.stream_info.sample_rate;
      mxverb(2, FPFX "  channels: %u\n", metadata->data.stream_info.channels);
      fhe->channels = metadata->data.stream_info.channels;
      mxverb(2, FPFX "  bits_per_sample: %u\n",
             metadata->data.stream_info.bits_per_sample);
      fhe->bits_per_sample = metadata->data.stream_info.bits_per_sample;
      fhe->metadata_parsed = true;
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

static void fhe_error_cb(const FLAC__StreamDecoder *,
                         FLAC__StreamDecoderErrorStatus status,
                         void *client_data) {
  ((flac_header_extractor_c *)client_data)->done = true;
  mxverb(2, FPFX "error (%d)\n", (int)status);
}

flac_header_extractor_c::flac_header_extractor_c(const char *file_name,
                                                 int64_t nsid):
  metadata_parsed(false), sid(nsid), num_packets(0), num_header_packets(0),
  done(false) {
  file = new mm_io_c(file_name, MODE_READ);
  decoder = FLAC__stream_decoder_new();
  if (decoder == NULL)
    mxerror(FPFX "FLAC__stream_decoder_new() failed.\n");
  FLAC__stream_decoder_set_client_data(decoder, this);
  if (!FLAC__stream_decoder_set_read_callback(decoder, fhe_read_cb))
    mxerror(FPFX "Could not set the read callback.\n");
  if (!FLAC__stream_decoder_set_write_callback(decoder, fhe_write_cb))
    mxerror(FPFX "Could not set the write callback.\n");
  if (!FLAC__stream_decoder_set_metadata_callback(decoder, fhe_metadata_cb))
    mxerror(FPFX "Could not set the metadata callback.\n");
  if (!FLAC__stream_decoder_set_error_callback(decoder, fhe_error_cb))
    mxerror(FPFX "Could not set the error callback.\n");
  if (!FLAC__stream_decoder_set_metadata_respond_all(decoder))
    mxerror(FPFX "Could not set metadata_respond_all.\n");
  if (FLAC__stream_decoder_init(decoder) !=
      FLAC__STREAM_DECODER_SEARCH_FOR_METADATA)
    mxerror(FPFX "Could not initialize the FLAC decoder.\n");
  ogg_sync_init(&oy);
}

flac_header_extractor_c::~flac_header_extractor_c() {
  FLAC__stream_decoder_reset(decoder);
  FLAC__stream_decoder_delete(decoder);
  ogg_sync_clear(&oy);
  ogg_stream_clear(&os);
  delete file;
}

bool flac_header_extractor_c::extract() {
  int result;

  mxverb(2, FPFX "extract\n");
  if (!read_page()) {
    mxverb(2, FPFX "read_page() failed.\n");
    return false;
  }
  result = (int)FLAC__stream_decoder_process_until_end_of_stream(decoder);
  mxverb(2, FPFX "extract, result: %d, mdp: %d, num_header_packets: %u\n",
         result, metadata_parsed, num_header_packets);

  return metadata_parsed;
}

bool flac_header_extractor_c::read_page() {
  int np, nread;
  unsigned char *buf;

  while (1) {
    np = ogg_sync_pageseek(&oy, &og);

    if (np <= 0) {
      if (np < 0)
        return false;
      buf = (unsigned char *)ogg_sync_buffer(&oy, BUFFER_SIZE);
      if (!buf)
        return false;

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
#endif // HAVE_FLAC_FORMAT_H

/*
 * Probes a file by simply comparing the first four bytes to 'OggS'.
 */
int ogm_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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
  if (strncmp((char *)data, "OggS", 4))
    return 0;
  return 1;
}

/*
 * Opens the file for processing, initializes an ogg_sync_state used for
 * reading from an OGG stream.
 */
ogm_reader_c::ogm_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    file_size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("ogm_reader: Could not open the source file.");
  }
  if (!ogm_reader_c::probe_file(mm_io, file_size))
    throw error_c("ogm_reader: Source is not a valid OGG media file.");

  ogg_sync_init(&oy);
  act_wchar = 0;
  sdemuxers = 0;
  nastreams = 0;
  nvstreams = 0;
  ntstreams = 0;
  numstreams = 0;
  sdemuxers = NULL;
  num_sdemuxers = 0;

  if (verbose)
    mxinfo("Using OGG/OGM demultiplexer for %s.\n", ti->fname);

  if (read_headers() <= 0)
    throw error_c("ogm_reader: Could not read all header packets.");
  create_packetizers();
}

ogm_reader_c::~ogm_reader_c() {
  int i;
  ogm_demuxer_t *dmx;

  ogg_sync_clear(&oy);

  for (i = 0; i < num_sdemuxers; i++) {
    dmx = sdemuxers[i];
    ogg_stream_clear(&dmx->os);
    delete dmx->packetizer;
    delete dmx;
  }
  safefree(sdemuxers);
  ti->private_data = NULL;
}

ogm_demuxer_t *ogm_reader_c::find_demuxer(int serialno) {
  int i;

  for (i = 0; i < num_sdemuxers; i++)
    if (sdemuxers[i]->serial == serialno)
      return sdemuxers[i];

  return NULL;
}

void ogm_reader_c::free_demuxer(int idx) {
  if (idx >= num_sdemuxers)
    return;
  delete sdemuxers[idx];

  memmove(&sdemuxers[idx], &sdemuxers[idx + 1], num_sdemuxers - idx - 1);
  num_sdemuxers--;
}

/*
 * Reads an OGG page from the stream. Returns 0 if there are no more pages
 * left, EMOREDATA otherwise.
 */
int ogm_reader_c::read_page(ogg_page *og) {
  int np, done, nread;
  unsigned char *buf;

  done = 0;
  while (!done) {
    np = ogg_sync_pageseek(&oy, og);

    // np == 0 means that there is not enough data for a complete page.
    if (np <= 0) {
      // np < 0 is the error case. Should not happen with local OGG files.
      if (np < 0)
        mxwarn("ogm_reader: Could not find the next Ogg "
               "page. This indicates a damaged Ogg/Ogm file. Will try to "
               "continue.\n");

      buf = (unsigned char *)ogg_sync_buffer(&oy, BUFFER_SIZE);
      if (!buf)
        mxerror("ogm_reader: ogg_sync_buffer failed\n");

      if ((nread = mm_io->read(buf, BUFFER_SIZE)) <= 0)
        return 0;

      ogg_sync_wrote(&oy, nread);
    } else
      // Alright, we have a page.
      done = 1;
  }

  // Here EMOREDATA actually indicates success - a page has been read.
  return EMOREDATA;
}

void ogm_reader_c::add_new_demuxer(ogm_demuxer_t *dmx) {
  sdemuxers = (ogm_demuxer_t **)saferealloc(sdemuxers, sizeof(ogm_demuxer_t *)
                                            * (num_sdemuxers + 1));
  sdemuxers[num_sdemuxers] = dmx;
  num_sdemuxers++;
}

void ogm_reader_c::create_packetizers() {
  vorbis_info vi;
  vorbis_comment vc;
  ogg_packet op;
  alBITMAPINFOHEADER bih;
  stream_header *sth;
  int i;
  ogm_demuxer_t *dmx;

  memset(&bih, 0, sizeof(alBITMAPINFOHEADER));
  i = 0;
  while (i < num_sdemuxers) {
    dmx = sdemuxers[i];
    sth = (stream_header *)&dmx->packet_data[0][1];
    ti->private_data = NULL;
    ti->private_size = 0;
    ti->id = dmx->serial;       // ID for this track.

    switch (dmx->stype) {
      case OGM_STREAM_TYPE_VIDEO:
        if (ti->fourcc[0] == 0) {
          memcpy(ti->fourcc, sth->subtype, 4);
          ti->fourcc[4] = 0;
        }
        // AVI compatibility mode. Fill in the values we've got and guess
        // the others.
        bih.bi_size = sizeof(alBITMAPINFOHEADER);
        bih.bi_width = get_uint32(&sth->sh.video.width);
        bih.bi_height = get_uint32(&sth->sh.video.height);
        bih.bi_planes = 1;
        bih.bi_bit_count = 24;
        memcpy(&bih.bi_compression, ti->fourcc, 4);
        bih.bi_size_image = bih.bi_width * bih.bi_height * 3;
        ti->private_data = (unsigned char *)&bih;
        ti->private_size = sizeof(alBITMAPINFOHEADER);

        try {
          dmx->packetizer =
            new video_packetizer_c(this, NULL, (double)10000000 /
                                   (double)get_uint64(&sth->time_unit),
                                   get_uint32(&sth->sh.video.width),
                                   get_uint32(&sth->sh.video.height),
                                   false, ti);
        } catch (error_c &error) {
          mxwarn("ogm_reader: could not initialize video "
                 "packetizer for stream id %d. Will try to continue and "
                 "ignore this stream.\n", dmx->serial);
          delete dmx;
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using video output "
                 "module for stream %d.\n", ti->fname, dmx->serial);

        break;

      case OGM_STREAM_TYPE_PCM:
        try {
          dmx->packetizer =
            new pcm_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                 get_uint16(&sth->sh.audio.channels),
                                 get_uint16(&sth->bits_per_sample), ti);
        } catch (error_c &error) {
          mxwarn("ogm_reader: could not initialize PCM "
                 "packetizer for stream id %d. Will try to continue and "
                 "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using PCM output "
                 "module for stream %d.\n", ti->fname, dmx->serial);
        break;

      case OGM_STREAM_TYPE_MP3:
        try {
          dmx->packetizer =
            new mp3_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                 get_uint16(&sth->sh.audio.channels), ti);
        } catch (error_c &error) {
          mxwarn("Error: ogm_reader: could not initialize MP3 "
                 "packetizer for stream id %d. Will try to continue and "
                 "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using MP3 output "
                 "module for stream %d.\n", ti->fname, dmx->serial);
        break;

      case OGM_STREAM_TYPE_AC3:
        try {
          dmx->packetizer =
            new ac3_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                 get_uint16(&sth->sh.audio.channels), 0, ti);
        } catch (error_c &error) {
          mxwarn("ogm_reader: could not initialize AC3 "
                 "packetizer for stream id %d. Will try to continue and "
                 "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using AC3 output "
                 "module for stream %d.\n", ti->fname, dmx->serial);

        break;

      case OGM_STREAM_TYPE_VORBIS:
        vorbis_info_init(&vi);
        vorbis_comment_init(&vc);
        memset(&op, 0, sizeof(ogg_packet));
        op.packet = dmx->packet_data[0];
        op.bytes = dmx->packet_sizes[0];
        op.b_o_s = 1;
        vorbis_synthesis_headerin(&vi, &vc, &op);
        dmx->vorbis_rate = vi.rate;
        vorbis_info_clear(&vi);
        vorbis_comment_clear(&vc);
        try {
          dmx->packetizer =
            new vorbis_packetizer_c(this,
                                    dmx->packet_data[0], dmx->packet_sizes[0],
                                    dmx->packet_data[1], dmx->packet_sizes[1],
                                    dmx->packet_data[2], dmx->packet_sizes[2],
                                    ti);
        } catch (error_c &error) {
          mxwarn("Error: ogm_reader: could not initialize Vorbis "
                 "packetizer for stream id %d. Will try to continue and "
                 "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using Vorbis output "
                 "module for stream %d.\n", ti->fname, dmx->serial);

        break;

      case OGM_STREAM_TYPE_TEXT:
        try {
          dmx->packetizer = new textsubs_packetizer_c(this, MKV_S_TEXTUTF8,
                                                      NULL, 0, true, false,
                                                      ti);
        } catch (error_c &error) {
          mxwarn("ogm_reader: could not initialize the "
                 "text subtitles packetizer for stream id %d. Will try to "
                 "continue and ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using text subtitle "
                 "output module for stream %d.\n", ti->fname, dmx->serial);

        break;

#if defined(HAVE_FLAC_FORMAT_H)
      case OGM_STREAM_TYPE_FLAC:
        try {
          unsigned char *buf;
          int size;

          size = 0;
          for (i = 1; i < (int)dmx->packet_sizes.size(); i++)
            size += dmx->packet_sizes[i];
          buf = (unsigned char *)safemalloc(size);
          size = 0;
          for (i = 1; i < (int)dmx->packet_sizes.size(); i++) {
            memcpy(&buf[size], dmx->packet_data[i], dmx->packet_sizes[i]);
            size += dmx->packet_sizes[i];
          }
          dmx->packetizer =
            new flac_packetizer_c(this, dmx->vorbis_rate, dmx->channels,
                                  dmx->bits_per_sample, buf, size, ti);
          safefree(buf);

        } catch (error_c &error) {
          mxwarn("ogm_reader: could not initialize the "
                 "FLAC packetizer for stream id %d. Will try to "
                 "continue and ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          mxinfo("OGG/OGM demultiplexer (%s): using the FLAC "
                 "output module for stream %d.\n", ti->fname, dmx->serial);

        break;
#endif
      default:
        die("ogm_reader: Don't know how to create a packetizer for stream "
            "type %d.\n", (int)dmx->stype);
        break;

    }
    i++;
  }
}

/*
 * Checks every demuxer if it has a page available.
 */
int ogm_reader_c::packet_available() {
  int i;

  if (num_sdemuxers == 0)
    return 0;

  for (i = 0; i < num_sdemuxers; i++)
    if (!sdemuxers[i]->packetizer->packet_available())
      return 0;

  return 1;
}

/*
 * The page is the beginning of a new stream. Check the contents for known
 * stream headers. If it is a known stream and the user has requested that
 * it should be extracted then allocate a new packetizer based on the
 * stream type and store the needed data in a new ogm_demuxer_t.
 */
void ogm_reader_c::handle_new_stream(ogg_page *og) {
  ogg_stream_state new_oss;
  ogg_packet op;
  ogm_demuxer_t *dmx;
  stream_header *sth;
  char buf[5];
  uint32_t codec_id;

  if (ogg_stream_init(&new_oss, ogg_page_serialno(og))) {
    mxwarn("ogm_reader: ogg_stream_init for stream number "
           "%d failed. Will try to continue and ignore this stream.",
           numstreams + 1);
    return;
  }

  // Read the first page and get its first packet.
  ogg_stream_pagein(&new_oss, og);
  ogg_stream_packetout(&new_oss, &op);

  dmx = new ogm_demuxer_t;
  dmx->packet_data.push_back((unsigned char *)safememdup(op.packet, op.bytes));
  dmx->packet_sizes.push_back(op.bytes);

  /*
   * Check the contents for known stream headers. This one is the
   * standard Vorbis header.
   */
  if ((op.bytes >= 7) && !strncmp((char *)&op.packet[1], "vorbis", 6)) {
    nastreams++;
    numstreams++;
    if (!demuxing_requested('a', ogg_page_serialno(og))) {
      ogg_stream_clear(&new_oss);
      delete dmx;
      return;
    }

    dmx->stype = OGM_STREAM_TYPE_VORBIS;
    dmx->serial = ogg_page_serialno(og);
    memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
    dmx->sid = nastreams;
    add_new_demuxer(dmx);

    return;
  }

  // FLAC
  if ((op.bytes >= 4) && !strncmp((char *)op.packet, "fLaC", 4)) {
    nastreams++;
    numstreams++;
    if (!demuxing_requested('a', ogg_page_serialno(og))) {
      ogg_stream_clear(&new_oss);
      delete dmx;
      return;
    }
    dmx->stype = OGM_STREAM_TYPE_FLAC;
    dmx->serial = ogg_page_serialno(og);
    memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
    dmx->sid = nastreams;
    add_new_demuxer(dmx);

#if !defined(HAVE_FLAC_FORMAT_H)
    mxerror("mkvmerge has not been compiled with FLAC support but handling "
            "of this stream has been requested.\n");
#else
    dmx->fhe = new flac_header_extractor_c(ti->fname, dmx->serial);
    if (!dmx->fhe->extract())
      mxerror("ogm_reader: Could not read the FLAC header packets for "
              "stream id %d.\n", dmx->sid);
    dmx->flac_header_packets = dmx->fhe->num_header_packets;
    dmx->vorbis_rate = dmx->fhe->sample_rate;
    dmx->channels = dmx->fhe->channels;
    dmx->bits_per_sample = dmx->fhe->bits_per_sample;
    dmx->last_granulepos = 0;
    dmx->units_processed = 1;
    delete dmx->fhe;
#endif

    return;
  }

  // The new stream headers introduced by OggDS (see ogmstreams.h).
  if (((*op.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
      (op.bytes >= ((int)sizeof(stream_header) + 1))) {
    sth = (stream_header *)(op.packet + 1);
    if (!strncmp(sth->streamtype, "video", 5)) {
      nvstreams++;
      numstreams++;
      if (!demuxing_requested('v', ogg_page_serialno(og))) {
        ogg_stream_clear(&new_oss);
        delete dmx;
        return;
      }

      dmx->stype = OGM_STREAM_TYPE_VIDEO;
      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = nvstreams;
      add_new_demuxer(dmx);
      if (video_fps < 0)
        video_fps = 10000000.0 / (float)get_uint64(&sth->time_unit);

      return;
    }

    if (!strncmp(sth->streamtype, "audio", 5)) {
      nastreams++;
      numstreams++;
      if (!demuxing_requested('a', ogg_page_serialno(og))) {
        ogg_stream_clear(&new_oss);
        return;
      }

      sth = (stream_header *)&op.packet[1];
      memcpy(buf, (char *)sth->subtype, 4);
      buf[4] = 0;
      codec_id = strtol(buf, (char **)NULL, 16);

      if (codec_id == 0x0001)
        dmx->stype = OGM_STREAM_TYPE_PCM;
      else if (codec_id == 0x0055)
        dmx->stype = OGM_STREAM_TYPE_MP3;
      else if (codec_id == 0x2000)
        dmx->stype = OGM_STREAM_TYPE_AC3;
      else {
        mxwarn("ogm_reader: Unknown audio stream type %u. "
               "Ignoring stream id %d.\n", codec_id, numstreams);
        delete dmx;
        return;
      }

      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = nastreams;
      add_new_demuxer(dmx);

      return;
    }

    if (!strncmp(sth->streamtype, "text", 4)) {

      ntstreams++;
      numstreams++;
      if (!demuxing_requested('s', ogg_page_serialno(og))) {
        ogg_stream_clear(&new_oss);
        return;
      }

      dmx->stype = OGM_STREAM_TYPE_TEXT;
      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = ntstreams;
      add_new_demuxer(dmx);

      return;
    }
  }

  /*
   * The old OggDS headers (see MPlayer's source, libmpdemux/demux_ogg.c)
   * are not supported.
   */

  // Failed to detect a supported header.
  ogg_stream_clear(&new_oss);
  delete dmx;

  return;
}

/*
 * Process the contents of a page. First find the demuxer associated with
 * the page's serial number. If there is no such demuxer then either the
 * OGG file is damaged (very rare) or the page simply belongs to a stream
 * that the user didn't want extracted.
 * If the demuxer is found then hand over all packets in this page to the
 * associated packetizer.
 */
void ogm_reader_c::process_page(ogg_page *og) {
  ogm_demuxer_t *dmx;
  ogg_packet op;
  int hdrlen, eos, i;
  long lenbytes;

  lenbytes = 0;
  dmx = find_demuxer(ogg_page_serialno(og));
  if (dmx == NULL)
    return;

  debug_enter("ogm_reader_c::process_page");

  ogg_stream_pagein(&dmx->os, og);
  while (ogg_stream_packetout(&dmx->os, &op) == 1) {
    eos = op.e_o_s;

#if defined(HAVE_FLAC_FORMAT_H)
    if (dmx->stype == OGM_STREAM_TYPE_FLAC) {
      dmx->units_processed++;
      if (dmx->units_processed <= dmx->flac_header_packets)
        continue;
      for (i = 0; i < (int)dmx->nh_packet_data.size(); i++) {
        dmx->packetizer->process(dmx->nh_packet_data[i],
                                 dmx->nh_packet_sizes[i], 0);
        safefree(dmx->nh_packet_data[i]);
      }
      dmx->nh_packet_data.clear();
      if (dmx->last_granulepos == -1)
        dmx->packetizer->process(op.packet, op.bytes, -1);
      else {
        dmx->packetizer->process(op.packet, op.bytes, dmx->last_granulepos *
                                 1000 / dmx->vorbis_rate);
        dmx->last_granulepos = ogg_page_granulepos(og);
      }

      if (eos) {
        dmx->eos = 1;
        debug_leave("ogm_reader_c::process_page");
        return;
      }

      continue;
    }
#endif

    hdrlen = (*op.packet & PACKET_LEN_BITS01) >> 6;
    hdrlen |= (*op.packet & PACKET_LEN_BITS2) << 1;
    if ((hdrlen > 0) && (op.bytes >= (hdrlen + 1)))
      for (i = 0, lenbytes = 0; i < hdrlen; i++) {
        lenbytes = lenbytes << 8;
        lenbytes += *((unsigned char *)op.packet + hdrlen - i);
      }

    if (((*op.packet & 3) != PACKET_TYPE_HEADER) &&
        ((*op.packet & 3) != PACKET_TYPE_COMMENT)) {

      if (dmx->stype == OGM_STREAM_TYPE_VIDEO) {
        dmx->packetizer->process(&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                                 -1, -1,
                                 (*op.packet & PACKET_IS_SYNCPOINT ?
                                  VFT_IFRAME : VFT_PFRAMEAUTOMATIC));
        dmx->units_processed += (hdrlen > 0 ? lenbytes : 1);

      } else if (dmx->stype == OGM_STREAM_TYPE_TEXT) {
        dmx->units_processed++;
        if (((op.bytes - 1 - hdrlen) > 2) ||
            ((op.packet[hdrlen + 1] != ' ') &&
             (op.packet[hdrlen + 1] != 0) && !iscr(op.packet[hdrlen + 1])))
          dmx->packetizer->process(&op.packet[hdrlen + 1], op.bytes - 1 -
                                   hdrlen, ogg_page_granulepos(og), lenbytes);

      } else if (dmx->stype == OGM_STREAM_TYPE_VORBIS) {
        dmx->packetizer->process(op.packet, op.bytes);

      } else {
        dmx->packetizer->process(&op.packet[hdrlen + 1],
                                 op.bytes - 1 - hdrlen);
        dmx->units_processed += op.bytes - 1;
      }
    }

    if (eos) {
      dmx->eos = 1;
      debug_leave("ogm_reader_c::process_page");
      return;
    }
  }

  debug_leave("ogm_reader_c::process_page");
}

/*
 * Search and store additional headers for the Vorbis streams.
 */
void ogm_reader_c::process_header_page(ogg_page *og) {
  ogm_demuxer_t *dmx;
  ogg_packet     op;

  dmx = find_demuxer(ogg_page_serialno(og));
  if (dmx == NULL)
    return;

  if (dmx->stype == OGM_STREAM_TYPE_FLAC) {
    if (dmx->headers_read)
      return;
#if defined HAVE_FLAC_FORMAT_H
    ogg_stream_pagein(&dmx->os, og);
    while ((dmx->packet_data.size() < dmx->flac_header_packets) &&
           (ogg_stream_packetout(&dmx->os, &op) == 1)) {
//     while (ogg_stream_packetout(&dmx->os, &op) == 1) {
      dmx->packet_data.push_back((unsigned char *)
                                 safememdup(op.packet, op.bytes));
      dmx->packet_sizes.push_back(op.bytes);
    }
    if (dmx->packet_data.size() >= dmx->flac_header_packets)
      dmx->headers_read = true;
    while (ogg_stream_packetout(&dmx->os, &op) == 1) {
      dmx->nh_packet_data.push_back((unsigned char *)
                                    safememdup(op.packet, op.bytes));
      dmx->nh_packet_sizes.push_back(op.bytes);
    }
#else
    dmx->headers_read = true;
#endif
    return;
  }

  ogg_stream_pagein(&dmx->os, og);
  while (ogg_stream_packetout(&dmx->os, &op) == 1) {
    dmx->packet_data.push_back((unsigned char *)
                               safememdup(op.packet, op.bytes));
    dmx->packet_sizes.push_back(op.bytes);
  }

  if ((dmx->stype == OGM_STREAM_TYPE_VORBIS) && (dmx->packet_data.size() == 3))
    dmx->headers_read = true;
}

/*
 * Read all header packets and - for Vorbis streams - the comment and
 * codec data packets.
 */
int ogm_reader_c::read_headers() {
  int done, i;
  ogm_demuxer_t *dmx;
  ogg_page og;

  done = 0;
  while (!done) {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0)
      return 0;

    // Is this the first page of a new stream?
    if (ogg_page_bos(&og))
      handle_new_stream(&og);
    else {                // No, so check if it's still a header page.
      bos_pages_read = 1;
      process_header_page(&og);

      done = 1;

      for (i = 0; i < num_sdemuxers; i++) {
        dmx = sdemuxers[i];
        if (((dmx->stype == OGM_STREAM_TYPE_VORBIS) ||
             (dmx->stype == OGM_STREAM_TYPE_FLAC)) &&
            !dmx->headers_read) {
          // Not all three headers have been found for this Vorbis stream.
          done = 0;
          break;
        }
      }
    }
  }

  mm_io->setFilePointer(0, seek_beginning);
  ogg_sync_clear(&oy);
  ogg_sync_init(&oy);

  return 1;
}

/*
 * General reader. Before returning it MUST guarantee that each demuxer has
 * a page available OR that the corresponding stream is finished.
 */
int ogm_reader_c::read(generic_packetizer_c *) {
  int i;
  ogg_page og;

  do {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0)
      return 0;

    // Is this the first page of a new stream? No, so process it normally.
    if (!ogg_page_bos(&og))
      process_page(&og);
  } while (ogg_page_bos(&og));

  // Are there streams that have not finished yet?
  for (i = 0; i < num_sdemuxers; i++)
    if (!sdemuxers[i]->eos)
      return EMOREDATA;

  // No, we're done with this file.
  return 0;
}

int ogm_reader_c::display_priority() {
  int i;

  for (i = 0; i < num_sdemuxers; i++)
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO)
      return DISPLAYPRIORITY_MEDIUM;

  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void ogm_reader_c::display_progress(bool final) {
  int i;

  for (i = 0; i < num_sdemuxers; i++)
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO) {
      if (final)
        mxinfo("progress: %d frames (100%%)\r", sdemuxers[i]->units_processed);
      else
        mxinfo("progress: %d frames (%d%%)\r", sdemuxers[i]->units_processed,
               (int)(mm_io->getFilePointer() * 100 / file_size));
      return;
    }

  mxinfo("working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void ogm_reader_c::set_headers() {
  int i;

  for (i = 0; i < num_sdemuxers; i++)
    sdemuxers[i]->packetizer->set_headers();
}

void ogm_reader_c::identify() {
  int i;
  stream_header *sth;
  char fourcc[5];

  mxinfo("File '%s': container: Ogg/OGM\n", ti->fname);
  for (i = 0; i < num_sdemuxers; i++) {
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO) {
      sth = (stream_header *)&sdemuxers[i]->packet_data[0][1];
      memcpy(fourcc, sth->subtype, 4);
      fourcc[4] = 0;
    }
    mxinfo("Track ID %u: %s (%s)\n", sdemuxers[i]->serial,
           (sdemuxers[i]->stype == OGM_STREAM_TYPE_VORBIS ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_PCM ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_MP3 ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_AC3) ? "audio" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO ? "video" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_TEXT ? "subtitles" :
           "unknown",
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VORBIS ? "Vorbis" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_PCM ? "PCM" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_MP3 ? "MP3" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_AC3 ? "AC3" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO ? fourcc :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_TEXT ? "text" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_FLAC ? "FLAC" :
           "unknown");
  }
}
