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
    \version \$Id: r_avi.cpp,v 1.25 2003/05/05 18:37:36 mosu Exp $
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

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "queue.h"
#include "r_avi.h"
#include "p_video.h"
#include "p_pcm.h"
#include "p_mp3.h"
#include "p_ac3.h"

int avi_reader_c::probe_file(FILE *file, int64_t size) {
  unsigned char data[12];
  
  if (size < 12)
    return 0;
  fseek(file, 0, SEEK_SET);
  if (fread(data, 1, 12, file) != 12) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  if(strncasecmp((char *)data, "RIFF", 4) ||
     strncasecmp((char *)data+8, "AVI ", 4))
    return 0;
  return 1;
}

/*
 * allocates and initializes local storage for a particular
 * substream conversion.
 */
avi_reader_c::avi_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int fsize, i, extract_video = 1;
  int64_t size;
  FILE *f;
  avi_demuxer_t *demuxer;
  char *codec;

  if ((f = fopen(ti->fname, "r")) == NULL)
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
    fprintf(stdout, "Using AVI demultiplexer for %s. Opening file. This "
            "may take some time depending on the file's size.\n", ti->fname);
  rederive_keyframes = 0;
  if ((avi = AVI_open_input_file(ti->fname, 1)) == NULL) {
    const char *msg = "avi_reader: Could not initialize AVI source. Reason: ";
    char *s, *error;
    error = AVI_strerror();
    s = (char *)safemalloc(strlen(msg) + strlen(error) + 1);
    sprintf(s, "%s%s", msg, error);
    throw error_c(s);
  }

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

  if (ti->vtracks != NULL) {
    extract_video = 0;
    for (i = 0; i < strlen((char *)ti->vtracks); i++) {
      if (ti->vtracks[i] > 0)
        fprintf(stderr, "Warning: avi_reader: only one video stream per AVI " \
                "is supported. Will ignore -d %d.\n", ti->vtracks[i]);
      else if (ti->vtracks[i] == 1)
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

    ti->private_data = (unsigned char *)avi->bitmap_info_header;
    if (ti->private_data != NULL)
      ti->private_size = avi->bitmap_info_header->bi_size;
    if (ti->fourcc[0] == 0) {
      memcpy(ti->fourcc, codec, 4);
      ti->fourcc[4] = 0;
    } else
      memcpy(&avi->bitmap_info_header->bi_compression, ti->fourcc, 4);
    vpacketizer = new video_packetizer_c(AVI_frame_rate(avi),
                                         AVI_video_width(avi),
                                         AVI_video_height(avi),
                                         24, // fixme!
                                         1, ti);
    if (verbose)
      fprintf(stdout, "+-> Using video output module for video stream.\n");
  } else
    vpacketizer = NULL;

  ademuxers = NULL;
  if (ti->atracks != NULL) { // use only specific audio tracks or none at all
    for (i = 0; i < strlen((char *)ti->atracks); i++) {
      if (ti->atracks[i] >= AVI_audio_tracks(avi))
        fprintf(stderr, "Warning: avi_reader: the AVI does not contain an " \
                "audio stream with the id %d. Number of audio tracks: %d\n",
                ti->atracks[i], AVI_audio_tracks(avi));
      else {
        int already_extracting = 0;
        avi_demuxer_t *demuxer = ademuxers;
        
        while (demuxer) {
          if (demuxer->aid == ti->atracks[i]) {
            already_extracting = 1;
            break;
          }
          demuxer = demuxer->next;
        }
        if (already_extracting)
          fprintf(stderr, "Warning: avi_reader: already extracting audio " \
                  "stream number %d. Will only do this once.\n",
                  ti->atracks[i]);
        else
          add_audio_demuxer(avi, ti->atracks[i]);
      }
    }
  } else // use all audio tracks (no parameter specified)
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
  chunk = (unsigned char *)safemalloc(fsize < 16384 ? 16384 : fsize);
  act_wchar = 0;
  old_key = 0;
  old_chunk = NULL;
  video_done = 0;
}

avi_reader_c::~avi_reader_c() {
  struct avi_demuxer_t *demuxer, *tmp;
  
  if (avi != NULL)
    AVI_close(avi);
  if (chunk != NULL)
    safefree(chunk);
  if (vpacketizer != NULL)
    delete vpacketizer;

  demuxer = ademuxers;
  while (demuxer) {
    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;
    tmp = demuxer->next;
    safefree(demuxer);
    demuxer = tmp;
  }

  if (old_chunk != NULL)
    safefree(old_chunk);

  ti->private_data = NULL;
}

int avi_reader_c::add_audio_demuxer(avi_t *avi, int aid) {
  avi_demuxer_t *demuxer, *append_to;
  WAVEFORMATEX *wfe;
  
  append_to = ademuxers;
  while ((append_to != NULL) && (append_to->next != NULL))
    append_to = append_to->next;
  AVI_set_audio_track(avi, aid);
  demuxer = (avi_demuxer_t *)safemalloc(sizeof(avi_demuxer_t));
  memset(demuxer, 0, sizeof(avi_demuxer_t));
  demuxer->aid = aid;
  wfe = avi->wave_format_ex[aid];
  ti->private_data = (unsigned char *)wfe;
  if (wfe != NULL)
    ti->private_size = wfe->cb_size + sizeof(WAVEFORMATEX);
  else
    ti->private_size = 0;
  switch (AVI_audio_format(avi)) {
    case 0x0001: // raw PCM audio
      if (verbose)
        fprintf(stdout, "+-> Using PCM output module for audio stream %d.\n",
                aid);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_bits(avi);
      demuxer->packetizer = new pcm_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample, ti);
      break;
    case 0x0055: // MP3
      if (verbose)
        fprintf(stdout, "+-> Using MP3 output module for audio stream %d.\n",
                aid);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_mp3rate(avi);
      demuxer->packetizer = new mp3_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels, ti);
      break;
    case 0x2000: // AC3
      if (verbose)
        fprintf(stdout, "+-> Using AC3 output module for audio stream %d.\n",
                aid);
      demuxer->samples_per_second = AVI_audio_rate(avi);
      demuxer->channels = AVI_audio_channels(avi);
      demuxer->bits_per_sample = AVI_audio_mp3rate(avi);
      demuxer->packetizer = new ac3_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels, ti);
      break;
    default:
      fprintf(stderr, "Error: Unknown audio format 0x%04x for audio stream " \
              "%d.\n", AVI_audio_format(avi), aid);
      return -1;
  }
  
  if (append_to == NULL)
    ademuxers = demuxer;
  else
    append_to->next = demuxer;

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
        if ((data[i] == 0x00) && (data[i + 1] == 0x00) &&
            (data[i + 2] == 0x01) && (data[i + 3] == 0xb6)) {
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
  int done, frames_read, size;
  
  need_more_data = 0;
  if ((vpacketizer != NULL) && !video_done) {
    last_frame = 0;
    while (!vpacketizer->packet_available() && !last_frame) {
      done = 0;
      
      // Make sure we have a frame to work with.
      if (old_chunk == NULL) {
        nread = AVI_read_frame(avi, (char *)chunk, &key);
        if (nread < 0) {
          frames = maxframes + 1;
          break;
        }
        key = is_keyframe(chunk, nread, key);
        old_chunk = (unsigned char *)safememdup(chunk, nread);
        old_key = key;
        old_nread = nread;
        frames++;
      }
      frames_read = 1;
      done = 0;
      // Check whether we have identical frames
      while (!done && (frames <= (maxframes - 1))) {
        nread = AVI_read_frame(avi, (char *)chunk, &key);
        if (nread < 0) {
          vpacketizer->process(old_chunk, old_nread, -1, -1,
                               old_key ? -1 : 0);
          frames = maxframes + 1;
          break;
        }
        key = is_keyframe(chunk, nread, key);
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
        vpacketizer->process(old_chunk, old_nread, -1, -1,
                             old_key ? -1 : 0);
        if (! last_frame) {
          if (old_chunk != NULL)
            safefree(old_chunk);
          if (nread == 0) 
            fprintf(stdout, "hmm\n");
          old_chunk = (unsigned char *)safememdup(chunk, nread);
          old_key = key;
          old_nread = nread;
        } else if (nread > 0)
          vpacketizer->process(chunk, nread, -1, -1,
                               key ? -1 : 0);
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
      if (AVI_audio_format(avi) == 0x0001)
        size = demuxer->channels * demuxer->bits_per_sample *
          demuxer->samples_per_second / 8;
      else
        size = 16384;

      nread = AVI_read_audio(avi, (char *)chunk, size);
      if (nread > 0) {
        if (nread < size)
          demuxer->eos = 1;
        else
          demuxer->eos = 0;
        demuxer->packetizer->process(chunk, nread);
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
  avi_demuxer_t *demuxer;
  
  winner = NULL;
  
  if ((vpacketizer != NULL) && (vpacketizer->packet_available()))
    winner = vpacketizer;
  
  demuxer = ademuxers;
  while (demuxer != NULL) {
    if (winner == NULL) {
      if (demuxer->packetizer->packet_available())
        winner = demuxer->packetizer;
    } else if (winner->packet_available() &&
               (winner->get_smallest_timecode() >
                demuxer->packetizer->get_smallest_timecode()))
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

void avi_reader_c::set_headers() {
  avi_demuxer_t *demuxer;
  
  if (vpacketizer != NULL)
    vpacketizer->set_headers();
  
  demuxer = ademuxers;
  while (demuxer != NULL) {
    demuxer->packetizer->set_headers();
    demuxer = demuxer->next;
  }
}
