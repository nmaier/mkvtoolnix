/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   AVI demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// {{{ includes

#include "os.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// This one goes out to haali ;)
#include <sys/types.h>

extern "C" {
#include <avilib.h>
}

#include "aac_common.h"
#include "common.h"
#include "error.h"
#include "mpeg4_common.h"
#include "output_control.h"
#include "r_avi.h"
#include "p_aac.h"
#include "p_ac3.h"
#include "p_dts.h"
#include "p_mp3.h"
#include "p_pcm.h"
#include "p_video.h"

#define PFX "avi_reader: "
#define MIN_CHUNK_SIZE 128000

// }}}

// {{{ FUNCTION avi_reader_c::probe_file

int
avi_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  unsigned char data[12];

  if (size < 12)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(data, 12) != 12)
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (strncasecmp((char *)data, "RIFF", 4) ||
      strncasecmp((char *)data+8, "AVI ", 4))
    return 0;
  return 1;
}

// }}}

// {{{ C'TOR

avi_reader_c::avi_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  long fsize, i;
  int64_t size;

  try {
    io = new mm_file_io_c(ti.fname);
    size = io->get_size();
    if (!avi_reader_c::probe_file(io, size))
      throw error_c(PFX "Source is not a valid AVI file.");
  } catch (...) {
    throw error_c(PFX "Could not read the source file.");
  }

  if (verbose)
    mxinfo(FMT_FN "Using the AVI demultiplexer. Opening file. This "
           "may take some time depending on the file's size.\n",
           ti.fname.c_str());

  fsize = 0;
  rederive_keyframes = 0;
  vptzr = -1;

  frames = 0;
  delete io;
  if ((avi = AVI_open_input_file(ti.fname.c_str(), 1)) == NULL) {
    const char *msg = PFX "Could not initialize AVI source. Reason: ";
    throw error_c(mxsprintf("%s%s", msg, AVI_strerror()));
  }

  fps = AVI_frame_rate(avi);
  maxframes = AVI_video_frames(avi);
  for (i = 0; i < maxframes; i++)
    if (AVI_frame_size(avi, i) > fsize)
      fsize = AVI_frame_size(avi, i);
  max_frame_size = fsize;

  if (video_fps < 0)
    video_fps = fps;
  chunk_size = max_frame_size < MIN_CHUNK_SIZE ? MIN_CHUNK_SIZE :
    max_frame_size;
  chunk = (unsigned char *)safemalloc(chunk_size);
  act_wchar = 0;
  old_key = 0;
  old_chunk = NULL;
  video_done = 0;
  dropped_frames = 0;
  bytes_to_process = 0;
  bytes_processed = 0;
}

// }}}

// {{{ D'TOR

avi_reader_c::~avi_reader_c() {
  if (avi != NULL)
    AVI_close(avi);

  safefree(chunk);
  safefree(old_chunk);
  ti.private_data = NULL;

  mxverb(2, "avi_reader_c: Dropped video frames: %d\n", dropped_frames);
}

// }}}

void
avi_reader_c::create_packetizer(int64_t tid) {
  char *codec;

  if ((tid == 0) && demuxing_requested('v', 0) && (vptzr == -1)) {
    int i, maxframes;

    maxframes = AVI_video_frames(avi);
    for (i = 0; i < maxframes; i++)
      bytes_to_process += AVI_frame_size(avi, i);

    codec = AVI_video_compressor(avi);
    if (!strcasecmp(codec, "DIV3") ||
        !strcasecmp(codec, "AP41") || // Angel Potion
        !strcasecmp(codec, "MPG3") ||
        !strcasecmp(codec, "MP43"))
      is_divx = RAVI_DIVX3;
    else if (is_mpeg4_p2_fourcc(codec))
      is_divx = RAVI_MPEG4;
    else
      is_divx = 0;

    ti.private_data = (unsigned char *)avi->bitmap_info_header;
    if (ti.private_data != NULL)
      ti.private_size = get_uint32_le(&avi->bitmap_info_header->bi_size);
    ti.id = 0;                 // ID for the video track.
    if (is_divx == RAVI_MPEG4) {
      vptzr =
        add_packetizer(new mpeg4_p2_video_packetizer_c(this,
                                                       AVI_frame_rate(avi),
                                                       AVI_video_width(avi),
                                                       AVI_video_height(avi),
                                                       false,
                                                       ti));
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG-4 part 2 video output module.\n",
               ti.fname.c_str(), (int64_t)0);
    } else {
      vptzr = add_packetizer(new video_packetizer_c(this, NULL,
                                                    AVI_frame_rate(avi),
                                                    AVI_video_width(avi),
                                                    AVI_video_height(avi),
                                                    ti));
      if (verbose)
        mxinfo(FMT_TID "Using the video output module.\n",
               ti.fname.c_str(), (int64_t)0);
    }
  }
  if (tid == 0)
    return;

  if ((tid <= AVI_audio_tracks(avi)) && demuxing_requested('a', tid))
    add_audio_demuxer(tid - 1);
}

void
avi_reader_c::create_packetizers() {
  uint32_t i, bps;
  vector<avi_demuxer_t>::const_iterator demuxer;

  create_packetizer(0);

  for (i = 0; i < AVI_audio_tracks(avi); i++)
    create_packetizer(i + 1);

  foreach(demuxer, ademuxers) {
    bps = demuxer->samples_per_second * demuxer->channels *
      demuxer->bits_per_sample / 8;
    if (bps > chunk_size) {
      chunk_size = bps;
      chunk = (unsigned char *)saferealloc(chunk, chunk_size);
    }
  }
}

// {{{ FUNCTION avi_reader_c::add_audio_demuxer

void
avi_reader_c::add_audio_demuxer(int aid) {
  generic_packetizer_c *packetizer;
  vector<avi_demuxer_t>::const_iterator it;
  avi_demuxer_t demuxer;
  alWAVEFORMATEX *wfe;
  uint32_t audio_format;
  int maxchunks, i;

  foreach(it, ademuxers)
    if (it->aid == aid) // Demuxer already added?
      return;

  memset(&demuxer, 0, sizeof(avi_demuxer_t));
  demuxer.aid = aid;
  demuxer.ptzr = -1;
  ti.id = aid + 1;             // ID for this audio track.
  packetizer = NULL;

  wfe = avi->wave_format_ex[aid];
  AVI_set_audio_track(avi, aid);
  if (AVI_read_audio_chunk(avi, NULL) < 0) {
    mxwarn("Could not find an index for audio track %d (avilib error message: "
           "%s). Skipping track.\n", aid + 1, AVI_strerror());
    return;
  }
  audio_format = AVI_audio_format(avi);
  demuxer.samples_per_second = AVI_audio_rate(avi);
  demuxer.channels = AVI_audio_channels(avi);
  demuxer.bits_per_sample = AVI_audio_bits(avi);
  ti.avi_block_align = get_uint16_le(&wfe->n_block_align);
  ti.avi_avg_bytes_per_sec = get_uint32_le(&wfe->n_avg_bytes_per_sec);
  ti.avi_samples_per_chunk =
    get_uint32_le(&avi->stream_headers[aid].dw_scale);

  if (get_uint16_le(&wfe->cb_size) > 0) {
    ti.private_data = (unsigned char *)(wfe + 1);
    ti.private_size = get_uint16_le(&wfe->cb_size);
  } else {
    ti.private_data = NULL;
    ti.private_size = 0;
  }
  ti.avi_samples_per_sec = demuxer.samples_per_second;

  switch(audio_format) {
    case 0x0001: // raw PCM audio
      if (verbose)
        mxinfo(FMT_TID "Using the PCM output module.\n", ti.fname.c_str(),
               (int64_t)aid + 1);
      packetizer = new pcm_packetizer_c(this, demuxer.samples_per_second,
                                        demuxer.channels,
                                        demuxer.bits_per_sample, ti);
      break;
    case 0x0050: // MP2
    case 0x0055: // MP3
      if (verbose)
        mxinfo(FMT_TID "Using the MPEG audio output module.\n",
               ti.fname.c_str(), (int64_t)aid + 1);
      packetizer = new mp3_packetizer_c(this, demuxer.samples_per_second,
                                        demuxer.channels, false, ti);
      break;
    case 0x2000: // AC3
      if (verbose)
        mxinfo(FMT_TID "Using the AC3 output module.\n", ti.fname.c_str(),
               (int64_t)aid + 1);
      packetizer = new ac3_packetizer_c(this, demuxer.samples_per_second,
                                        demuxer.channels, 0, ti);
      break;
    case 0x2001: { // DTS
      dts_header_t dtsheader;

      dtsheader.core_sampling_frequency = demuxer.samples_per_second;
      dtsheader.audio_channels = demuxer.channels;
      if (verbose)
        mxinfo(FMT_TID "Using the DTS output module.\n", ti.fname.c_str(),
               (int64_t)aid + 1);
      packetizer = new dts_packetizer_c(this, dtsheader, ti, true);
      break;
    }
    case 0x00ff: { // AAC
      int profile, channels, sample_rate, output_sample_rate;
      bool is_sbr;

      if ((ti.private_size != 2) && (ti.private_size != 5))
        mxerror(FMT_TID "This AAC track does not contain valid headers. The "
                "extra header size is %d bytes, expected were 2 or 5 bytes.\n",
                ti.fname.c_str(), (int64_t)aid + 1, ti.private_size);
      if (!parse_aac_data(ti.private_data, ti.private_size, profile,
                          channels, sample_rate, output_sample_rate,
                          is_sbr))
        mxerror(FMT_TID "This AAC track does not contain valid headers. Could "
                "not parse the AAC information.\n", ti.fname.c_str(),
                (int64_t)aid + 1);
      if (is_sbr)
        profile = AAC_PROFILE_SBR;
      demuxer.samples_per_second = sample_rate;
      demuxer.channels = channels;
      if (verbose)
        mxinfo(FMT_TID "Using the AAC audio output module.\n",
               ti.fname.c_str(), (int64_t)aid + 1);
      packetizer = new aac_packetizer_c(this, AAC_ID_MPEG4, profile,
                                        demuxer.samples_per_second,
                                        demuxer.channels, ti, false, true);
      if (is_sbr)
        packetizer->set_audio_output_sampling_freq(output_sample_rate);
      break;
    }
    default:
      mxerror(FMT_TID "Unknown/unsupported audio format 0x%04x for this audio "
              "track.\n", ti.fname.c_str(), (int64_t)aid + 1, audio_format);
  }
  demuxer.ptzr = add_packetizer(packetizer);

  ademuxers.push_back(demuxer);

  maxchunks = AVI_audio_chunks(avi);
  for (i = 0; i < maxchunks; i++)
    bytes_to_process += AVI_audio_size(avi, i);
}

// }}}

// {{{ FUNCTION avi_reader_c::is_keyframe

int
avi_reader_c::is_keyframe(unsigned char *data,
                          long size,
                          int suggestion) {
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
            (data[i + 2] == 0x01)) {
          if ((data[i + 3] == 0) || (data[i + 3] == 0xb0))
            return 1;
          if (data[i + 3] == 0xb6) {
            if ((data[i + 4] & 0xc0) == 0x00)
              return 1;
            else
              return 0;
          }
          i += 2;
        }
      }

      return suggestion;
    default:
      return suggestion;
  }
}

// }}}

// {{{ FUNCTION avi_reader_c::read

file_status_e
avi_reader_c::read(generic_packetizer_c *ptzr,
                   bool force) {
  vector<avi_demuxer_t>::const_iterator demuxer;
  int key, last_frame, frames_read, size;
  long nread;
  bool need_more_data, done;
  int64_t duration;

  key = 0;
  nread = 0;
  if ((vptzr != -1) && !video_done && (PTZR(vptzr) == ptzr)) {
    debug_enter("avi_reader_c::read (video)");

    need_more_data = false;
    last_frame = 0;
    done = false;

    // Make sure we have a frame to work with.
    while (NULL == old_chunk) {
      debug_enter("AVI_read_frame");
      nread = AVI_read_frame(avi, (char *)chunk, &key);
      debug_leave("AVI_read_frame");
      if (nread < 0) {
        frames = maxframes + 1;
        done = true;
        break;
      } else if (nread > 0) {
        old_chunk = chunk;
        chunk = (unsigned char *)safemalloc(chunk_size);
        old_key = key;
        old_nread = nread;
        frames++;
      } else {
        ++frames;
        ++dropped_frames;
      }
    }
    if (!done) {
      frames_read = 1;
      // Check whether we have identical frames
      while (!done && (frames <= (maxframes - 1))) {
        debug_enter("AVI_read_frame");
        nread = AVI_read_frame(avi, (char *)chunk, &key);
        debug_leave("AVI_read_frame");
        if (nread < 0) {
          PTZR(vptzr)->process(new packet_t(new memory_c(old_chunk, old_nread,
                                                         true),
                                            -1, -1,
                                            old_key ? VFT_IFRAME :
                                            VFT_PFRAMEAUTOMATIC,
                                            VFT_NOBFRAME));
          bytes_processed += old_nread;
          old_chunk = NULL;
          mxwarn(PFX "Reading frame number %d resulted in an error. "
                 "Aborting this track.\n", frames);
          frames = maxframes + 1;
          break;
        }
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
      duration = (int64_t)(1000000000.0 * frames_read / fps);
      if (nread > 0) {
        PTZR(vptzr)->process(new packet_t(new memory_c(old_chunk, old_nread,
                                                       true),
                                          -1, duration,
                                          old_key ? VFT_IFRAME :
                                          VFT_PFRAMEAUTOMATIC,
                                          VFT_NOBFRAME));
        bytes_processed += old_nread;
        old_chunk = NULL;
        if (! last_frame) {
          if (old_chunk != NULL)
            safefree(old_chunk);
          old_chunk = chunk;
          chunk = (unsigned char *)safemalloc(chunk_size);
          old_key = key;
          old_nread = nread;
        } else if (nread > 0) {
          PTZR(vptzr)->process(new packet_t(new memory_c(chunk, nread, false),
                                            -1, duration,
                                            key ? VFT_IFRAME :
                                            VFT_PFRAMEAUTOMATIC,
                                            VFT_NOBFRAME));
          bytes_processed += nread;
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
      return FILE_STATUS_MOREDATA;
    else {
      PTZR(vptzr)->flush();
      return FILE_STATUS_DONE;
    }
  }

  foreach(demuxer, ademuxers) {
    unsigned char *audio_chunk;

    if (PTZR(demuxer->ptzr) != ptzr)
      continue;

    debug_enter("avi_reader_c::read (audio)");

    need_more_data = false;

    AVI_set_audio_track(avi, demuxer->aid);
    size = AVI_read_audio_chunk(avi, NULL);
    if (size > 0) {
      audio_chunk = (unsigned char *)safemalloc(size);

      debug_enter("AVI_read_audio");
      nread = AVI_read_audio_chunk(avi, (char *)audio_chunk);
      debug_leave("AVI_read_audio");

      if (nread > 0) {
        size = AVI_read_audio_chunk(avi, NULL);
        if (size > 0)
          need_more_data = true;
        PTZR(demuxer->ptzr)->add_avi_block_size(nread);
        PTZR(demuxer->ptzr)->process(new packet_t(new memory_c(audio_chunk,
                                                               nread, true)));
        bytes_processed += nread;
      } else
        safefree(audio_chunk);
    }

    debug_leave("avi_reader_c::read (audio)");

    if (need_more_data)
      return FILE_STATUS_MOREDATA;
    else {
      PTZR(demuxer->ptzr)->flush();
      return FILE_STATUS_DONE;
    }
  }

  return FILE_STATUS_DONE;
}

// }}}

int
avi_reader_c::get_progress() {
  if (bytes_to_process == 0)
    return 0;
  return 100 * bytes_processed / bytes_to_process;
}

// {{{ FUNCTION avi_reader_c::identify

void
avi_reader_c::identify() {
  int i;
  const char *type;
  uint32_t par_num, par_den;
  bool extended_info_shown;

  mxinfo("File '%s': container: AVI\n", ti.fname.c_str());
  extended_info_shown = false;
  type = AVI_video_compressor(avi);
  if (!strncasecmp(type, "MP42", 4) ||
      !strncasecmp(type, "DIV2", 4) ||
      !strncasecmp(type, "DIVX", 4) ||
      !strncasecmp(type, "XVID", 4) ||
      !strncasecmp(type, "DX50", 4)) {
    unsigned char *buffer;
    uint32_t width, height, disp_width, disp_height;
    float aspect_ratio;
    int size, key;
    string extended_info;

    size = AVI_frame_size(avi, 0);
    if (size > 0) {
      buffer = (unsigned char *)safemalloc(size);
      AVI_read_frame(avi, (char *)buffer, &key);
      if (mpeg4_p2_extract_par(buffer, size, par_num, par_den)) {
        width = AVI_video_width(avi);
        height = AVI_video_height(avi);
        aspect_ratio = (float)width / (float)height * (float)par_num /
          (float)par_den;
        if (aspect_ratio > ((float)width / (float)height)) {
          disp_width = irnd(height * aspect_ratio);
          disp_height = height;
        } else {
          disp_width = width;
          disp_height = irnd(width / aspect_ratio);
        }
        if (identify_verbose)
          extended_info = mxsprintf(" [display_dimensions:%ux%u ]",
                                    disp_width, disp_height);
        mxinfo("Track ID 0: video (%s)%s\n", type, extended_info.c_str());
        extended_info_shown = true;
      }
      safefree(buffer);
    }
  }
  if (!extended_info_shown)
    mxinfo("Track ID 0: video (%s)\n", AVI_video_compressor(avi));
  for (i = 0; i < AVI_audio_tracks(avi); i++) {
    AVI_set_audio_track(avi, i);
    switch (AVI_audio_format(avi)) {
      case 0x0001:
        type = "PCM";
        break;
      case 0x0050:
        type = "MP2";
        break;
      case 0x0055:
        type = "MP3";
        break;
      case 0x2000:
        type = "AC3";
        break;
      case 0x2001:
        type = "DTS";
        break;
      case 0x00ff:
        type = "AAC";
        break;
      default:
        type = "unknown";
    }
    mxinfo("Track ID %d: audio (%s)\n", i + 1, type);
  }
}

void
avi_reader_c::add_available_track_ids() {
  int i;

  // Yes, this is correct. Don't forget the video track!
  for (i = 0; i <= AVI_audio_tracks(avi); i++)
    available_track_ids.push_back(i);
}

// }}}
