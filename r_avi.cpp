/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h
  AVI demultiplexer module

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <avilib.h>
}

#include "common.h"
#include "mkvmerge.h"
//#include "queue.h"
#include "r_avi.h"
//#include "p_video.h"
//#include "p_pcm.h"
//#include "p_mp3.h"
//#include "p_ac3.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

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
/*avi_reader_c::avi_reader_c(char *fname, unsigned char *astreams,
                           unsigned char *vstreams, audio_sync_t *nasync,
                           range_t *nrange, char **ncomments, char *nfourcc)
                           throw (error_c) {*/
avi_reader_c::avi_reader_c(char *fname) throw (error_c) {
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
  
/*  if (astreams != NULL)
    this->astreams = (unsigned char *)strdup((char *)astreams);
  else
    this->astreams = NULL;
  if (vstreams != NULL)
    this->vstreams = (unsigned char *)strdup((char *)vstreams);
  else
    this->vstreams = NULL;

  if (ncomments == NULL)
    comments = ncomments;
  else
    comments = dup_comments(ncomments);*/
    
  fps = AVI_frame_rate(avi);
/*  if (video_fps < 0)
    video_fps = fps;*/
  frames = 0;
  fsize = 0;
  maxframes = AVI_video_frames(avi);
  for (i = 0; i < maxframes; i++)
    if (AVI_frame_size(avi, i) > fsize)
      fsize = AVI_frame_size(avi, i);
  max_frame_size = fsize;

/*  if (vstreams != NULL) {
    extract_video = 0;
    for (i = 0; i < strlen((char *)vstreams); i++) {
      if (vstreams[i] > 1)
        fprintf(stderr, "Warning: avi_reader: only one video stream per AVI " \
                "is supported. Will not ignore -d %d.\n", vstreams[i]);
      else if (vstreams[i] == 1)
        extract_video = 1;
    }
  }
  if (extract_video) {*/
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

/*    if (nfourcc != NULL)
      codec = nfourcc;*/
/*    vpacketizer = new video_packetizer_c(codec, AVI_frame_rate(avi),
                                         AVI_video_width(avi),
                                         AVI_video_height(avi),
                                         24, // fixme!
                                         fsize, NULL, nrange, comments);*/
    if (verbose)
      fprintf(stderr, "+-> Using video output module for video stream.\n");
/*  } else
    vpacketizer = NULL;

  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));*/
  ademuxers = NULL;
/*  if (astreams != NULL) { // use only specific audio streams (or none at all)
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
  chunk = (char *)malloc(fsize);
  if (chunk == NULL)
    die("malloc");
  act_wchar = 0;
  old_key = 0;
  old_chunk = NULL;
  video_done = 0;
}

avi_reader_c::~avi_reader_c() {
  struct avi_demuxer_t *demuxer, *tmp;
  
/*  if (astreams != NULL)
    free(astreams);
  if (vstreams != NULL)
    free(vstreams);*/
  if (avi != NULL)
    AVI_close(avi);
  if (chunk != NULL)
    free(chunk);
/*  if (vpacketizer != NULL)
    delete vpacketizer;*/
  demuxer = ademuxers;
  while (demuxer) {
/*    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;*/
    tmp = demuxer->next;
    free(demuxer);
    demuxer = tmp;
  }
/*  if (comments != NULL)
    free_comments(comments);*/
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
/*      demuxer->packetizer = new mp3_packetizer_c(demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample,
                                                 &async, &range, comments);*/
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
  if (/*(vpacketizer != NULL) &&*/ !video_done) {
/*    if (frames == 0)
      vpacketizer->set_chapter_info(chapter_info);*/
    last_frame = 0;
    while (/*!vpacketizer->page_available() &&*/ !last_frame) {
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
//          vpacketizer->flush_pages();
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
//          vpacketizer->process(old_chunk, old_nread, frames_read, old_key, 1);
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
//        vpacketizer->process(old_chunk, old_nread, frames_read, old_key, 0);
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
;//          vpacketizer->process(chunk, nread, 1, key, 1);
      }
    }
    if (last_frame) {
//      vpacketizer->flush_pages();
      frames = maxframes + 1;
      video_done = 1;
    } else if (frames != (maxframes + 1))
      need_more_data = 1;
  }
  
  demuxer = ademuxers;
  while (demuxer != NULL) {
    while (!demuxer->eos /*&& !demuxer->packetizer->page_available()*/) {
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
          nread = AVI_read_audio_chunk(avi, NULL);
          if (nread > max_frame_size) {
            chunk = (char *)realloc(chunk, max_frame_size);
            max_frame_size = nread;
          }
          nread = AVI_read_audio_chunk(avi, chunk);
          if (nread <= 0) {
            demuxer->eos = 1;
/*            demuxer->packetizer->produce_eos_packet();
            demuxer->packetizer->flush_pages();*/
          } /*else
            ((mp3_packetizer_c *)demuxer->packetizer)->process(chunk, nread, 0);*/
          
          break;
        case 0x2000: // AC3
          nread = AVI_read_audio_chunk(avi, NULL);
          if (nread > max_frame_size) {
            chunk = (char *)realloc(chunk, max_frame_size);
            max_frame_size = nread;
          }
          nread = AVI_read_audio_chunk(avi, chunk);
          if (nread <= 0) {
            demuxer->eos = 1;
/*            demuxer->packetizer->produce_eos_packet();
            demuxer->packetizer->flush_pages();*/
          } {/*else {
            ((ac3_packetizer_c *)demuxer->packetizer)->process(chunk, nread, 0);*/
            demuxer->bytes_processed += nread;
          }
          
          break;
      }
    }
    if (!demuxer->eos)
      need_more_data = 1;
    demuxer = demuxer->next;
  }
  
  if (need_more_data)
    return 1;
  else 
    return 0;  
}

/*int avi_reader_c::serial_in_use(int serial) {
  avi_demuxer_t *demuxer;
  
  if ((vpacketizer != NULL) && (vpacketizer->serial_in_use(serial)))
    return 1;
  demuxer = ademuxers;
  while (demuxer != NULL) {
    if (demuxer->packetizer->serial_in_use(serial))
      return 1;
    demuxer = demuxer->next;
  }
  return 0;
}

ogmmerge_page_t *avi_reader_c::get_header_page(int header_type) {
  ogmmerge_page_t *ompage = NULL;
  avi_demuxer_t   *demuxer;
  
  if (vpacketizer) {
    ompage = vpacketizer->get_header_page(header_type);
    if (ompage != NULL)
      return ompage;
  }
  
  demuxer = ademuxers;
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

ogmmerge_page_t *avi_reader_c::get_page() {
  generic_packetizer_c *winner;
  avi_demuxer_t        *demuxer;
  
  winner = NULL;
  
  if ((vpacketizer != NULL) && (vpacketizer->page_available()))
    winner = vpacketizer;
  
  demuxer = ademuxers;
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
}*/

int avi_reader_c::display_priority() {
//  if (vpacketizer != NULL)
    return DISPLAYPRIORITY_HIGH;
//  else
//    return DISPLAYPRIORITY_LOW;
}

/*void avi_reader_c::reset() {
  avi_demuxer_t *demuxer;
  
  if (vpacketizer != NULL)
    vpacketizer->reset();
  demuxer = ademuxers;
  while (demuxer != NULL) {
    demuxer->packetizer->reset();
    demuxer = demuxer->next;
  } 
}*/

static char wchar[] = "-\\|/-\\|/-";

void avi_reader_c::display_progress() {
//  if (vpacketizer != NULL) {
    int myframes = frames;
    if (frames == (maxframes + 1))
      myframes--;
    fprintf(stdout, "progress: %d/%ld frames (%ld%%)\r",
            myframes, AVI_video_frames(avi),
            myframes * 100 / AVI_video_frames(avi));
/*  } else {
    fprintf(stdout, "working... %c\r", wchar[act_wchar]);
    act_wchar++;
    if (act_wchar == strlen(wchar))
      act_wchar = 0;
  }*/
  fflush(stdout);
}

