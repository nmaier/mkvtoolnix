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
    \version $Id$
    \brief AVI demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

// {{{ includes

#include "os.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_AVICLASSES
#include "common_mmreg.h"
#include "common_gdivfw.h"
#include "AVIReadHandler.h"
#else
extern "C" {
#include <avilib.h>
}
#endif

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_avi.h"
#include "p_video.h"
#include "p_pcm.h"
#include "p_mp3.h"
#include "p_ac3.h"

#define PFX "avi_reader: "
#define MIN_CHUNK_SIZE 128000

// }}}

// {{{ FUNCTION avi_reader_c::probe_file

int avi_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  unsigned char data[12];

  if (size < 12)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(data, 12) != 12)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if(strncasecmp((char *)data, "RIFF", 4) ||
     strncasecmp((char *)data+8, "AVI ", 4))
    return 0;
  return 1;
}

// }}}

// {{{ C'TOR

avi_reader_c::avi_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  long fsize, i;
  int64_t size, bps;
  avi_demuxer_t *demuxer;
#ifdef HAVE_AVICLASSES
  w32AVISTREAMINFO stream_info;
  long format_size;
  w32BITMAPINFOHEADER *bih;
  char codec[5];
#else
  char *codec;
#endif

  try {
    io = new mm_io_c(ti->fname, MODE_READ);
    size = io->get_size();
    if (!avi_reader_c::probe_file(io, size))
      throw error_c(PFX "Source is not a valid AVI file.");
  } catch (exception &ex) {
    throw error_c(PFX "Could not read the source file.");
  }

  if (verbose)
    mxinfo("Using AVI demultiplexer for %s. Opening file. This "
           "may take some time depending on the file's size.\n", ti->fname);

  fsize = 0;
  rederive_keyframes = 0;

#ifdef HAVE_AVICLASSES
  avi = CreateAVIReadHandler(io);
  if (avi == NULL)
    throw error_c(PFX "Could not initialize the AVI handler.");
  s_video = avi->GetStream(streamtypeVIDEO, 0);
  if (s_video == NULL)
    throw error_c(PFX "No video stream found in file.");

  mxverb(2, "1: 0x%08x\n", s_video);
  if (s_video->Info(&stream_info, sizeof(stream_info)) != S_OK)
    throw error_c(PFX "Could not get the video stream information.");
  s_video->FormatSize(0, &format_size);
  mxverb(2, "2: format_size %ld\n", format_size);
  bih = (w32BITMAPINFOHEADER *)safemalloc(format_size);
  if (s_video->ReadFormat(0, bih, &format_size) != S_OK)
    throw error_c(PFX "Could not read the video format information.");

  frames = s_video->Start();
  maxframes = s_video->End();
  mxverb(2, "3: maxframes %ld\n", maxframes);
  fps = (float)stream_info.dwRate / (float)stream_info.dwScale;
  mxverb(2, "4: fps %.3f\n", fps);
  max_frame_size = 0;
  for (i = 0; i < maxframes; i++) {
    if (s_video->Read(i, 1, NULL, 0, &fsize, NULL) != S_OK)
      break;
    if (fsize > max_frame_size)
      max_frame_size = fsize;
  }
  mxverb(2, "5: max_frame_size %d\n", max_frame_size);

  if (demuxing_requested('v', 0)) {
    memcpy(codec, &bih->biCompression, 4);
    codec[4] = 0;
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

    ti->private_data = (unsigned char *)bih;
    if (ti->private_data != NULL)
      ti->private_size = get_uint32(&bih->biSize);
    if (ti->fourcc[0] == 0) {
      memcpy(ti->fourcc, codec, 4);
      ti->fourcc[4] = 0;
    } else
      memcpy(&bih->biCompression, ti->fourcc, 4);
    ti->id = 0;                 // ID for the video track.
    vpacketizer = new video_packetizer_c(this, NULL, fps,
                                         get_uint32(&bih->biWidth),
                                         get_uint32(&bih->biHeight),
                                         false, ti);
    if (verbose)
      mxinfo("+-> Using video output module for video track ID 0.\n");
    mxverb(2, "6: width %u, height %u\n", get_uint32(&bih->biWidth),
           get_uint32(&bih->biHeight));
  } else
    vpacketizer = NULL;

  safefree(bih);

  i = 0;
  while (true) {
    if (avi->GetStream(streamtypeAUDIO, i) == NULL)
      break;
    if (demuxing_requested('a', i + 1))
      add_audio_demuxer(i);
    i++;
  }

  for (i = 0; i < ademuxers.size(); i++) {
    demuxer = ademuxers[i];
    bps = demuxer->samples_per_second * demuxer->channels *
      demuxer->bits_per_sample / 8;
    if (bps > max_frame_size)
      max_frame_size = bps;
  }

#else
  frames = 0;
  delete io;
  if ((avi = AVI_open_input_file(ti->fname, 1)) == NULL) {
    const char *msg = PFX "Could not initialize AVI source. Reason: ";
    char *s, *error;
    error = AVI_strerror();
    s = (char *)safemalloc(strlen(msg) + strlen(error) + 1);
    mxprints(s, "%s%s", msg, error);
    throw error_c(s, true);
  }

  fps = AVI_frame_rate(avi);
  maxframes = AVI_video_frames(avi);
  for (i = 0; i < maxframes; i++)
    if (AVI_frame_size(avi, i) > fsize)
      fsize = AVI_frame_size(avi, i);
  max_frame_size = fsize;

  if (demuxing_requested('v', 0)) {
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
      ti->private_size = get_uint32(&avi->bitmap_info_header->bi_size);
    if (ti->fourcc[0] == 0) {
      memcpy(ti->fourcc, codec, 4);
      ti->fourcc[4] = 0;
    } else
      memcpy(&avi->bitmap_info_header->bi_compression, ti->fourcc, 4);
    ti->id = 0;                 // ID for the video track.
    vpacketizer = new video_packetizer_c(this, NULL, AVI_frame_rate(avi),
                                         AVI_video_width(avi),
                                         AVI_video_height(avi), false, ti);
    if (verbose)
      mxinfo("+-> Using video output module for video track ID 0.\n");
  } else
    vpacketizer = NULL;

  for (i = 0; i < AVI_audio_tracks(avi); i++)
    if (demuxing_requested('a', i + 1))
      add_audio_demuxer(i);

  for (i = 0; i < ademuxers.size(); i++) {
    demuxer = ademuxers[i];
    bps = demuxer->samples_per_second * demuxer->channels *
      demuxer->bits_per_sample / 8;
    if (bps > fsize)
      fsize = bps;
  }
#endif

  if (video_fps < 0)
    video_fps = fps;
  chunk = (unsigned char *)safemalloc(max_frame_size < MIN_CHUNK_SIZE ?
                                      MIN_CHUNK_SIZE : max_frame_size);
  act_wchar = 0;
  old_key = 0;
  old_chunk = NULL;
  video_done = 0;
  dropped_frames = 0;
}

// }}}

// {{{ D'TOR

avi_reader_c::~avi_reader_c() {
  avi_demuxer_t *demuxer;
  int i;

#ifdef HAVE_AVICLASSES
  avi->Release();
  delete io;
#else
  if (avi != NULL)
    AVI_close(avi);
#endif
  if (chunk != NULL)
    safefree(chunk);
  if (vpacketizer != NULL)
    delete vpacketizer;

  for (i = 0; i < ademuxers.size(); i++) {
    demuxer = ademuxers[i];
    if (demuxer->packetizer != NULL)
      delete demuxer->packetizer;
    safefree(demuxer);
  }
  ademuxers.clear();

  if (old_chunk != NULL)
    safefree(old_chunk);

  ti->private_data = NULL;

  if (verbose > 1)
    mxinfo("avi_reader_c: Dropped video frames: %d\n", dropped_frames);
}

// }}}

// {{{ FUNCTION avi_reader_c::add_audio_demuxer

void avi_reader_c::add_audio_demuxer(int aid) {
  avi_demuxer_t *demuxer;
  int i;
#ifdef HAVE_AVICLASSES
  w32AVISTREAMINFO stream_info;
  long format_size;


//   w32WAVEFORMATEX *wfe;
  w32WAVEFORMATEX *wfe;
  IAVIReadStream *stream;
#else
  alWAVEFORMATEX *wfe;
#endif
  uint32_t audio_format;

  for (i = 0; i < ademuxers.size(); i++)
    if (ademuxers[i]->aid == aid) // Demuxer already added?
      return;

  demuxer = (avi_demuxer_t *)safemalloc(sizeof(avi_demuxer_t));
  memset(demuxer, 0, sizeof(avi_demuxer_t));
  demuxer->aid = aid;
  ti->id = aid + 1;             // ID for this audio track.

#ifdef HAVE_AVICLASSES
  stream = avi->GetStream(streamtypeAUDIO, aid);
  if (stream == NULL)
    die(PFX "stream == NULL in add_audio_demuxer. Should not have happened. "
        "Please file a bug report.\n");
  demuxer->stream = stream;
  if (stream->Info(&stream_info, sizeof(w32AVISTREAMINFO)) != S_OK)
    mxerror(PFX "Could not read the stream information header for audio track "
            "%d.\n", aid + 1);
  stream->FormatSize(0, &format_size);
  wfe = (w32WAVEFORMATEX *)safemalloc(format_size);
  if (stream->ReadFormat(0, wfe, &format_size) != S_OK)
    mxerror(PFX "Could not read the format information header for audio track "
            "%d.\n", aid + 1);
  audio_format = wfe->wFormatTag;
  demuxer->samples_per_second = wfe->nSamplesPerSec;
  demuxer->channels = wfe->nChannels;
  demuxer->bits_per_sample = wfe->wBitsPerSample;
  demuxer->frame = stream->Start();
  demuxer->maxframes = stream->End();
  mxverb(2, "7: sizeof(w32WAVE...) %d, sps %d, c %d, bps %d, mf %d\n",
         sizeof(w32WAVEFORMATEX), demuxer->samples_per_second,
         demuxer->channels, demuxer->bits_per_sample, demuxer->maxframes);
  if (wfe->cbSize > 0) {
    ti->private_data = (unsigned char *)(wfe + 1);
    ti->private_size = wfe->cbSize;
  } else {
    ti->private_data = NULL;
    ti->private_size = 0;
  }
#else
  wfe = avi->wave_format_ex[aid];
  AVI_set_audio_track(avi, aid);
  audio_format = AVI_audio_format(avi);
  demuxer->samples_per_second = AVI_audio_rate(avi);
  demuxer->channels = AVI_audio_channels(avi);
  demuxer->bits_per_sample = AVI_audio_bits(avi);

  if (wfe->cb_size > 0) {
    ti->private_data = (unsigned char *)(wfe + 1);
    ti->private_size = wfe->cb_size;
  } else {
    ti->private_data = NULL;
    ti->private_size = 0;
  }
#endif

  switch(audio_format) {
    case 0x0001: // raw PCM audio
      if (verbose)
        mxinfo("+-> Using PCM output module for audio track ID %d.\n",
               aid + 1);
      demuxer->packetizer = new pcm_packetizer_c(this,
                                                 demuxer->samples_per_second,
                                                 demuxer->channels,
                                                 demuxer->bits_per_sample, ti);
      break;
    case 0x0055: // MP3
      if (verbose)
        mxinfo("+-> Using MPEG audio output module for audio track ID %d.\n",
               aid + 1);
      demuxer->packetizer = new mp3_packetizer_c(this,
                                                 demuxer->samples_per_second,
                                                 demuxer->channels, 3, ti);
      break;
    case 0x2000: // AC3
      if (verbose)
        mxinfo("+-> Using AC3 output module for audio track ID %d.\n",
               aid + 1);
      demuxer->packetizer = new ac3_packetizer_c(this,
                                                 demuxer->samples_per_second,
                                                 demuxer->channels, 0, ti);
      break;
    default:
      mxerror("Unknown audio format 0x%04x for audio track ID %d.\n",
              audio_format, aid + 1);
      return;
  }

#ifdef HAVE_AVICLASSES
  safefree(wfe);
#endif

  ademuxers.push_back(demuxer);
}

// }}}

// {{{ FUNCTION avi_reader_c::is_keyframe

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

// }}}

// {{{ FUNCTION avi_reader_c::read

int avi_reader_c::read(generic_packetizer_c *ptzr) {
  int key, last_frame, i, frames_read;
  long nread;
  avi_demuxer_t *demuxer;
  bool need_more_data, done;
  int64_t duration;
#ifdef HAVE_AVICLASSES
  long blread;
  int result;
#else
  int size;
#endif

  if ((vpacketizer != NULL) && !video_done && (vpacketizer == ptzr)) {
    debug_enter("avi_reader_c::read (video)");

    need_more_data = false;
    last_frame = 0;
    done = false;

    // Make sure we have a frame to work with.
    if (old_chunk == NULL) {
#ifdef HAVE_AVICLASSES
      debug_enter("aviclasses->Read() video");
      key = s_video->IsKeyFrame(frames);
      if (fast_mode) {
        if (s_video->Read(frames, 1, NULL, 0, &nread, &blread) != S_OK)
          nread = -1;
      } else if (s_video->Read(frames, 1, chunk, 0x7FFFFFFF, &nread,
                               &blread) != S_OK)
        nread = -1;
      debug_leave("aviclasses->Read() video");
#else
      debug_enter("AVI_read_frame");
      if (fast_mode)
        nread = AVI_read_frame(avi, NULL, &key);
      else
        nread = AVI_read_frame(avi, (char *)chunk, &key);
      debug_leave("AVI_read_frame");
#endif
      if (nread < 0) {
        frames = maxframes + 1;
        done = true;
      } else {
//         key = is_keyframe(chunk, nread, key);
        if (fast_mode)
          old_chunk = (unsigned char *)safemalloc(nread);
        else
          old_chunk = (unsigned char *)safememdup(chunk, nread);
        old_key = key;
        old_nread = nread;
        frames++;
      }
    }
    if (!done) {
      frames_read = 1;
      // Check whether we have identical frames
      while (!done && (frames <= (maxframes - 1))) {
#ifdef HAVE_AVICLASSES
        debug_enter("aviclasses->Read() video");
        key = s_video->IsKeyFrame(frames);
        if (fast_mode) {
          if (s_video->Read(frames, 1, NULL, 0, &nread, &blread) != S_OK)
            nread = -1;
        } else if (s_video->Read(frames, 1, chunk, 0x7FFFFFFF, &nread,
                                 &blread) != S_OK)
          nread = -1;
        debug_leave("aviclasses->Read() video");
#else
        debug_enter("AVI_read_frame");
        if (fast_mode)
          nread = AVI_read_frame(avi, NULL, &key);
        else
          nread = AVI_read_frame(avi, (char *)chunk, &key);
        debug_leave("AVI_read_frame");
#endif
        if (nread < 0) {
          vpacketizer->process(old_chunk, old_nread, -1, -1,
                               old_key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC);
          mxwarn(PFX "Reading frame number %d resulted in an error. "
                 "Aborting this track.\n", frames);
          frames = maxframes + 1;
          break;
        }
//         key = is_keyframe(chunk, nread, key);
        if (frames == (maxframes - 1)) {
          last_frame = 1;
          done = true;
        }
        if (nread == 0) {
          frames_read++;
          dropped_frames++;
        }
        else if (nread > 0)
          done = true;
        frames++;
      }
      duration = (int64_t)(1000.0 * frames_read / fps);
      if (nread > 0) {
        vpacketizer->process(old_chunk, old_nread, -1, duration,
                             old_key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC);
        if (! last_frame) {
          if (old_chunk != NULL)
            safefree(old_chunk);
          if (fast_mode)
            old_chunk = (unsigned char *)safemalloc(nread);
          else
            old_chunk = (unsigned char *)safememdup(chunk, nread);
          old_key = key;
          old_nread = nread;
        } else if (nread > 0) {
          vpacketizer->process(chunk, nread, -1, duration,
                               key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC);
        }
      }
    }

    if (last_frame) {
      frames = maxframes + 1;
      video_done = 1;
    } else if (frames != (maxframes + 1))
      need_more_data = true;

    debug_leave("avi_reader_c::read (video)");

    if (need_more_data)
      return EMOREDATA;
    else
      return 0;
  }

  for (i = 0; i < ademuxers.size(); i++) {
    demuxer = ademuxers[i];

    if (demuxer->packetizer != ptzr)
      continue;

    debug_enter("avi_reader_c::read (audio)");

    need_more_data = false;
#ifdef HAVE_AVICLASSES
    if (demuxer->frame < demuxer->maxframes) {
      debug_enter("aviclasses->Read() audio");
      result = demuxer->stream->Read(demuxer->frame, AVISTREAMREAD_CONVENIENT,
                                     chunk, max_frame_size, &nread, &blread);
      debug_leave("aviclasses->Read() audio");
      demuxer->frame += blread;
      if (result == S_OK) {
        demuxer->packetizer->process(chunk, nread);
        need_more_data = demuxer->frame < demuxer->maxframes;
      } else
        demuxer->frame = demuxer->maxframes;
    }
#else
    AVI_set_audio_track(avi, demuxer->aid);
    if (AVI_audio_format(avi) == 0x0001)
      size = demuxer->channels * demuxer->bits_per_sample *
        demuxer->samples_per_second / 8;
    else
      size = MIN_CHUNK_SIZE;

    debug_enter("AVI_read_audio");
    nread = AVI_read_audio(avi, (char *)chunk, size);
    debug_leave("AVI_read_audio");

    if (nread > 0) {
      if (nread >= size)
        need_more_data = true;
      demuxer->packetizer->process(chunk, nread);
    }
#endif

    debug_leave("avi_reader_c::read (audio)");

    if (need_more_data)
      return EMOREDATA;
    else
      return 0;
  }

  return 0;
}

// }}}

// {{{ FUNCTIONS avi_reader_c::display_priority/_progess

int avi_reader_c::display_priority() {
  if (vpacketizer != NULL)
    return DISPLAYPRIORITY_HIGH;
  else
    return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void avi_reader_c::display_progress(bool final) {
  int myframes;

  if (vpacketizer != NULL) {
    myframes = frames;
    if (frames == (maxframes + 1))
      myframes--;
    if (final)
      mxinfo("progress: %ld/%ld frames (100%%)\r", maxframes, maxframes);
    else
      mxinfo("progress: %d/%ld frames (%ld%%)\r", myframes, maxframes,
             myframes * 100 / maxframes);
  } else {
    mxinfo("Working... %c\r", wchar[act_wchar]);
    act_wchar++;
    if (act_wchar == strlen(wchar))
      act_wchar = 0;
  }
}

// }}}

// {{{ FUNCTION avi_reader_c::set_headers

void avi_reader_c::set_headers() {
  int i;

  if (vpacketizer != NULL)
    vpacketizer->set_headers();

  for (i = 0; i < ademuxers.size(); i++)
    ademuxers[i]->packetizer->set_headers();
}

// }}}

// {{{ FUNCTION avi_reader_c::identify

void avi_reader_c::identify() {
  int i;
  const char *type;
#ifdef HAVE_AVICLASSES
  w32AVISTREAMINFO stream_info;
  w32BITMAPINFOHEADER *bih;
  w32WAVEFORMATEX *wfe;
  IAVIReadStream *stream;
  long format_size;
  char codec[5];
#endif
  
  mxinfo("File '%s': container: AVI\n", ti->fname);
#ifdef HAVE_AVICLASSES
  stream = avi->GetStream(streamtypeVIDEO, 0);
  if (stream->Info(&stream_info, sizeof(stream_info)) != S_OK)
    mxerror(PFX "Could not get the video stream information.");
  stream->FormatSize(0, &format_size);
  mxverb(2, "2: format_size %ld\n", format_size);
  bih = (w32BITMAPINFOHEADER *)safemalloc(format_size);
  if (stream->ReadFormat(0, bih, &format_size) != S_OK)
    mxerror(PFX "Could not read the video format information.");
  memcpy(codec, &bih->biCompression, 4);
  safefree(bih);
  codec[4] = 0;
  mxinfo("Track ID 0: video (%s)\n", codec);

  i = 0;
  while (true) {
    stream = avi->GetStream(streamtypeAUDIO, i);
    if (stream == NULL)
      break;
    if (stream->Info(&stream_info, sizeof(w32AVISTREAMINFO)) != S_OK)
      mxerror(PFX "Could not read the stream information header for audio "
              "track %d.\n", i + 1);
    stream->FormatSize(0, &format_size);
    wfe = (w32WAVEFORMATEX *)safemalloc(format_size);
    if (stream->ReadFormat(0, wfe, &format_size) != S_OK)
      mxerror(PFX "Could not read the format information header for audio "
              "track %d.\n", i + 1);
    switch (wfe->wFormatTag) {
      case 0x0001:
        type = "PCM";
        break;
      case 0x0055:
        type = "MP3";
        break;
      case 0x2000:
        type = "AC3";
        break;
      default:
        type = "unknown";
    }
    mxinfo("Track ID %d: audio (%s)\n", i + 1, type);
    safefree(wfe);
    i++;
  }
#else
  mxinfo("Track ID 0: video (%s)\n", AVI_video_compressor(avi));
  for (i = 0; i < AVI_audio_tracks(avi); i++) {
    AVI_set_audio_track(avi, i);
    switch (AVI_audio_format(avi)) {
      case 0x0001:
        type = "PCM";
        break;
      case 0x0055:
        type = "MP3";
        break;
      case 0x2000:
        type = "AC3";
        break;
      default:
        type = "unknown";
    }
    mxinfo("Track ID %d: audio (%s)\n", i + 1, type);
  }
#endif
}

// }}}
