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

#include "os.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#if defined(HAVE_FLAC_FORMAT_H)
#include <FLAC/stream_decoder.h>
#endif
#if defined(SYS_WINDOWS)
#include <windows.h>
#include <time.h>
#endif

extern "C" {                    // for BITMAPINFOHEADER
#include "avilib.h"
}

#include "aac_common.h"
#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "iso639.h"
#include "matroska.h"
#include "mkvmerge.h"
#include "ogmstreams.h"
#include "pr_generic.h"
#include "p_aac.h"
#include "p_ac3.h"
#if defined(HAVE_FLAC_FORMAT_H)
#include "p_flac.h"
#endif
#include "p_mp3.h"
#include "p_pcm.h"
#include "p_textsubs.h"
#include "p_video.h"
#include "p_vorbis.h"
#include "r_ogm.h"

#define BUFFER_SIZE 4096

static void
free_string_array(char **comments) {
  int i;

  if (comments == NULL)
    return;
  for (i = 0; comments[i] != NULL; i++)
    safefree(comments[i]);
  safefree(comments);
}

static char **
extract_vorbis_comments(unsigned char *buffer,
                        int size) {
  mm_mem_io_c in(buffer, size);
  char **comments;
  uint32_t i, n, len;

  comments = NULL;
  try {
    in.skip(7);                 // 0x03 "vorbis"
    n = in.read_uint32();       // vendor_length
    in.skip(n);                 // vendor_string
    n = in.read_uint32();       // user_comment_list_length
    comments = (char **)safemalloc((n + 1) * sizeof(char *));
    memset(comments, 0, (n + 1) * sizeof(char *));
    for (i = 0; i < n; i++) {
      len = in.read_uint32();
      comments[i] = (char *)safemalloc(len + 1);
      if (in.read(comments[i], len) != len)
        throw false;
      comments[i][len] = 0;
    }
  } catch(...) {
    free_string_array(comments);
    return NULL;
  }

  return comments;
}

#if defined(HAVE_FLAC_FORMAT_H)
#define FPFX "flac_header_extraction: "

static FLAC__StreamDecoderReadStatus
fhe_read_cb(const FLAC__StreamDecoder *decoder,
            FLAC__byte buffer[],
            unsigned *bytes,
            void *client_data) {
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
    mxerror(FPFX "bytes (%u) < op.bytes (%ld). Could not read the FLAC "
            "headers.\n", *bytes, op.bytes);
  memcpy(buffer, op.packet, op.bytes);
  *bytes = op.bytes;
  fhe->num_packets++;
  mxverb(2, FPFX "read packet number %lld with %ld bytes\n", fhe->num_packets,
         op.bytes);

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

static void
fhe_error_cb(const FLAC__StreamDecoder *,
             FLAC__StreamDecoderErrorStatus status,
             void *client_data) {
  ((flac_header_extractor_c *)client_data)->done = true;
  mxverb(2, FPFX "error (%d)\n", (int)status);
}

flac_header_extractor_c::flac_header_extractor_c(const char *file_name,
                                                 int64_t nsid):
  metadata_parsed(false),
  sid(nsid),
  num_packets(0),
  num_header_packets(0),
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

bool
flac_header_extractor_c::extract() {
  int result;

  mxverb(2, FPFX "extract\n");
  if (!read_page()) {
    mxverb(2, FPFX "read_page() failed.\n");
    return false;
  }
  result = (int)FLAC__stream_decoder_process_until_end_of_stream(decoder);
  mxverb(2, FPFX "extract, result: %d, mdp: %d, num_header_packets: %lld\n",
         result, metadata_parsed, num_header_packets);

  return metadata_parsed;
}

bool
flac_header_extractor_c::read_page() {
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
int
ogm_reader_c::probe_file(mm_io_c *mm_io,
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
  if (strncmp((char *)data, "OggS", 4))
    return 0;
  return 1;
}

/*
 * Opens the file for processing, initializes an ogg_sync_state used for
 * reading from an OGG stream.
 */
ogm_reader_c::ogm_reader_c(track_info_c *nti)
  throw (error_c):
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

  if (verbose)
    mxinfo(FMT_FN "Using the OGG/OGM demultiplexer.\n", ti->fname);

  if (read_headers() <= 0)
    throw error_c("ogm_reader: Could not read all header packets.");
  handle_stream_comments();
}

ogm_reader_c::~ogm_reader_c() {
  int i;
  ogm_demuxer_t *dmx;

  ogg_sync_clear(&oy);

  for (i = 0; i < sdemuxers.size(); i++) {
    dmx = sdemuxers[i];
    ogg_stream_clear(&dmx->os);
    delete dmx;
  }
  delete mm_io;
}

ogm_demuxer_t *
ogm_reader_c::find_demuxer(int serialno) {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    if (sdemuxers[i]->serialno == serialno) {
      if (sdemuxers[i]->in_use)
        return sdemuxers[i];
      return NULL;
    }

  return NULL;
}

/*
 * Reads an OGG page from the stream. Returns 0 if there are no more pages
 * left, EMOREDATA otherwise.
 */
int
ogm_reader_c::read_page(ogg_page *og) {
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

void
ogm_reader_c::create_packetizer(int64_t tid) {
  vorbis_info vi;
  vorbis_comment vc;
  ogg_packet op;
  alBITMAPINFOHEADER bih;
  stream_header *sth;
  int i, profile, channels, sample_rate, output_sample_rate;
  bool sbr, aac_info_extracted;
  ogm_demuxer_t *dmx;
  generic_packetizer_c *ptzr;

  if ((tid < 0) || (tid >= sdemuxers.size()))
    return;
  dmx = sdemuxers[tid];

  if ((dmx->ptzr == -1) && dmx->in_use) {
    memset(&bih, 0, sizeof(alBITMAPINFOHEADER));
    sth = (stream_header *)&dmx->packet_data[0][1];
    ti->private_data = NULL;
    ti->private_size = 0;
    ti->id = tid;
    ti->language = dmx->language;
    ti->track_name = dmx->title;

    ptzr = NULL;
    switch (dmx->stype) {
      case OGM_STREAM_TYPE_VIDEO:
        // AVI compatibility mode. Fill in the values we've got and guess
        // the others.
        put_uint32(&bih.bi_size, sizeof(alBITMAPINFOHEADER));
        put_uint32(&bih.bi_width, get_uint32(&sth->sh.video.width));
        put_uint32(&bih.bi_height, get_uint32(&sth->sh.video.height));
        put_uint16(&bih.bi_planes, 1);
        put_uint16(&bih.bi_bit_count, 24);
        memcpy(&bih.bi_compression, sth->subtype, 4);
        put_uint32(&bih.bi_size_image, get_uint32(&bih.bi_width) *
                   get_uint32(&bih.bi_height) * 3);
        ti->private_data = (unsigned char *)&bih;
        ti->private_size = sizeof(alBITMAPINFOHEADER);

        ptzr = new video_packetizer_c(this, NULL, (double)10000000.0 /
                                      get_uint64(&sth->time_unit),
                                      get_uint32(&sth->sh.video.width),
                                      get_uint32(&sth->sh.video.height),
                                      false, ti);

        mxinfo(FMT_TID "Using the video output module.\n", ti->fname,
               (int64_t)tid);
        ti->private_data = NULL;

        break;

      case OGM_STREAM_TYPE_PCM:
        ptzr = new pcm_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                    get_uint16(&sth->sh.audio.channels),
                                    get_uint16(&sth->bits_per_sample), ti);

        mxinfo(FMT_TID "Using the PCM output module.\n", ti->fname,
               (int64_t)tid);
        break;

      case OGM_STREAM_TYPE_MP3:
        ptzr = new mp3_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                    get_uint16(&sth->sh.audio.channels),
                                    true, ti);
        mxinfo(FMT_TID "Using the MP3 output module.\n", ti->fname,
               (int64_t)tid);
        break;

      case OGM_STREAM_TYPE_AC3:
        ptzr = new ac3_packetizer_c(this, get_uint64(&sth->samples_per_unit),
                                    get_uint16(&sth->sh.audio.channels), 0,
                                    ti);
        mxinfo(FMT_TID "Using the AC3 output module.\n", ti->fname,
               (int64_t)tid);

        break;

      case OGM_STREAM_TYPE_AAC:
        aac_info_extracted = false;
        if (dmx->packet_sizes[0] >= (sizeof(stream_header) + 5)) {
          if (parse_aac_data(&dmx->packet_data[0][sizeof(stream_header) + 5],
                             dmx->packet_sizes[0] - sizeof(stream_header) - 5,
                             profile, channels, sample_rate,
                             output_sample_rate, sbr)) {
            aac_info_extracted = true;
            if (sbr)
              profile = AAC_PROFILE_SBR;
          }
        }
        if (!aac_info_extracted) {
          sbr = false;
          channels = get_uint16(&sth->sh.audio.channels);
          sample_rate = get_uint64(&sth->samples_per_unit);
          profile = AAC_PROFILE_LC;
        }
        mxverb(2, "ogm_reader: %lld/%s: profile %d, channels %d, sample_rate "
               "%d, sbr %d, output_sample_rate %d, ex %d\n", ti->id, ti->fname,
               profile, channels, sample_rate, (int)sbr, output_sample_rate,
               (int)aac_info_extracted);
        ptzr = new aac_packetizer_c(this, AAC_ID_MPEG4, profile, sample_rate,
                                    channels, ti, false, true);
        if (sbr)
          ptzr->set_audio_output_sampling_freq(output_sample_rate);

        mxinfo(FMT_TID "Using the AAC output module.\n", ti->fname,
               (int64_t)tid);

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
        ptzr =
          new vorbis_packetizer_c(this,
                                  dmx->packet_data[0], dmx->packet_sizes[0],
                                  dmx->packet_data[1], dmx->packet_sizes[1],
                                  dmx->packet_data[2], dmx->packet_sizes[2],
                                  ti);

        mxinfo(FMT_TID "Using the Vorbis output module.\n", ti->fname,
               (int64_t)tid);

        break;

      case OGM_STREAM_TYPE_TEXT:
        ptzr = new textsubs_packetizer_c(this, MKV_S_TEXTUTF8, NULL, 0, true,
                                         false, ti);
        mxinfo(FMT_TID "Using the text subtitle output module.\n", ti->fname,
               (int64_t)tid);

        break;

#if defined(HAVE_FLAC_FORMAT_H)
      case OGM_STREAM_TYPE_FLAC:
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
        ptzr = new flac_packetizer_c(this, buf, size, ti);
        safefree(buf);

        mxinfo(FMT_TID "Using the FLAC output module.\n", ti->fname,
               (int64_t)tid);

        break;
#endif
      default:
        die("ogm_reader: Don't know how to create a packetizer for stream "
            "type %d.\n", (int)dmx->stype);
        break;

    }
    if (ptzr != NULL)
      dmx->ptzr = add_packetizer(ptzr);
    ti->language = NULL;
    ti->track_name = NULL;
  }
}

void
ogm_reader_c::create_packetizers() {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    create_packetizer(i);
}

/*
 * Checks every demuxer if it has a page available.
 */
int
ogm_reader_c::packet_available() {
  int i;

  if (sdemuxers.size() == 0)
    return 0;

  for (i = 0; i < sdemuxers.size(); i++)
    if (!PTZR(sdemuxers[i]->ptzr)->packet_available())
      return 0;

  return 1;
}

void
ogm_reader_c::handle_new_stream_and_packets(ogg_page *og) {
  ogm_demuxer_t *dmx;

  handle_new_stream(og);
  dmx = find_demuxer(ogg_page_serialno(og));
  if (dmx != NULL)
    process_header_packets(dmx);
}

/*
 * The page is the beginning of a new stream. Check the contents for known
 * stream headers. If it is a known stream and the user has requested that
 * it should be extracted then allocate a new packetizer based on the
 * stream type and store the needed data in a new ogm_demuxer_t.
 */
void
ogm_reader_c::handle_new_stream(ogg_page *og) {
  ogg_packet op;
  ogm_demuxer_t *dmx;
  stream_header *sth;
  char buf[5];
  uint32_t codec_id;

  dmx = new ogm_demuxer_t;
  sdemuxers.push_back(dmx);
  if (ogg_stream_init(&dmx->os, ogg_page_serialno(og))) {
    mxwarn("ogm_reader: ogg_stream_init for stream number "
           "%d failed. Will try to continue and ignore this stream.",
           sdemuxers.size());
    return;
  }

  // Read the first page and get its first packet.
  ogg_stream_pagein(&dmx->os, og);
  ogg_stream_packetout(&dmx->os, &op);

  dmx->packet_data.push_back((unsigned char *)safememdup(op.packet, op.bytes));
  dmx->packet_sizes.push_back(op.bytes);
  dmx->serialno = ogg_page_serialno(og);

  /*
   * Check the contents for known stream headers. This one is the
   * standard Vorbis header.
   */
  if ((op.bytes >= 7) && !strncmp((char *)&op.packet[1], "vorbis", 6)) {
    if (!demuxing_requested('a', sdemuxers.size() - 1))
      return;

    dmx->stype = OGM_STREAM_TYPE_VORBIS;
    dmx->in_use = true;

    return;
  }

  // FLAC
  if ((op.bytes >= 4) && !strncmp((char *)op.packet, "fLaC", 4)) {
    if (!demuxing_requested('a', sdemuxers.size() - 1))
      return;

    dmx->stype = OGM_STREAM_TYPE_FLAC;
    dmx->in_use = true;

#if !defined(HAVE_FLAC_FORMAT_H)
    mxerror("mkvmerge has not been compiled with FLAC support but handling "
            "of this stream has been requested.\n");
#else
    dmx->fhe = new flac_header_extractor_c(ti->fname, dmx->serialno);
    if (!dmx->fhe->extract())
      mxerror("ogm_reader: Could not read the FLAC header packets for "
              "stream id %d.\n", sdemuxers.size() - 1);
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
      if (!demuxing_requested('v', sdemuxers.size() - 1))
        return;

      dmx->stype = OGM_STREAM_TYPE_VIDEO;
      dmx->native_mode = false;
      if (video_fps < 0)
        video_fps = 10000000.0 / (float)get_uint64(&sth->time_unit);
      dmx->in_use = true;

      return;
    }

    if (!strncmp(sth->streamtype, "audio", 5)) {
      if (!demuxing_requested('a', sdemuxers.size() - 1))
        return;

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
      else if (codec_id == 0x00ff)
        dmx->stype = OGM_STREAM_TYPE_AAC;
      else {
        mxwarn("ogm_reader: Unknown audio stream type %u. "
               "Ignoring stream id %d.\n", codec_id, sdemuxers.size() - 1);
        return;
      }

      dmx->native_mode = false;
      dmx->in_use = true;

      return;
    }

    if (!strncmp(sth->streamtype, "text", 4)) {
      if (!demuxing_requested('s', sdemuxers.size() - 1))
        return;

      dmx->stype = OGM_STREAM_TYPE_TEXT;
      dmx->native_mode = false;
      dmx->in_use = true;

      return;
    }
  }

  /*
   * The old OggDS headers (see MPlayer's source, libmpdemux/demux_ogg.c)
   * are not supported.
   */
}

/*
 * Process the contents of a page. First find the demuxer associated with
 * the page's serial number. If there is no such demuxer then either the
 * OGG file is damaged (very rare) or the page simply belongs to a stream
 * that the user didn't want extracted.
 * If the demuxer is found then hand over all packets in this page to the
 * associated packetizer.
 */
void
ogm_reader_c::process_page(ogg_page *og) {
  ogm_demuxer_t *dmx;
  ogg_packet op;
  int duration_len, eos, i;
  long duration;

  duration = 0;
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
        memory_c mem(dmx->nh_packet_data[i], dmx->nh_packet_sizes[i], true);
        PTZR(dmx->ptzr)->process(mem, 0);
      }
      dmx->nh_packet_data.clear();
      if (dmx->last_granulepos == -1) {
        memory_c mem(op.packet, op.bytes, false);
        PTZR(dmx->ptzr)->process(mem, -1);
      } else {
        memory_c mem(op.packet, op.bytes, false);
        PTZR(dmx->ptzr)->process(mem, dmx->last_granulepos * 1000000000 /
                                 dmx->vorbis_rate);
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

    duration_len = (*op.packet & PACKET_LEN_BITS01) >> 6;
    duration_len |= (*op.packet & PACKET_LEN_BITS2) << 1;
    if ((duration_len > 0) && (op.bytes >= (duration_len + 1)))
      for (i = 0, duration = 0; i < duration_len; i++) {
        duration = duration << 8;
        duration += *((unsigned char *)op.packet + duration_len - i);
      }

    if (((*op.packet & 3) != PACKET_TYPE_HEADER) &&
        ((*op.packet & 3) != PACKET_TYPE_COMMENT)) {

      if (dmx->stype == OGM_STREAM_TYPE_VIDEO) {
        memory_c mem(&op.packet[duration_len + 1], op.bytes - 1 -
                     duration_len, false);
        PTZR(dmx->ptzr)->process(mem, -1, -1,
                                 (*op.packet & PACKET_IS_SYNCPOINT ?
                                  VFT_IFRAME : VFT_PFRAMEAUTOMATIC));
        dmx->units_processed += (duration_len > 0 ? duration : 1);

      } else if (dmx->stype == OGM_STREAM_TYPE_TEXT) {
        dmx->units_processed++;
        if (((op.bytes - 1 - duration_len) > 2) ||
            ((op.packet[duration_len + 1] != ' ') &&
             (op.packet[duration_len + 1] != 0) &&
             !iscr(op.packet[duration_len + 1]))) {
          memory_c mem(&op.packet[duration_len + 1], op.bytes - 1 -
                       duration_len, false);
          PTZR(dmx->ptzr)->process(mem, ogg_page_granulepos(og) * 1000000,
                                   (int64_t)duration * 1000000);
        }

      } else if (dmx->stype == OGM_STREAM_TYPE_VORBIS) {
        memory_c mem(op.packet, op.bytes, false);
        PTZR(dmx->ptzr)->process(mem);

      } else {
        memory_c mem(&op.packet[duration_len + 1], op.bytes - 1 -
                     duration_len, false);
        PTZR(dmx->ptzr)->process(mem);
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

void
ogm_reader_c::process_header_page(ogg_page *og) {
  ogm_demuxer_t *dmx;

  dmx = find_demuxer(ogg_page_serialno(og));
  if (dmx == NULL)
    return;
  if (dmx->headers_read)
    return;
  ogg_stream_pagein(&dmx->os, og);
  process_header_packets(dmx);
}

/*
 * Search and store additional headers for the Ogg streams.
 */
void
ogm_reader_c::process_header_packets(ogm_demuxer_t *dmx) {
  ogg_packet op;

  if (dmx->headers_read)
    return;

  if (dmx->stype == OGM_STREAM_TYPE_FLAC) {
#if defined HAVE_FLAC_FORMAT_H
    while ((dmx->packet_data.size() < dmx->flac_header_packets) &&
           (ogg_stream_packetout(&dmx->os, &op) == 1)) {
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

  while (ogg_stream_packetout(&dmx->os, &op) == 1) {
    if (((dmx->stype != OGM_STREAM_TYPE_VORBIS) || !dmx->native_mode) &&
        ((op.packet[0] & 1) != 1)) {
      mxwarn("ogm_reader: Missing header/comment packets for stream %d in "
             "'%s'. This file is broken but should be muxed correctly. If "
             "not please contact the author Moritz Bunkus "
             "<moritz@bunkus.org>.\n", dmx->serialno, ti->fname);
      dmx->headers_read = true;
      ogg_stream_reset(&dmx->os);
      return;
    }
    dmx->packet_data.push_back((unsigned char *)
                               safememdup(op.packet, op.bytes));
    dmx->packet_sizes.push_back(op.bytes);
  }

  if (dmx->stype == OGM_STREAM_TYPE_VORBIS) {
    if (dmx->packet_data.size() == 3)
      dmx->headers_read = true;
  } else if (dmx->packet_data.size() == 2)
    dmx->headers_read = true;
}

/*
 * Read all header packets and - for Vorbis streams - the comment and
 * codec data packets.
 */
int
ogm_reader_c::read_headers() {
  int i;
  bool done;
  ogm_demuxer_t *dmx;
  ogg_page og;

  done = false;
  while (!done) {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0)
      return 0;

    // Is this the first page of a new stream?
    if (ogg_page_bos(&og))
      handle_new_stream_and_packets(&og);
    else {                // No, so check if it's still a header page.
      bos_pages_read = 1;
      process_header_page(&og);

      done = true;

      for (i = 0; i < sdemuxers.size(); i++) {
        dmx = sdemuxers[i];
        if (!dmx->headers_read && dmx->in_use) {
          done = false;
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
 * General reader. Read a page and hand it over for processing.
 */
int
ogm_reader_c::read(generic_packetizer_c *,
                   bool) {
  int i;
  ogg_page og;

  do {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0) {
      flush_packetizers();
      return 0;
    }

    // Is this the first page of a new stream? No, so process it normally.
    if (!ogg_page_bos(&og))
      process_page(&og);
  } while (ogg_page_bos(&og));

  // Are there streams that have not finished yet?
  for (i = 0; i < sdemuxers.size(); i++)
    if (!sdemuxers[i]->eos && sdemuxers[i]->in_use)
      return EMOREDATA;

  // No, we're done with this file.
  flush_packetizers();
  return 0;
}

int
ogm_reader_c::display_priority() {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    if (sdemuxers[i]->in_use && (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO))
      return DISPLAYPRIORITY_MEDIUM;

  return DISPLAYPRIORITY_LOW;
}

void
ogm_reader_c::display_progress(bool final) {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    if (sdemuxers[i]->in_use &&
        (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO)) {
      if (final)
        mxinfo("progress: %d frames (100%%)\r", sdemuxers[i]->units_processed);
      else
        mxinfo("progress: %d frames (%d%%)\r", sdemuxers[i]->units_processed,
               (int)(mm_io->getFilePointer() * 100 / file_size));
      return;
    }

  if (final)
    mxinfo("progress: 100%%\r");
  else
    mxinfo("progress: %d%%\r",
           (int)(mm_io->getFilePointer() * 100 / file_size));
}

void
ogm_reader_c::identify() {
  int i;
  stream_header *sth;
  char fourcc[5];
  string info;

  // Check if a video track has a TITLE comment. If yes we use this as the
  // new segment title / global file title.
  if (identify_verbose)
    for (i = 0; i < sdemuxers.size(); i++)
      if ((sdemuxers[i]->title != NULL) &&
          (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO)) {
        info = string(" [title:") + escape(sdemuxers[i]->title) + string("]");
        break;
      }

  mxinfo("File '%s': container: Ogg/OGM%s\n", ti->fname, info.c_str());
  for (i = 0; i < sdemuxers.size(); i++) {
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO) {
      sth = (stream_header *)&sdemuxers[i]->packet_data[0][1];
      memcpy(fourcc, sth->subtype, 4);
      fourcc[4] = 0;
    }
    if (identify_verbose) {
      info = " [";
      if (sdemuxers[i]->language != NULL)
        info += string("language:") + escape(sdemuxers[i]->language) +
          string(" ");
      if ((sdemuxers[i]->title != NULL) &&
          (sdemuxers[i]->stype != OGM_STREAM_TYPE_VIDEO))
        info += string("track_name:") + escape(sdemuxers[i]->title) +
          string(" ");
      info += "]";
    } else
      info = "";
    mxinfo("Track ID %d: %s (%s)%s\n", i,
           (sdemuxers[i]->stype == OGM_STREAM_TYPE_VORBIS ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_PCM ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_MP3 ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_AC3 ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_AAC ||
            sdemuxers[i]->stype == OGM_STREAM_TYPE_FLAC) ? "audio" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO ? "video" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_TEXT ? "subtitles" :
           "unknown",
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VORBIS ? "Vorbis" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_PCM ? "PCM" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_MP3 ? "MP3" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_AC3 ? "AC3" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_AAC ? "AAC" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO ? fourcc :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_TEXT ? "text" :
           sdemuxers[i]->stype == OGM_STREAM_TYPE_FLAC ? "FLAC" :
           "unknown",
           info.c_str());
  }
}

void
ogm_reader_c::flush_packetizers() {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    if (sdemuxers[i]->ptzr != -1)
      PTZR(sdemuxers[i]->ptzr)->flush();
}

void
ogm_reader_c::handle_stream_comments() {
  int i, j;
  ogm_demuxer_t *dmx;
  char **comments, name[100];
  const char *iso639_2;
  vector<string> comment;
  vector<char *> chapters;
  mm_io_c *out;
  bool comments_in_utf8;

  for (i = 0; i < sdemuxers.size(); i++) {
    dmx = sdemuxers[i];
    if ((dmx->stype == OGM_STREAM_TYPE_FLAC) ||
        (dmx->packet_data.size() < 2))
      continue;
    comments = extract_vorbis_comments(dmx->packet_data[1],
                                       dmx->packet_sizes[1]);
    if (comments == NULL)
      continue;
    chapters.clear();

    comments_in_utf8 = true;
    for (j = 0; comments[j] != NULL; j++)
      if (!is_valid_utf8_string(comments[j])) {
        comments_in_utf8 = false;
        break;
      }

    for (j = 0; comments[j] != NULL; j++) {
      mxverb(2, "ogm_reader: commment for #%d for %d: %s\n", j, i,
             comments[j]);
      comment = split(comments[j], "=", 2);
      if (comment.size() != 2)
        continue;

      if (!comments_in_utf8)
        comment[1] = to_utf8(cc_local_utf8, comment[1]);

      if (comment[0] == "LANGUAGE") {
        iso639_2 = map_english_name_to_iso639_2(comment[1].c_str());
        if (iso639_2 == NULL)
          iso639_2 = map_iso639_1_to_iso639_2(comment[1].c_str());
        if ((iso639_2 == NULL) && (is_valid_iso639_2_code(comment[1].c_str())))
          iso639_2 = comment[1].c_str();
        if (iso639_2 == NULL) {
          string lang;
          int pos1, pos2;

          lang = comment[1];
          pos1 = lang.find("[");
          while (pos1 >= 0) {
            pos2 = lang.find("]", pos1);
            if (pos2 == -1)
              pos2 = lang.length() - 1;
            lang.erase(pos1, pos2 - pos1 + 1);
            pos1 = lang.find("[");
          }
          pos1 = lang.find("(");
          while (pos1 >= 0) {
            pos2 = lang.find(")", pos1);
            if (pos2 == -1)
              pos2 = lang.length() - 1;
            lang.erase(pos1, pos2 - pos1 + 1);
            pos1 = lang.find("(");
          }
          for (pos1 = 0; iso639_languages[pos1].iso639_2_code != NULL; pos1++)
            if (starts_with_case(lang, iso639_languages[pos1].english_name)) {
              iso639_2 = iso639_languages[pos1].iso639_2_code;
              break;
            }
        }
        if (iso639_2 != NULL) {
          safefree(dmx->language);
          dmx->language = safestrdup(iso639_2);
        }

      } else if (comment[0] == "TITLE") {
        if (!segment_title_set && (segment_title.length() == 0) &&
            (dmx->stype == OGM_STREAM_TYPE_VIDEO))
          segment_title = comment[1].c_str();
        safefree(dmx->title);
        dmx->title = safestrdup(comment[1].c_str());

      } else if (starts_with(comment[0], "CHAPTER")) {
        if (comments_in_utf8)
          chapters.push_back(safestrdup(comments[j]));
        else
          chapters.push_back(to_utf8_c(cc_local_utf8, comments[j]));
      }
    }
    if ((chapters.size() > 0) && !ti->no_chapters && (kax_chapters == NULL)) {
#if defined(SYS_WINDOWS)
      sprintf(name, "mkvmerge-ogm-reader-chapters-%d-%d",
              (int)GetCurrentProcessId(), (int)time(NULL));
#else
      sprintf(name, "mkvmerge-ogm-reader-chapters-%d-%d", getpid(),
              (int)time(NULL));
#endif
      try {
        out = new mm_io_c(name, MODE_WRITE);
        out->write_bom("UTF-8");
        for (j = 0; j < chapters.size(); j++) {
          out->puts_unl(chapters[j]);
          out->puts_unl("\n");
        }
        delete out;
        kax_chapters = parse_chapters(name);
      } catch (...) {
      }
      unlink(name);
    }
    for (j = 0; j < chapters.size(); j++)
      safefree(chapters[j]);
    free_string_array(comments);
  }
}

void
ogm_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < sdemuxers.size(); i++)
    available_track_ids.push_back(i);
}

