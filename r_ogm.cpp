/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_ogm.cpp
  OGG demultiplexer module (supports both the 'flat' Vorbis-only OGG
  files and the OGG files produced by ogmmerge itself)

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "ogmmerge.h"
#include "ogmstreams.h"
#include "queue.h"
#include "r_ogm.h"
#include "p_vorbis.h"
#include "p_video.h"
#include "p_pcm.h"
#include "p_textsubs.h"
#include "p_mp3.h"
#include "p_ac3.h"
#include "vorbis_header_utils.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*
 * Probes a file by simply comparing the first four bytes to 'OggS'.
 */
int ogm_reader_c::probe_file(FILE *file, u_int64_t size) {
  char data[4];
  
  if (size < 4)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(data, 1, 4, file) != 4) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  if (strncmp(data, "OggS", 4))
    return 0;
  return 1;
}

/*
 * Opens the file for processing, initializes an ogg_sync_state used for
 * reading from an OGG stream.
 */
ogm_reader_c::ogm_reader_c(char *fname, unsigned char *astreams,
                           unsigned char *vstreams, unsigned char *tstreams,
                           audio_sync_t *nasync, range_t *nrange,
                           char **ncomments, char *nfourcc) throw (error_c) {
  u_int64_t size;
  
  if ((file = fopen(fname, "r")) == NULL)
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

  if (astreams != NULL)
    this->astreams = (unsigned char *)strdup((char *)astreams);
  else
    this->astreams = NULL;
  if (vstreams != NULL)
    this->vstreams = (unsigned char *)strdup((char *)vstreams);
  else
    this->vstreams = NULL;
  if (tstreams != NULL)
    this->tstreams = (unsigned char *)strdup((char *)tstreams);
  else
    this->tstreams = NULL;
  if (nfourcc != NULL) {
    fourcc = strdup(nfourcc);
    if (fourcc == NULL)
      die("malloc");
  } else
    fourcc = NULL;
  if (verbose)
    fprintf(stdout, "Using OGG/OGM demultiplexer for %s.\n", fname);
  filename = strdup(fname);
  if (filename == NULL)
    die("malloc");
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));
  o_eos = 0;
  if (ncomments == NULL)
    comments = NULL;
  else
    comments = dup_comments(ncomments);
}

ogm_reader_c::~ogm_reader_c() {
  ogm_demuxer_t *dmx, *tmp;
  
  if (astreams != NULL)
    free(astreams);
  if (vstreams != NULL)
    free(vstreams);
  if (tstreams != NULL)
    free(tstreams);
  ogg_sync_clear(&oy);
  dmx = sdemuxers;
  while (dmx != NULL) {
    ogg_stream_clear(&dmx->os);
    delete dmx->packetizer;
    tmp = dmx->next;
    free(dmx);
    dmx = tmp;
  }
  if (filename != NULL)
    free(filename);
  if (comments != NULL)
    free_comments(comments);
  if (fourcc != NULL)
    free(fourcc);
}

/*
 * Checks whether the user wants a certain stream extracted or not.
 */
int ogm_reader_c::demuxing_requested(unsigned char *streams, int streamno) {
  int i;
  
  if (streams == NULL)
    return 1;

  for (i = 0; i < strlen((char *)streams); i++)
    if (streams[i] == streamno)
      return 1;

  return 0;
}

ogm_demuxer_t *ogm_reader_c::find_demuxer(int serialno) {
  ogm_demuxer_t *demuxer = sdemuxers;
  
  while (demuxer != NULL) {
    if (demuxer->serial == serialno)
      return demuxer;
    demuxer = demuxer->next;
  }
  
  return NULL;
}

void ogm_reader_c::flush_packetizers() {
  ogm_demuxer_t *demuxer;
  
  demuxer = sdemuxers;
  while (demuxer != NULL) {
    // Is the input stream at its end and has not yet flagged EOS?
    if (!demuxer->eos)
      demuxer->packetizer->produce_eos_packet();
    demuxer->packetizer->flush_pages();
    demuxer = demuxer->next;
  }
  
  return;
}

#define BUFFER_SIZE 4096

/*
 * Reads an OGG page from the stream. Returns 0 if there are no more pages
 * left, EMOREDATA otherwise.
 */
int ogm_reader_c::read_page(ogg_page *og) {
  int np, done, nread;
  char *buf;
  
  done = 0;
  while (!done) {
    np = ogg_sync_pageseek(&oy, og);
    // np < 0 is the error case. Should not happen with local OGG files.
    if (np < 0) {
      fprintf(stderr, "FATAL: ogm_reader: ogg_sync_pageseek failed\n");
      exit(1);
    }
    // np == 0 means that there is not enough data for a complete page.
    if (np == 0) {
      buf = ogg_sync_buffer(&oy, BUFFER_SIZE);
      if (!buf) {
        fprintf(stderr, "FATAL: ogm_reader: ogg_sync_buffer failed\n");
        exit(1);
      }
      if ((nread = fread(buf, 1, BUFFER_SIZE, file)) <= 0) {
        flush_packetizers();
        return 0;
      }
      ogg_sync_wrote(&oy, nread);
    } else
      // Alright, we have a page.
      done = 1;    
  }
  
  // Here EMOREDATA actually indicates success - a page has been read.
  return EMOREDATA;
}

void ogm_reader_c::add_new_demuxer(ogm_demuxer_t *dmx) {
  ogm_demuxer_t *append_to;
  
  if (sdemuxers == NULL) {
    sdemuxers = dmx;
    return;
  }
  
  append_to = sdemuxers;
  while (append_to->next != NULL)
    append_to = append_to->next;
  append_to->next = dmx;
}

/*
 * Checks every demuxer if it has a page available.
 */
int ogm_reader_c::pages_available() {
  ogm_demuxer_t *dmx;
  
  if (sdemuxers == NULL)
    return 0;
  dmx = sdemuxers;
  while (dmx != NULL) {
    if (!dmx->packetizer->page_available())
      return 0;
    dmx = dmx->next;
  }
  
  return 1;
}

/*
 * The page is the beginning of a new stream. Check the contents for known
 * stream headers. If it is a known stream and the user has requested that
 * it should be extracted than allocate a new packetizer based on the
 * stream type and store the needed data in a new ogm_demuxer_t.
 */
void ogm_reader_c::handle_new_stream(ogg_page *og) {
  ogg_stream_state  new_oss;
  ogg_packet        op;
  ogm_demuxer_t    *dmx;
  stream_header    *sth;
  char             *codec;

  if (ogg_stream_init(&new_oss, ogg_page_serialno(og))) {
    fprintf(stderr, "FATAL: ogm_reader: ogg_stream_init for stream number " \
            "%d failed. Will try to continue and ignore this stream.",
            numstreams + 1);
    return;
  }

  // Read the first page and get its first packet.
  ogg_stream_pagein(&new_oss, og);
  ogg_stream_packetout(&new_oss, &op);
  
  /*
   * Check the contents for known stream headers. This one is the
   * standard Vorbis header.
   */
  if ((op.bytes >= 7) && !strncmp((char *)&op.packet[1], "vorbis", 6)) {
    nastreams++;
    numstreams++;
    if (!demuxing_requested(astreams, nastreams)) {
      ogg_stream_clear(&new_oss);
      return;
    }
    dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
    if (dmx == NULL)
      die("malloc");
    memset(dmx, 0, sizeof(ogm_demuxer_t));
    try {
      dmx->packetizer = new vorbis_packetizer_c(&async, &range, comments);
    } catch (error_c error) {
      fprintf(stderr, "FATAL: ogm_reader: could not initialize Vorbis " \
              "packetizer for stream id %d. Will try to continue and " \
              "ignore this stream.\n", numstreams);
      free(dmx);
      ogg_stream_clear(&new_oss);
      return;
    }
    dmx->stype = OGM_STREAM_TYPE_VORBIS;
    dmx->serial = ogg_page_serialno(og);
    memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
    dmx->sid = nastreams;
    add_new_demuxer(dmx);
    if (verbose)
      fprintf(stdout, "OGG/OGM demultiplexer (%s): using Vorbis audio " \
              "output module for stream %d.\n", filename, numstreams);
    do {
      ((vorbis_packetizer_c *)dmx->packetizer)->
        process(&op, ogg_page_granulepos(og));
      if (op.e_o_s) {
        dmx->packetizer->flush_pages();
        dmx->eos = 1;
        return;
      }
    } while (ogg_stream_packetout(&dmx->os, &op));
    
    return;
  }

  // The new stream headers introduced by OggDS (see ogmstreams.h).
  if (((*op.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER) &&
      (op.bytes >= ((int)sizeof(stream_header) + 1 - sizeof(int)))) {
    sth = (stream_header *)(op.packet + 1);
    if (!strncmp(sth->streamtype, "video", 5)) {
      nvstreams++;
      numstreams++;
      if (!demuxing_requested(vstreams, nvstreams)) {
        ogg_stream_clear(&new_oss);
        return;
      }
      
      dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
      if (dmx == NULL)
        die("malloc");
      memset(dmx, 0, sizeof(ogm_demuxer_t));
      if (fourcc == NULL)
        codec = sth->subtype;
      else
        codec = fourcc;
      try {
        dmx->packetizer =
          new video_packetizer_c(codec,
                                 (double)10000000 / (double)sth->time_unit, 
                                 sth->sh.video.width, sth->sh.video.height,
                                 sth->bits_per_sample, sth->buffersize, NULL,
                                 &range, comments);
      } catch (error_c error) {
        fprintf(stderr, "FATAL: ogm_reader: could not initialize video " \
                "packetizer for stream id %d. Will try to continue and " \
                "ignore this stream.\n", numstreams);
        free(dmx);
        ogg_stream_clear(&new_oss);
        return;
      }
      ((video_packetizer_c *)dmx->packetizer)->set_chapter_info(chapter_info);
      dmx->stype = OGM_STREAM_TYPE_VIDEO;
      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = nvstreams;
      add_new_demuxer(dmx);
      if (video_fps < 0)
        video_fps = 10000000.0 / (float)sth->time_unit;
      if (verbose)
        fprintf(stdout, "OGG/OGM demultiplexer (%s): using video output " \
                  "module for stream %d.\n", filename, numstreams);
      
      return;
    }
    
    if (!strncmp(sth->streamtype, "audio", 5)) {
      char buf[5];
      long codec_id;
      
      nastreams++;
      numstreams++;
      if (!demuxing_requested(astreams, nastreams)) {
        ogg_stream_clear(&new_oss);
        return;
      }
      
      memcpy(buf, (char *)sth->subtype, 4);
      buf[4] = 0;
      codec_id = strtol(buf, (char **)NULL, 16);
      
      dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
      if (dmx == NULL)
        die("malloc");
      memset(dmx, 0, sizeof(ogm_demuxer_t));
      switch (codec_id) {
        case 0x0001: // raw PCM
          try {
            dmx->packetizer =
              new pcm_packetizer_c(sth->samples_per_unit,
                                   sth->sh.audio.channels, 
                                   sth->bits_per_sample, &async, &range,
                                   comments);
          } catch (error_c error) {
            fprintf(stderr, "FATAL: ogm_reader: could not initialize PCM " \
                    "packetizer for stream id %d. Will try to continue and " \
                    "ignore this stream.\n", numstreams);
            free(dmx);
            ogg_stream_clear(&new_oss);
            return;
          }
          dmx->stype = OGM_STREAM_TYPE_PCM;
          if (verbose)
            fprintf(stdout, "OGG/OGM demultiplexer (%s): using PCM output " \
                      "module for stream %d.\n", filename, numstreams);
          break;
        case 0x0055: // MP3
          try {
            dmx->packetizer = 
              new mp3_packetizer_c(sth->samples_per_unit,
                                   sth->sh.audio.channels,
                                   sth->sh.audio.avgbytespersec * 8 / 1000,
                                   &async, &range, comments);
          } catch (error_c error) {
            fprintf(stderr, "FATAL: ogm_reader: could not initialize MP3 " \
                    "packetizer for stream id %d. Will try to continue and " \
                    "ignore this stream.\n", numstreams);
            free(dmx);
            ogg_stream_clear(&new_oss);
            return;
          }
          dmx->stype = OGM_STREAM_TYPE_MP3;
          if (verbose)
            fprintf(stdout, "OGG/OGM demultiplexer (%s): using MP3 output " \
                      "module for stream %d.\n", filename, numstreams);
          break;
        case 0x2000: // AC3
          try {
            dmx->packetizer = 
              new ac3_packetizer_c(sth->samples_per_unit,
                                   sth->sh.audio.channels,
                                   sth->sh.audio.avgbytespersec * 8 / 1000,
                                   &async, &range, comments);
          } catch (error_c error) {
            fprintf(stderr, "FATAL: ogm_reader: could not initialize AC3 " \
                    "packetizer for stream id %d. Will try to continue and " \
                    "ignore this stream.\n", numstreams );
            free(dmx);
            ogg_stream_clear(&new_oss);
            return;
          }
          dmx->stype = OGM_STREAM_TYPE_AC3;
          if (verbose)
            fprintf(stdout, "OGG/OGM demultiplexer (%s): using AC3 output " \
                      "module for stream %d.\n", filename, numstreams);
          break;
        default:
          fprintf(stderr, "FATAL: ogm_reader: Audio stream %d has a unknown " \
                  "type. Will try to continue and ignore this stream.\n",
                  numstreams);
          free(dmx);
          ogg_stream_clear(&new_oss);
          return;
          break;
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
      if (!demuxing_requested(tstreams, ntstreams)) {
        ogg_stream_clear(&new_oss);
        return;
      }
      
      dmx = (ogm_demuxer_t *)malloc(sizeof(ogm_demuxer_t));
      if (dmx == NULL)
        die("malloc");
      memset(dmx, 0, sizeof(ogm_demuxer_t));

      try {
        dmx->packetizer =
          new textsubs_packetizer_c(&async, &range, comments);
      } catch (error_c error) {
        fprintf(stderr, "FATAL: ogm_reader: could not initialize text " \
                "subtitle packetizer for stream id %d. Will try to " \
                "continue and ignore this stream.\n", numstreams);
        free(dmx);
        ogg_stream_clear(&new_oss);
        return;
      }
      dmx->stype = OGM_STREAM_TYPE_TEXT;

      dmx->serial = ogg_page_serialno(og);
      memcpy(&dmx->os, &new_oss, sizeof(ogg_stream_state));
      dmx->sid = ntstreams;
      add_new_demuxer(dmx);
      if (verbose)
        fprintf(stdout, "OGG/OGM demultiplexer (%s): using text subtitle " \
                "output module for stream %d.\n", filename, numstreams);
      
      return;
    }
  }

  /*
   * The old OggDS headers (see MPlayer's source, libmpdemux/demux_ogg.c)
   * are not supported.
   */

// Failed to detect a supported header.  
  ogg_stream_clear(&new_oss);
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
  ogg_packet     op;
  int            hdrlen, eos, i;
  long           lenbytes;
  
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
    if (o_eos)
      op.e_o_s = 0;
    switch (dmx->stype) {
      case OGM_STREAM_TYPE_VORBIS:
        ((vorbis_packetizer_c *)dmx->packetizer)->
          process(&op, ogg_page_granulepos(og));
        break;
      case OGM_STREAM_TYPE_VIDEO:
        if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
          /*
           * Give the video packetizer the old comments if the user did not
           * provide comments via -c.
           */
          if ((comments == NULL) || (comments[0] == NULL)) {
            vorbis_comment vc;
            if (vorbis_unpack_comment(&vc, (char *)op.packet, op.bytes) >= 0)
              dmx->packetizer->set_comments(&vc);
          }
        } else if ((*op.packet & 3) != PACKET_TYPE_HEADER) {
          ((video_packetizer_c *)dmx->packetizer)->
            process((char *)&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                    hdrlen > 0 ? lenbytes : 1,
                    *op.packet & PACKET_IS_SYNCPOINT, op.e_o_s);
          dmx->units_processed += (hdrlen > 0 ? lenbytes : 1);
        }
        break;
      case OGM_STREAM_TYPE_PCM:
        if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
          /*
           * Give the video packetizer the old comments if the user did not
           * provide comments via -c.
           */
          if ((comments == NULL) || (comments[0] == NULL)) {
            vorbis_comment vc;
            if (vorbis_unpack_comment(&vc, (char *)op.packet, op.bytes) >= 0)
              dmx->packetizer->set_comments(&vc);
          }
        } else if ((*op.packet & 3) != PACKET_TYPE_HEADER) {
          ((pcm_packetizer_c *)dmx->packetizer)->
            process((char *)&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                    op.e_o_s);
          dmx->units_processed += op.bytes - 1;
        }
        break;
      case OGM_STREAM_TYPE_MP3: // MP3
        if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
          /*
           * Give the video packetizer the old comments if the user did not
           * provide comments via -c.
           */
          if ((comments == NULL) || (comments[0] == NULL)) {
            vorbis_comment vc;
            if (vorbis_unpack_comment(&vc, (char *)op.packet, op.bytes) >= 0)
              dmx->packetizer->set_comments(&vc);
          }
        } else if ((*op.packet & 3) != PACKET_TYPE_HEADER) {
          ((mp3_packetizer_c *)dmx->packetizer)->
            process((char *)&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                    op.e_o_s);
          dmx->units_processed += op.bytes - 1;
        }
        break;
      case OGM_STREAM_TYPE_AC3: // AC3
        if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
          /*
           * Give the video packetizer the old comments if the user did not
           * provide comments via -c.
           */
          if ((comments == NULL) || (comments[0] == NULL)) {
            vorbis_comment vc;
            if (vorbis_unpack_comment(&vc, (char *)op.packet, op.bytes) >= 0)
              dmx->packetizer->set_comments(&vc);
          }
        } else if ((*op.packet & 3) != PACKET_TYPE_HEADER) {
          ((ac3_packetizer_c *)dmx->packetizer)->
            process((char *)&op.packet[hdrlen + 1], op.bytes - 1 - hdrlen,
                    op.e_o_s);
          dmx->units_processed += op.bytes - 1;
        }
        break;
      case OGM_STREAM_TYPE_TEXT: // text subtitles
        if ((*op.packet & 3) == PACKET_TYPE_COMMENT) {
          /*
           * Give the text packetizer the old comments if the user did not
           * provide comments via -c.
           */
          if ((comments == NULL) || (comments[0] == NULL)) {
            vorbis_comment vc;
            if (vorbis_unpack_comment(&vc, (char *)op.packet, op.bytes) >= 0)
              dmx->packetizer->set_comments(&vc);
          }
        } else if ((*op.packet & 3) != PACKET_TYPE_HEADER) {
          char *subs = (char *)&op.packet[1 + hdrlen];
          if ((*subs != 0) && (*subs != '\n') && (*subs != '\r') &&
              strcmp(subs, " ")) {
            ((textsubs_packetizer_c *)dmx->packetizer)->
              process(op.granulepos, op.granulepos + lenbytes, subs,
                      op.e_o_s);
            dmx->units_processed++;
          }
        }
        break;
    }
    if (eos) {
      dmx->packetizer->flush_pages();
      dmx->eos = 1;
      return;
    }
  }
}

/*
 * General reader. Before returning it MUST guarantee that each demuxer has
 * a page available OR that the corresponding stream is finished.
 */
int ogm_reader_c::read() {
  int            done;
  ogm_demuxer_t *dmx;
  ogg_page       og;
  
  if (pages_available() && !o_eos)
    return EMOREDATA;
  
  done = 0;
  while (!done) {
    // Make sure we have a page that we can work with.
    if (read_page(&og) == 0)
      return 0;
    // Is this the first page of a new stream?
    if (ogg_page_bos(&og))
      handle_new_stream(&og);
    else // No, so process it normally.
      process_page(&og);    
    done = 1;
    dmx = sdemuxers;
    if (dmx == NULL) 
      // We haven't encountered a stream yet that we want to extract.
      done = 0;
    else
      do {
        if (!dmx->eos && !dmx->packetizer->page_available())
          // This stream is not finished but has not yet produced a new page.
          done = 0;
        dmx = dmx->next;
      } while ((dmx != NULL) && done);
  }
  // Each known stream has now produced a new page or is at its end.
  
  dmx = sdemuxers;
  if (dmx == NULL)
    return EMOREDATA;

  // Are there streams that have not finished yet?
  do {
    if (!dmx->eos)
      return EMOREDATA;
    dmx = dmx->next;
  } while (dmx != NULL);
  
  // No, we're done with this file.
  return 0;
}

int ogm_reader_c::serial_in_use(int serial) {
  ogm_demuxer_t *demuxer;
  
  demuxer = sdemuxers;
  while (demuxer != NULL) {
    if (demuxer->packetizer->serial_in_use(serial))
      return 1;
    demuxer = demuxer->next;
  }
  return 0;
}

ogmmerge_page_t *ogm_reader_c::get_header_page(int header_type) {
  ogmmerge_page_t *ompage = NULL;
  ogm_demuxer_t   *demuxer;
  
  demuxer = sdemuxers;
  while (demuxer != NULL) {
    if (demuxer->packetizer != NULL) {
      ompage = demuxer->packetizer->get_header_page(header_type);
      if (ompage != NULL)
        return ompage;
    }
    demuxer = demuxer->next;
  }
  
  return NULL;
}

ogmmerge_page_t *ogm_reader_c::get_page() {
  generic_packetizer_c *winner;
  ogm_demuxer_t        *demuxer;
  
  winner = NULL;
  
  demuxer = sdemuxers;
  while (demuxer != NULL) {
    if (winner == NULL) {
      if (demuxer->packetizer->page_available())
        winner = demuxer->packetizer;
    } else if (winner->page_available() &&
               (winner->get_smallest_timestamp() >
                demuxer->packetizer->get_smallest_timestamp()))
      winner = demuxer->packetizer;
    demuxer = demuxer->next;
  }
  
  if (winner != NULL)
    return winner->get_page();
  else
    return NULL;
}

void ogm_reader_c::overwrite_eos(int no_eos) {
  o_eos = no_eos;
}

int ogm_reader_c::display_priority() {
  ogm_demuxer_t *dmx;
  
  dmx = sdemuxers;
  while (dmx != NULL) {
    if (dmx->stype == OGM_STREAM_TYPE_VIDEO)
      return DISPLAYPRIORITY_MEDIUM;
    dmx = dmx->next;
  }
  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void ogm_reader_c::display_progress() {
  ogm_demuxer_t *dmx;
  
  dmx = sdemuxers;
  while (dmx != NULL) {
    if (dmx->stype == OGM_STREAM_TYPE_VIDEO) {
      fprintf(stdout, "progress: %d frames\r", dmx->units_processed);
      fflush(stdout);
      return;
    }
    dmx = dmx->next;
  }

  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

generic_packetizer_c *ogm_reader_c::set_packetizer(generic_packetizer_c *np) {
  generic_packetizer_c *old;

  if (sdemuxers == NULL)
    return NULL;
  old = sdemuxers->packetizer;
  sdemuxers->packetizer = np;
  return old;
}

void ogm_reader_c::reset() {
  ogm_demuxer_t *dmx;
  
  dmx = sdemuxers;
  while (dmx != NULL) {
    dmx->packetizer->reset();
    dmx = dmx->next;
  }
}
