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
    \version \$Id: r_ogm.cpp,v 1.16 2003/04/20 17:22:04 mosu Exp $
    \brief OGG media stream reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include "config.h"

#ifdef HAVE_OGGVORBIS

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

extern "C" {                    // for BITMAPINFOHEADER
#include "avilib.h"
}

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "ogmstreams.h"
#include "queue.h"
#include "r_ogm.h"
#include "p_vorbis.h"
#include "p_video.h"
#include "p_pcm.h"
//#include "p_textsubs.h"
#include "p_mp3.h"
#include "p_ac3.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*
 * Probes a file by simply comparing the first four bytes to 'OggS'.
 */
int ogm_reader_c::probe_file(FILE *file, int64_t size) {
  unsigned char data[4];
  
  if (size < 4)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(data, 1, 4, file) != 4) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
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
  int64_t size;
  
  if ((file = fopen(ti->fname, "r")) == NULL)
    throw error_c("ogm_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("ogm_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("ogm_reader: Could not seek to beginning of file.");
  if (!ogm_reader_c::probe_file(file, size))
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
    fprintf(stdout, "Using OGG/OGM demultiplexer for %s.\n", ti->fname);

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
    free(dmx);
  }
  if (sdemuxers != NULL)
    free(sdemuxers);
  ti->private_data = NULL;
}

/*
 * Checks whether the user wants a certain stream extracted or not.
 */
int ogm_reader_c::demuxing_requested(unsigned char *streams, int serialno) {
  int i;
  
  if (streams == NULL)
    return 1;

  for (i = 0; i < strlen((char *)streams); i++)
    if (streams[i] == serialno)
      return 1;

  return 0;
}

ogm_demuxer_t *ogm_reader_c::find_demuxer(int serialno) {
  int i;

  for (i = 0; i < num_sdemuxers; i++)
    if (sdemuxers[i]->serial == serialno)
      return sdemuxers[i];

  return NULL;
}

#define BUFFER_SIZE 4096

void ogm_reader_c::free_demuxer(int idx) {
  int i;
  ogm_demuxer_t *dmx;

  if (idx >= num_sdemuxers)
    return;
  dmx = sdemuxers[idx];
  for (i = 0; i < 3; i++)
    if (dmx->packet_data[i] != NULL)
      free(dmx->packet_data[i]);
  free(dmx);

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
    // np < 0 is the error case. Should not happen with local OGG files.
    if (np < 0) {
      fprintf(stderr, "Fatal: ogm_reader: ogg_sync_pageseek failed\n");
      exit(1);
    }

    // np == 0 means that there is not enough data for a complete page.
    if (np == 0) {
      buf = (unsigned char *)ogg_sync_buffer(&oy, BUFFER_SIZE);
      if (!buf) {
        fprintf(stderr, "Fatal: ogm_reader: ogg_sync_buffer failed\n");
        exit(1);
      }

      if ((nread = fread(buf, 1, BUFFER_SIZE, file)) <= 0)
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
  sdemuxers = (ogm_demuxer_t **)realloc(sdemuxers, sizeof(ogm_demuxer_t *) *
                                        (num_sdemuxers + 1));
  if (sdemuxers == NULL)
    die ("realloc");
  sdemuxers[num_sdemuxers] = dmx;
  num_sdemuxers++;
}

void ogm_reader_c::create_packetizers() {
  vorbis_info vi;
  vorbis_comment vc;
  ogg_packet op;
  BITMAPINFOHEADER bih;
  stream_header *sth;
  int i;
  ogm_demuxer_t *dmx;

  memset(&bih, 0, sizeof(BITMAPINFOHEADER));
  i = 0;
  while (i < num_sdemuxers) {
    dmx = sdemuxers[i];
    sth = (stream_header *)&dmx->packet_data[0][1];
    ti->private_data = NULL;
    ti->private_size = 0;

    switch (dmx->stype) {
      case OGM_STREAM_TYPE_VIDEO:
        if (ti->fourcc[0] == 0) {
          memcpy(ti->fourcc, sth->subtype, 4);
          ti->fourcc[4] = 0;
        }
        // AVI compatibility mode. Fill in the values we've got and guess
        // the others.
        bih.bi_size = sizeof(BITMAPINFOHEADER);
        bih.bi_width = sth->sh.video.width;
        bih.bi_height = sth->sh.video.height;
        bih.bi_planes = 1;
        bih.bi_bit_count = 24;
        memcpy(&bih.bi_compression, ti->fourcc, 4);
        bih.bi_size_image = sth->sh.video.width * sth->sh.video.height * 3;
        ti->private_data = (unsigned char *)&bih;
        ti->private_size = sizeof(BITMAPINFOHEADER);

        try {
          dmx->packetizer =
            new video_packetizer_c((double)10000000 / (double)sth->time_unit, 
                                   sth->sh.video.width, sth->sh.video.height,
                                   sth->bits_per_sample, 1, ti);
        } catch (error_c &error) {
          fprintf(stderr, "Error: ogm_reader: could not initialize video "
                  "packetizer for stream id %d. Will try to continue and "
                  "ignore this stream.\n", dmx->serial);
          free(dmx);
          continue;
        }

        if (verbose)
          fprintf(stdout, "OGG/OGM demultiplexer (%s): using video output "
                    "module for stream %d.\n", ti->fname, dmx->serial);

        break;

      case OGM_STREAM_TYPE_PCM:
        try {
          dmx->packetizer =
            new pcm_packetizer_c(sth->samples_per_unit,
                                 sth->sh.audio.channels, 
                                 sth->bits_per_sample, ti);
        } catch (error_c &error) {
          fprintf(stderr, "Error: ogm_reader: could not initialize PCM "
                  "packetizer for stream id %d. Will try to continue and "
                  "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          fprintf(stdout, "OGG/OGM demultiplexer (%s): using PCM output "
                    "module for stream %d.\n", ti->fname, dmx->serial);
        break;

      case OGM_STREAM_TYPE_MP3:
        try {
          dmx->packetizer = 
            new mp3_packetizer_c(sth->samples_per_unit,
                                 sth->sh.audio.channels, ti);
        } catch (error_c &error) {
          fprintf(stderr, "Error: ogm_reader: could not initialize MP3 "
                  "packetizer for stream id %d. Will try to continue and "
                  "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          fprintf(stdout, "OGG/OGM demultiplexer (%s): using MP3 output "
                    "module for stream %d.\n", ti->fname, dmx->serial);
        break;

      case OGM_STREAM_TYPE_AC3:
        try {
          dmx->packetizer = 
            new ac3_packetizer_c(sth->samples_per_unit,
                                 sth->sh.audio.channels, ti);
        } catch (error_c &error) {
          fprintf(stderr, "Error: ogm_reader: could not initialize AC3 "
                  "packetizer for stream id %d. Will try to continue and "
                  "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          fprintf(stdout, "OGG/OGM demultiplexer (%s): using AC3 output "
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
            new vorbis_packetizer_c(dmx->packet_data[0], dmx->packet_sizes[0],
                                    dmx->packet_data[1], dmx->packet_sizes[1],
                                    dmx->packet_data[2], dmx->packet_sizes[2],
                                    ti);
        } catch (error_c &error) {
          fprintf(stderr, "Error: ogm_reader: could not initialize Vorbis "
                  "packetizer for stream id %d. Will try to continue and "
                  "ignore this stream.\n", dmx->serial);
          free_demuxer(i);
          continue;
        }

        if (verbose)
          fprintf(stdout, "OGG/OGM demultiplexer (%s): using Vorbis output "
                    "module for stream %d.\n", ti->fname, dmx->serial);

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
  ogg_stream_state  new_oss;
  ogg_packet        op;
  ogm_demuxer_t    *dmx;
  stream_header    *sth;
  char              buf[5];
  u_int32_t         codec_id;

  if (ogg_stream_init(&new_oss, ogg_page_serialno(og))) {
    fprintf(stderr, "Error: ogm_reader: ogg_stream_init for stream number "
            "%d failed. Will try to continue and ignore this stream.",
            numstreams + 1);
    return;
  }

  // Read the first page and get its first packet.
  ogg_stream_pagein(&new_oss, og);
  ogg_stream_packetout(&new_oss, &op);
  
  dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
  if (dmx == NULL)
    die("malloc");
  memset(dmx, 0, sizeof(ogm_demuxer_t));
  dmx->num_packets = 1;
  dmx->packet_data[0] = (unsigned char *)malloc(op.bytes);
  if (dmx->packet_data[0] == NULL)
    die("malloc");
  memcpy(dmx->packet_data[0], op.packet, op.bytes);
  dmx->packet_sizes[0] = op.bytes;

  /*
   * Check the contents for known stream headers. This one is the
   * standard Vorbis header.
   */
  if ((op.bytes >= 7) && !strncmp((char *)&op.packet[1], "vorbis", 6)) {
    nastreams++;
    numstreams++;
    if (!demuxing_requested(ti->atracks, ogg_page_serialno(og))) {
      ogg_stream_clear(&new_oss);
      free(dmx->packet_data[0]);
      free(dmx);
      return;
    }
      
    dmx->stype = OGM_STREAM_TYPE_VORBIS;
    dmx->serial = ogg_page_serialno(og);
    memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
    dmx->sid = nastreams;
    add_new_demuxer(dmx);
      
    return;
  }

  // The new stream headers introduced by OggDS (see ogmstreams.h).
  if (((*op.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
      (op.bytes >= ((int)sizeof(stream_header) + 1 - sizeof(int)))) {
    sth = (stream_header *)(op.packet + 1);
    if (!strncmp(sth->streamtype, "video", 5)) {
      nvstreams++;
      numstreams++;
      if (!demuxing_requested(ti->vtracks, ogg_page_serialno(og))) {
        ogg_stream_clear(&new_oss);
        free(dmx->packet_data[0]);
        free(dmx);
        return;
      }
      
      dmx->stype = OGM_STREAM_TYPE_VIDEO;
      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = nvstreams;
      add_new_demuxer(dmx);
      if (video_fps < 0)
        video_fps = 10000000.0 / (float)sth->time_unit;
      
      return;
    }
    
    if (!strncmp(sth->streamtype, "audio", 5)) {
      nastreams++;
      numstreams++;
      if (!demuxing_requested(ti->atracks, ogg_page_serialno(og))) {
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
        fprintf(stderr, "Error: ogm_reader: Unknown audio stream type %u. "
                "Ignoring stream id %d.\n", codec_id, numstreams);
        free(dmx->packet_data[0]);
        free(dmx);
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
      fprintf(stderr, "Warning: ogm_reader: No support for subtitles at "
              "the moment. Ignoring stream id %d.\n", numstreams);

//       if (!demuxing_requested(ti->stracks, ntstreams)) {
//         ogg_stream_clear(&new_oss);
//         return;
//       }
      
//       dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
//       if (dmx == NULL)
//         die("malloc");
//       memset(dmx, 0, sizeof(ogm_demuxer_t));

//       try {
//         dmx->packetizer =
//           new textsubs_packetizer_c(&async);
//       } catch (error_c error) {
//         fprintf(stderr, "Error: ogm_reader: could not initialize text "
//                 "subtitle packetizer for stream id %d. Will try to "
//                 "continue and ignore this stream.\n", numstreams);
//         free(dmx);
//         ogg_stream_clear(&new_oss);
//         return;
//       }
//       dmx->stype = OGM_STREAM_TYPE_TEXT;

//       dmx->serial = ogg_page_serialno(og);
//       memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
//       dmx->sid = ntstreams;
//       add_new_demuxer(dmx);
//       if (verbose)
//         fprintf(stdout, "OGG/OGM demultiplexer (%s): using text subtitle "
//                 "output module for stream %d.\n", filename, numstreams);
      
      return;
    }
  }

  /*
   * The old OggDS headers (see MPlayer's source, libmpdemux/demux_ogg.c)
   * are not supported.
   */

  // Failed to detect a supported header.  
  ogg_stream_clear(&new_oss);
  free(dmx->packet_data[0]);
  free(dmx);

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
  int64_t flags;
  
  dmx = find_demuxer(ogg_page_serialno(og));
  if (dmx == NULL)
    return;

  ogg_stream_pagein(&dmx->os, og);
  while (ogg_stream_packetout(&dmx->os, &op) == 1) {
    hdrlen = (*op.packet & PACKET_LEN_BITS01) >> 6;
    hdrlen |= (*op.packet & PACKET_LEN_BITS2) << 1;
    if ((hdrlen > 0) && (op.bytes >= (hdrlen + 1)))
      for (i = 0, lenbytes = 0; i < hdrlen; i++) {
        lenbytes = lenbytes << 8;
        lenbytes += *((unsigned char *)op.packet + hdrlen - i);
      }

    eos = op.e_o_s;

    if (((*op.packet & 3) != PACKET_TYPE_HEADER) &&
        ((*op.packet & 3) != PACKET_TYPE_COMMENT)) {

      if (dmx->stype == OGM_STREAM_TYPE_VIDEO) {
        flags = hdrlen > 0 ? lenbytes : 1;
        if (*op.packet & PACKET_IS_SYNCPOINT)
          flags |= VFT_IFRAME;
        dmx->packetizer->process(&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                                 -1, flags);
        dmx->units_processed += (hdrlen > 0 ? lenbytes : 1);

      } else if (dmx->stype == OGM_STREAM_TYPE_TEXT) {
        dmx->units_processed++;

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
      return;
    }
  }
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

  ogg_stream_pagein(&dmx->os, og);
  while (ogg_stream_packetout(&dmx->os, &op) == 1) {
    if ((*op.packet & 3) == PACKET_TYPE_HEADER) {
      dmx->num_packets++;
      dmx->packet_data[2] = (unsigned char *)malloc(op.bytes);
      if (dmx->packet_data[2] == NULL)
        die("malloc");
      memcpy(dmx->packet_data[2], op.packet, op.bytes);
      dmx->packet_sizes[2] = op.bytes;
    } else if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
      dmx->num_packets++;
      dmx->packet_data[1] = (unsigned char *)malloc(op.bytes);
      if (dmx->packet_data[1] == NULL)
        die("malloc");
      memcpy(dmx->packet_data[1], op.packet, op.bytes);
      dmx->packet_sizes[1] = op.bytes;
    }
  }
}

/*
 * Read all header packets and - for Vorbis streams - the comment and
 * codec data packets.
 */
int ogm_reader_c::read_headers() {
  int            done, i;
  ogm_demuxer_t *dmx;
  ogg_page       og;
  
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
        if ((dmx->stype == OGM_STREAM_TYPE_VORBIS) && (dmx->num_packets < 3)) {
          // Not all three headers have been found for this Vorbis stream.
          done = 0;
          break;
        }
      }
    }
  }
  
  fseek(file, 0, SEEK_SET);
  ogg_sync_clear(&oy);
  ogg_sync_init(&oy);

  return 1;
}

/*
 * General reader. Before returning it MUST guarantee that each demuxer has
 * a page available OR that the corresponding stream is finished.
 */
int ogm_reader_c::read() {
  int            done, i;
  ogm_demuxer_t *dmx;
  ogg_page       og;
  
  if (packet_available())
    return EMOREDATA;
  
  done = 0;
  while (!done) {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0)
      return 0;

    // Is this the first page of a new stream?
    if (ogg_page_bos(&og))
      continue;
    else // No, so process it normally.
      process_page(&og);    

    done = 1;
    if (num_sdemuxers == 0) 
      // We haven't encountered a stream yet that we want to extract.
      done = 0;
    else
      for (i = 0; i < num_sdemuxers; i++) {
        dmx = sdemuxers[i];
        if (!dmx->eos && !dmx->packetizer->packet_available())
          // This stream is not finished but has not produced a new page yet.
          done = 0;
      }
  }
  // Each known stream has now produced a new page or is at its end.
  
  if (num_sdemuxers == 0)
    return EMOREDATA;

  // Are there streams that have not finished yet?
  for (i = 0; i < num_sdemuxers; i++)
    if (!sdemuxers[i]->eos)
      return EMOREDATA;
  
  // No, we're done with this file.
  return 0;
}

packet_t *ogm_reader_c::get_packet() {
  generic_packetizer_c *winner;
  ogm_demuxer_t        *demuxer;
  int                   i;
  
  winner = NULL;
  
  for (i = 0; i < num_sdemuxers; i++) {
    demuxer = sdemuxers[i];
    if (winner == NULL) {
      if (demuxer->packetizer->packet_available())
        winner = demuxer->packetizer;
    } else if (winner->packet_available() &&
               (winner->get_smallest_timecode() >
                demuxer->packetizer->get_smallest_timecode()))
      winner = demuxer->packetizer;
  }
  
  if (winner != NULL)
    return winner->get_packet();
  else
    return NULL;
}

int ogm_reader_c::display_priority() {
  int i;
  
  for (i = 0; i < num_sdemuxers; i++)
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO)
      return DISPLAYPRIORITY_MEDIUM;

  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void ogm_reader_c::display_progress() {
  int i;

  for (i = 0; i < num_sdemuxers; i++)  
    if (sdemuxers[i]->stype == OGM_STREAM_TYPE_VIDEO) {
      fprintf(stdout, "progress: %d frames\r", sdemuxers[i]->units_processed);
      fflush(stdout);
      return;
    }

  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

#endif // HAVE_OGGVORBIS
