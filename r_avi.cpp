/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_avi.cpp,v 1.6 2003/02/19 09:31:24 mosu Exp $
    \brief AVI demultiplexer module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <avilib.h>
}

#include "common.h"
#include "error.h"
#include "mkvmerge.h"
#include "queue.h"
#include "r_avi.h"
#include "p_video.h"
//#include "p_pcm.h"
#include "p_mp3.h"
//#include "p_ac3.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG
static void dump_bih(BITMAPINFOHEADER *b) {
  char *bi_compression = (char *)&b->bi_compression;
  int i;

  if (b == NULL) {
    fprintf(stdout, "[bih] IS NULL\n");
    return;
  }
  fprintf(stdout, "[bih] size: %u, width: %u, height: %u\n", b->bi_size,
          b->bi_width, b->bi_height);
  fprintf(stdout, "[bih] planes: %u, bit_count: %u, compression: 0x%04x "
          "(%c%c%c%c)\n",
          b->bi_planes, b->bi_bit_count, b->bi_compression, bi_compression[0],
          bi_compression[1], bi_compression[2], bi_compression[3]);
  fprintf(stdout, "[bih] size_image: %u xppm: %u yppm: %u\n", b->bi_size_image,
          b->bi_x_pels_per_meter, b->bi_y_pels_per_meter);
  fprintf(stdout, "[bih] clr_used: %u, clr_important: %d\n", b->bi_clr_used,
          b->bi_clr_important);
  if (b->bi_size > sizeof(BITMAPINFOHEADER)) {
    unsigned char *data = (unsigned char *)b + sizeof(BITMAPINFOHEADER);
    fprintf(stdout, "[bih] data: ");
    for (i = 0; i < (b->bi_size - sizeof(BITMAPINFOHEADER)); i++)
      fprintf(stdout, "%x%x ", (data[i] & 0xf0) >> 4, data[i] & 0x0f);
    fprintf(stdout, "\n\n");
  } else
    fprintf(stdout, "[bih] no additional data\n\n");
}

static void dump_wfe(WAVEFORMATEX *w) {
  int i;
  
  if (w == NULL) {
    fprintf(stdout, "[wfe] IS NULL\n");
    return;
  }
  fprintf(stdout, "[wfe] format 0x%04x, channels %u, samp/sec %u\n",
          w->w_format_tag, w->n_channels, w->n_samples_per_sec);
  fprintf(stdout, "[wfe] avg b/s %u, block align %u, bits/sam %u\n",
          w->n_avg_bytes_per_sec, w->n_block_align, w->w_bits_per_sample);
  fprintf(stdout, "[wfe] size %u\n", w->cb_size);
  if (w->cb_size > 0) {
    unsigned char *data = (unsigned char *)w + sizeof(WAVEFORMATEX);
    fprintf(stdout, "[wfe] data: ");
    for (i = 0; i < w->cb_size; i++)
      fprintf(stdout, "%x%x ", (data[i] & 0xf0) >> 4, data[i] & 0x0f);
    fprintf(stdout, "\n\n");
  } else
    fprintf(stdout, "[wfe] no additional data\n\n");
}
#endif // DEBUG

int avi_reader_c::probe_file(FILE *file, u_int64_t size) {
  char data[12];
  
  if (size < 12)
    return 0;
  fseek(file, 0, SEEK_SET);
  if (fread(data, 1, 12, file) != 12) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  if(strncasecmp(data, "RIFF", 4) || strncasecmp(data+8, "AVI ", 4))
    return 0;
  return 1;
}

/*
 * allocates and initializes local storage for a particular
 * substream conversion.
 */
avi_reader_c::avi_reader_c(char *fname, unsigned char *astreams,
                           unsigned char *vstreams, audio_sync_t *nasync,
                           range_t *nrange, char *nfourcc)
                           throw (error_c) {
  int            fsize, i;
  u_int64_t      size;
  FILE          *f;
  int            extract_video = 1;
  avi_demuxer_t *demuxer;
  char          *codec;

  if (fname == NULL)
    throw error_c("avi_reader: fname == NULL !?");
  
  if ((f = fopen(fname, "r")) == NULL)
    throw error_c("avi_reader: Could not open source file.");
  if (fseek(f, 0, SEEK_END) != 0)
    throw error_c("avi_reader: Could not seek to end of file.");
  size = ftell(f);
  if (fseek(f, 0, SEEK_SET) != 0)
    throw error_c("avi_reader: Could not seek to beginning of file.");
  if (!avi_reader_c::probe_file(f, size))
    throw error_c("avi_reader: Source is not a valid AVI file.");
  fclose(f);

  if (verbose)
    fprintf(stderr, "Using AVI demultiplexer for %s. Opening file. This "
            "may take some time depending on the file's size.\n", fname);
  rederive_keyframes = 0;
  if ((avi = AVI_open_input_file(fname, 1)) == NULL) {
    const char *msg = "avi_reader: Could not initialize AVI source. Reason: ";
    char *s, *error;
    error = AVI_strerror();
    s = (char *)malloc(strlen(msg) + strlen(error) + 1);
    if (s == NULL)
      die("malloc");
    sprintf(s, "%s%s", msg, error);
    throw error_c(s);
  }

  if (astreams != NULL)
    this->astreams = (unsigned char *)strdup((char *)astreams);
  else
    this->astreams = NULL;
  if (vstreams != NULL)
    this->vstreams = (unsigned char *)strdup((char *)vstreams);
  else
    this->vstreams = NULL;

/*  if (ncomments == NULL)
    comments = ncomments;
  else
    comments = dup_comments(ncomments);*/
    
  fps = AVI_frame_rate(avi);
  if (video_fps < 0)
    video_fps = fps;
  frames = 0;
  fsize = 0;
  maxframes = AVI_video_frames(avi);
  for (i = 0; i < maxframes; i++)
    if (AVI_frame_size(avi, i) > fsize)
      fsize = AVI_frame_size(avi, i);
  max_frame_size = fsize;

  if (vstreams != NULL) {
    extract_video = 0;
    for (i = 0; i < strlen((char *)vstreams); i++) {
      if (vstreams[i] > 1)
        fprintf(stderr, "Warning: avi_reader: only one video stream per AVI " \
                "is supported. Will not ignore -d %d.\n", vstreams[i]);
      else if (vstreams[i] == 1)
        extract_video = 1;
    }
  }
  if (extract_video) {
    codec = AVI_video_compressor(avi);
    if (!strcasecmp(codec, "DIV3") ||
        !strcasecmp(codec, "AP41") || // Angel Potion
        !strcasecmp(codec, "MPG3") ||
        !strcasecmp(codec, "MP43"))
      is_divx = RAVI_DIVX3;
    else if (!strcasecmp(codec, "MP42") ||
             !strcasecmp(codec, "DIV2") ||
             !strcasecmp(codec, "DIVX") ||
             !strcasecmp(codec, "XVID") ||
             !strcasecmp(codec, "DX50"))
      is_divx = RAVI_MPEG4;
    else
      is_divx = 0;

    if (nfourcc != NULL)
      codec = nfourcc;
    vpacketizer = new video_packetizer_c(avi->bitmap_info_header,
                                         avi->bitmap_info_header == NULL ? 0 : 
                                           sizeof(BITMAPINFOHEADER),
                                         codec, AVI_frame_rate(avi),
                                         AVI_video_width(avi),
                                         AVI_video_height(avi),
                                         24, // fixme!
                                         fsize, NULL, nrange, 1);
    if (verbose)
      fprintf(stderr, "+-> Using video output module for video stream.\n");
  } else
    vpacketizer = NULL;

#ifdef DEBUG  
  dump_bih(avi->bitmap_info_header);
#endif
  
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));
  ademuxers = NULL;
  if (astreams != NULL) { // use only specific audio streams (or none at all)
    for (i = 0; i < strlen((char *)astreams); i++) {
      if (astreams[i] > AVI_audio_tracks(avi))
        fprintf(stderr, "Warning: avi_reader: the AVI does not contain an " \
                "audio stream with the id %d. Number of audio streams: %d\n",
                astreams[i], AVI_audio_tracks(avi));
      else {
        int already_extracting = 0;
        avi_demuxer_t *demuxer = ademuxers;
        
        while (demuxer) {
          if (demuxer->aid == astreams[i]) {
            already_extracting = 1;
            break;
          }
          demuxer = demuxer->next;
        }
        if (already_extracting)
          fprintf(stderr, "Warning: avi_reader: already extracting audio " \
                  "stream number %d. Will only do this once.\n", astreams[i]);
        else
          add_audio_demuxer(avi, astreams[i] - 1);
      }
    }
  } else // use all audio streams (no parameter specified)*/
    for (i = 0; i < AVI_audio_tracks(avi); i++)
      add_audio_demuxer(avi, i);

  demuxer = ademuxers;
  while (demuxer != NULL) {
    long bps = demuxer->samples_per_second * demuxer->channels *
               demuxer->bits_per_sample / 8;
    if (bps > fsize)
      fsize = bps;
    demuxer = demuxer->next;
  }
  max_frame_size = fsize;
  chunk = (char *)malloc(fsize < 16384 ? 16384 : fsize);
  if (chunk == NULL)
    die("malloc");
  act_wchar = 0;
  old_key = 0;
  old_chunk = NULL;
  video_done = 0;
}

avi_reader_c::~avi_reader_c() {
  struct avi_demuxer_t *demuxer, *tmp;
  
  if (astreams != NULL)
    free(astreams);
  if (vstreams != NULL)
    free(vstreams);
  if (avi != NULL)
    AVI_close(avi);
  if (chunk != NULL)
    free(chunk);
  if (vpacketizer != NULL)
    delete vpacketizer;
  demuxer = ademuxers;
  while (demuxer) {
    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;
    tmp = demuxer->next;
    free(demuxer);
    demuxer = tmp;
  }
  if (old_chunk != NULL)
    free(old_chunk);
}

int avi_reader_c::add_audio_demuxer(avi_t *avi, int aid) {
  avi_demuxer_t *demuxer, *append_to;
  
  append_to = ademuxers;
  while ((append_to != NULL) && (append_to->next != NULL))
    append_to = append_to->next;
  AVI_set_audio_track(avi, aid);
  demuxer = (avi_demuxer_t *)malloc(sizeof(avi_demuxer_t));
  if (demuxer == NULL)
    die("malloc");
  memset(demuxer, 0, sizeof(avi_demuxer_t));
  demuxer->aid = aid;
  switch (AVI_audio_format(avi)) {
    case 0x0001: // raw PCM audio
      if (verbose)
        fprintf(stdout, "+-> Using PCM output module for audio stream %d.\n",
                aid + 1);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_bits(avi);
/*      demuxer->packetizer = new pcm_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample,
                                                 &async, &range, comments);*/
      break;
    case 0x0055: // MP3
      if (verbose)
        fprintf(stdout, "+-> Using MP3 output module for audio stream %d.\n",
                aid + 1);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_mp3rate(avi);
      demuxer->packetizer = new mp3_packetizer_c(avi->wave_format_ex[aid],
                                                 avi->wave_format_ex[aid] ?
                                                   sizeof(WAVEFORMATEX) : 0,
                                                 demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample,
                                                 &async, &range);
      break;
    case 0x2000: // AC3
      if (verbose)
        fprintf(stdout, "+-> Using AC3 output module for audio stream %d.\n",
                aid + 1);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_mp3rate(avi);
/*      demuxer->packetizer = new ac3_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample,
                                                 &async, &range, comments);*/
      break;
    default:
      fprintf(stderr, "Error: Unknown audio format 0x%04x for audio stream " \
              "%d.\n", AVI_audio_format(avi), aid + 1);
      return -1;
  }
  
  if (append_to == NULL)
    ademuxers = demuxer;
  else
    append_to->next = demuxer;

#ifdef DEBUG
  dump_wfe(avi->wave_format_ex[aid]);
#endif

  return 0;
}

int avi_reader_c::is_keyframe(unsigned char *data, long size, int suggestion) {
  int i;

  if (!rederive_keyframes)
    return suggestion;

  switch (is_divx) {
    case RAVI_DIVX3:
      i = *((int *)data);
      return ((i & 0x40000000) ? 0 : 1);
    case RAVI_MPEG4:
      for (i = 0; i < size - 5; i++) {
        if ((data[i] == 0x00) && (data[i + 1] == 0x00) && (data[i + 2] == 0x01) &&
            (data[i + 3] == 0xb6)) {
          if ((data[i + 4] & 0xc0) == 0x00)
            return 1;
          else
            return 0;
        }
      }

      return 0;
    default:
      return suggestion;
  }
}

int avi_reader_c::read() {
  int nread, key, last_frame;
  avi_demuxer_t *demuxer;
  int need_more_data;
  int done, frames_read;
  
  need_more_data = 0;
  if ((vpacketizer != NULL) && !video_done) {
/*    if (frames == 0)
      vpacketizer->set_chapter_info(chapter_info);*/
    last_frame = 0;
    while (!vpacketizer->packet_available() && !last_frame) {
      done = 0;
      
      // Make sure we have a frame to work with.
      if (old_chunk == NULL) {
        nread = AVI_read_frame(avi, (char *)chunk, &key);
        if (nread > max_frame_size) {
          fprintf(stderr, "FATAL: r_avi: nread (%d) > max_frame_size (%d)\n",
                  nread, max_frame_size);
          exit(1);
        }
        if (nread < 0) {
          frames = maxframes + 1;
          break;
        }
        key = is_keyframe((unsigned char *)chunk, nread, key);
        old_chunk = (char *)malloc(nread);
        if (old_chunk == NULL)
          die("malloc");
        memcpy(old_chunk, chunk, nread);
        old_key = key;
        old_nread = nread;
        frames++;
      }
      frames_read = 1;
      done = 0;
      // Check whether we have identical frames
      while (!done && (frames <= (maxframes - 1))) {
        nread = AVI_read_frame(avi, (char *)chunk, &key);
        if (nread > max_frame_size) {
          fprintf(stderr, "FATAL: r_avi: nread (%d) > max_frame_size (%d)\n",
                  nread, max_frame_size);
          exit(1);
        }
        if (nread < 0) {
          vpacketizer->process(old_chunk, old_nread, frames_read, old_key, 1);
          frames = maxframes + 1;
          break;
        }
        key = is_keyframe((unsigned char *)chunk, nread, key);
        if (frames == (maxframes - 1)) {
          last_frame = 1;
          done = 1;
        }
        if (nread == 0)
          frames_read++;
        else if (nread > 0)
          done = 1;
        frames++;
      }
      if (nread > 0) {
        vpacketizer->process(old_chunk, old_nread, frames_read, old_key, 0);
        if (! last_frame) {
          if (old_chunk != NULL)
            free(old_chunk);
          if (nread == 0) 
            fprintf(stdout, "hmm\n");
          old_chunk = (char *)malloc(nread);
          if (old_chunk == NULL)
            die("malloc");
          memcpy(old_chunk, chunk, nread);
          old_key = key;
          old_nread = nread;
        } else if (nread > 0)
          vpacketizer->process(chunk, nread, 1, key, 1);
      }
    }
    if (last_frame) {
      frames = maxframes + 1;
      video_done = 1;
    } else if (frames != (maxframes + 1))
      need_more_data = 1;
  }
  
  demuxer = ademuxers;
  while (demuxer != NULL) {
    while (!demuxer->eos && !demuxer->packetizer->packet_available()) {
      AVI_set_audio_track(avi, demuxer->aid);
      switch (AVI_audio_format(avi)) {
        case 0x0001: // raw PCM
          nread = AVI_read_audio(avi, chunk, demuxer->samples_per_second *
                                 demuxer->channels * demuxer->bits_per_sample /
                                 8);
          if (nread <= 0) {
            demuxer->eos = 1;
            *chunk = 1;
//            ((pcm_packetizer_c *)demuxer->packetizer)->process(chunk, 1, 1);
//            demuxer->packetizer->flush_pages();
          } /*else
            ((pcm_packetizer_c *)demuxer->packetizer)->process(chunk, nread, 0);*/
          break;
        case 0x0055: // MP3
          nread = AVI_read_audio(avi, chunk, 16384);
          if (nread <= 0)
            demuxer->eos = 1;
          else
            ((mp3_packetizer_c *)demuxer->packetizer)->process(chunk, nread, 0);
          
          break;
        case 0x2000: // AC3
          nread = AVI_read_audio(avi, chunk, 16384);
          if (nread <= 0)
            demuxer->eos = 1;
/*          else
            ((ac3_packetizer_c *)demuxer->packetizer)->process(chunk, nread, 0);
*/
          
          break;
      }
    }
    if (!demuxer->eos)
      need_more_data = 1;
    demuxer = demuxer->next;
  }
  
  if (need_more_data)
    return EMOREDATA;
  else 
    return 0;  
}

packet_t *avi_reader_c::get_packet() {
  generic_packetizer_c *winner;
  avi_demuxer_t        *demuxer;
  
  winner = NULL;
  
  if ((vpacketizer != NULL) && (vpacketizer->packet_available()))
    winner = vpacketizer;
  
  demuxer = ademuxers;
  while (demuxer != NULL) {
    if (winner == NULL) {
      if (demuxer->packetizer->packet_available())
        winner = demuxer->packetizer;
    } else if (winner->packet_available() &&
               (winner->get_smallest_timestamp() >
                demuxer->packetizer->get_smallest_timestamp()))
      winner = demuxer->packetizer;
    demuxer = demuxer->next;
  }
  
  if (winner != NULL)
    return winner->get_packet();
  else
    return NULL;
}

int avi_reader_c::display_priority() {
  if (vpacketizer != NULL)
    return DISPLAYPRIORITY_HIGH;
  else
    return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void avi_reader_c::display_progress() {
  if (vpacketizer != NULL) {
    int myframes = frames;
    if (frames == (maxframes + 1))
      myframes--;
    fprintf(stdout, "progress: %d/%ld frames (%ld%%)\r",
            myframes, AVI_video_frames(avi),
            myframes * 100 / AVI_video_frames(avi));
  } else {
    fprintf(stdout, "working... %c\r", wchar[act_wchar]);
    act_wchar++;
    if (act_wchar == strlen(wchar))
      act_wchar = 0;
  }
  fflush(stdout);
}

