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
#include "hacks.h"
#include "mpeg4_common.h"
#include "output_control.h"
#include "r_avi.h"
#include "p_aac.h"
#include "p_ac3.h"
#include "p_avc.h"
#include "p_dts.h"
#include "p_mp3.h"
#include "p_pcm.h"
#include "p_video.h"
#include "p_vorbis.h"

// }}}

// {{{ FUNCTION avi_reader_c::probe_file

int
avi_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  unsigned char data[12];

  if (12 > size)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(data, 12) != 12)
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }

  if (strncasecmp((char *)data, "RIFF", 4) || strncasecmp((char *)data + 8, "AVI ", 4))
    return 0;

  return 1;
}

// }}}

// {{{ C'TOR

avi_reader_c::avi_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  divx_type(DIVX_TYPE_NONE),
  avi(NULL),
  vptzr(-1),
  fps(1.0),
  video_frames_read(0),
  max_video_frames(0),
  dropped_video_frames(0),
  act_wchar(0),
  rederive_keyframes(false),
  bytes_to_process(0),
  bytes_processed(0) {

  int64_t size;

  try {
    mm_file_io_c io(ti.fname);
    size = io.get_size();
    if (!avi_reader_c::probe_file(&io, size))
      throw error_c(Y("avi_reader: Source is not a valid AVI file."));

  } catch (...) {
    throw error_c(Y("avi_reader: Could not read the source file."));
  }

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the AVI demultiplexer. Opening file. This may take some time depending on the file's size.\n"));

  if (NULL == (avi = AVI_open_input_file(ti.fname.c_str(), 1)))
    throw error_c(boost::format(Y("avi_reader: Could not initialize AVI source. Reason: %1%")) % AVI_strerror());

  fps              = AVI_frame_rate(avi);
  max_video_frames = AVI_video_frames(avi);
}

// }}}

// {{{ D'TOR

avi_reader_c::~avi_reader_c() {
  if (NULL != avi)
    AVI_close(avi);

  ti.private_data = NULL;

  mxverb(2, boost::format(Y("avi_reader_c: Dropped video frames: %1%\n")) % dropped_video_frames);
}

// }}}

void
avi_reader_c::create_packetizer(int64_t tid) {
  char *codec;

  if ((0 == tid) && demuxing_requested('v', 0) && (-1 == vptzr)) {
    int i;

    mxverb_tid(4, ti.fname, 0, Y("frame sizes:\n"));

    for (i = 0; i < max_video_frames; i++) {
      bytes_to_process += AVI_frame_size(avi, i);
      mxverb(4, boost::format("  %1%: %2%\n") % i % AVI_frame_size(avi, i));
    }

    codec = AVI_video_compressor(avi);
    if (mpeg4::p2::is_v3_fourcc(codec))
      divx_type = DIVX_TYPE_V3;
    else if (mpeg4::p2::is_fourcc(codec))
      divx_type = DIVX_TYPE_MPEG4;

    ti.private_data = (unsigned char *)avi->bitmap_info_header;
    if (NULL != ti.private_data)
      ti.private_size = get_uint32_le(&avi->bitmap_info_header->bi_size);

    mxverb(4, boost::format(Y("track extra data size: %1%\n")) % (ti.private_size - sizeof(alBITMAPINFOHEADER)));
    if (sizeof(alBITMAPINFOHEADER) < ti.private_size) {
      mxverb(4, "  ");
      for (i = sizeof(alBITMAPINFOHEADER); i < ti.private_size; ++i)
        mxverb(4, boost::format("%|1$02x| ") % ti.private_data[i]);
      mxverb(4, "\n");
    }

    ti.id = 0;                 // ID for the video track.
    if (DIVX_TYPE_MPEG4 == divx_type) {
      vptzr = add_packetizer(new mpeg4_p2_video_packetizer_c(this, AVI_frame_rate(avi), AVI_video_width(avi), AVI_video_height(avi), false, ti));
      if (verbose)
        mxinfo_tid(ti.fname, 0, Y("Using the MPEG-4 part 2 video output module.\n"));

    } else if (mpeg4::p10::is_avc_fourcc(codec) && !hack_engaged(ENGAGE_ALLOW_AVC_IN_VFW_MODE)) {
      try {
        memory_cptr avcc                      = extract_avcc();
        mpeg4_p10_es_video_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, avcc, AVI_video_width(avi), AVI_video_height(avi), ti);
        vptzr                                 = add_packetizer(ptzr);

        ptzr->enable_timecode_generation(false);
        ptzr->set_track_default_duration((int64_t)(1000000000 / AVI_frame_rate(avi)));

        if (verbose)
          mxinfo_tid(ti.fname, 0, Y("Using the MPEG-4 part 10 ES video output module.\n"));

      } catch (...) {
        mxerror_tid(ti.fname, 0, Y("Could not extract the decoder specific config data (AVCC) from this AVC/h.264 track.\n"));
      }
    } else {
      vptzr = add_packetizer(new video_packetizer_c(this, NULL, AVI_frame_rate(avi), AVI_video_width(avi), AVI_video_height(avi), ti));

      if (verbose)
        mxinfo_tid(ti.fname, 0, Y("Using the video output module.\n"));
    }
  }

  if (0 == tid)
    return;

  if ((tid <= AVI_audio_tracks(avi)) && demuxing_requested('a', tid))
    add_audio_demuxer(tid - 1);
}

void
avi_reader_c::create_packetizers() {
  int i;

  create_packetizer(0);

  for (i = 0; i < AVI_audio_tracks(avi); i++)
    create_packetizer(i + 1);
}

memory_cptr
avi_reader_c::extract_avcc() {
  avc_es_parser_c parser;
  int i, size, key;

  parser.ignore_nalu_size_length_errors();
  if (map_has_key(ti.nalu_size_lengths, 0))
    parser.set_nalu_size_length(ti.nalu_size_lengths[0]);
  else if (map_has_key(ti.nalu_size_lengths, -1))
    parser.set_nalu_size_length(ti.nalu_size_lengths[-1]);

  for (i = 0; i < max_video_frames; ++i) {
    size = AVI_frame_size(avi, i);
    if (0 == size)
      continue;

    memory_cptr buffer(new memory_c((unsigned char *)safemalloc(size), size, true));

    AVI_set_video_position(avi, i);
    size = AVI_read_frame(avi, (char *)buffer->get(), &key);

    if (0 < size) {
      parser.add_bytes(buffer->get(), size);
      if (parser.headers_parsed()) {
        AVI_set_video_position(avi, 0);
        return parser.get_avcc();
      }
    }
  }

  AVI_set_video_position(avi, 0);

  throw false;
}

// {{{ FUNCTION avi_reader_c::add_audio_demuxer

void
avi_reader_c::add_audio_demuxer(int aid) {
  vector<avi_demuxer_t>::const_iterator it;
  avi_demuxer_t demuxer;

  mxforeach(it, ademuxers)
    if (it->aid == aid) // Demuxer already added?
      return;

  AVI_set_audio_track(avi, aid);
  if (AVI_read_audio_chunk(avi, NULL) < 0) {
    mxwarn(boost::format(Y("Could not find an index for audio track %1% (avilib error message: %2%). Skipping track.\n")) % (aid + 1) % AVI_strerror());
    return;
  }

  memset(&demuxer, 0, sizeof(avi_demuxer_t));

  generic_packetizer_c *packetizer = NULL;
  alWAVEFORMATEX *wfe              = avi->wave_format_ex[aid];
  uint32_t audio_format            = AVI_audio_format(avi);

  demuxer.aid                      = aid;
  demuxer.ptzr                     = -1;
  demuxer.samples_per_second       = AVI_audio_rate(avi);
  demuxer.channels                 = AVI_audio_channels(avi);
  demuxer.bits_per_sample          = AVI_audio_bits(avi);

  ti.id                            = aid + 1;       // ID for this audio track.
  ti.avi_block_align               = get_uint16_le(&wfe->n_block_align);
  ti.avi_avg_bytes_per_sec         = get_uint32_le(&wfe->n_avg_bytes_per_sec);
  ti.avi_samples_per_chunk         = get_uint32_le(&avi->stream_headers[aid].dw_scale);
  ti.avi_samples_per_sec           = demuxer.samples_per_second;

  if (get_uint16_le(&wfe->cb_size) > 0) {
    ti.private_data                = (unsigned char *)(wfe + 1);
    ti.private_size                = get_uint16_le(&wfe->cb_size);
  } else {
    ti.private_data                = NULL;
    ti.private_size                = 0;
  }

  switch(audio_format) {
    case 0x0001:                // raw PCM audio
    case 0x0003:                // raw PCM audio (float)
      packetizer = new pcm_packetizer_c(this, demuxer.samples_per_second, demuxer.channels, demuxer.bits_per_sample, ti, false, audio_format == 0x0003);

      if (verbose)
        mxinfo_tid(ti.fname, aid + 1, Y("Using the PCM output module.\n"));
      break;

    case 0x0050:                // MP2
    case 0x0055:                // MP3
      packetizer = new mp3_packetizer_c(this, demuxer.samples_per_second, demuxer.channels, false, ti);

      if (verbose)
        mxinfo_tid(ti.fname, aid + 1, Y("Using the MPEG audio output module.\n"));
      break;

    case 0x2000:                // AC3
      packetizer = new ac3_packetizer_c(this, demuxer.samples_per_second, demuxer.channels, 0, ti);

      if (verbose)
        mxinfo_tid(ti.fname, aid + 1, Y("Using the AC3 output module.\n"));
      break;

    case 0x2001: {              // DTS
      dts_header_t dtsheader;

      dtsheader.core_sampling_frequency = demuxer.samples_per_second;
      dtsheader.audio_channels          = demuxer.channels;
      packetizer                        = new dts_packetizer_c(this, dtsheader, ti, true);

      if (verbose)
        mxinfo_tid(ti.fname, aid + 1, Y("Using the DTS output module.\n"));
      break;
    }

    case 0x00ff:
    case 0x706d:                // AAC
      packetizer = create_aac_packetizer(aid, demuxer);
      break;

    case 0x566f:                // Vorbis
      packetizer = create_vorbis_packetizer(aid);
      break;

    default:
      mxerror_tid(ti.fname, aid + 1, boost::format(Y("Unknown/unsupported audio format 0x%|1$04x| for this audio rack.\n")) % audio_format);
  }

  demuxer.ptzr = add_packetizer(packetizer);

  ademuxers.push_back(demuxer);

  int i, maxchunks = AVI_audio_chunks(avi);
  for (i = 0; i < maxchunks; i++)
    bytes_to_process += AVI_audio_size(avi, i);
}

// }}}

generic_packetizer_c *
avi_reader_c::create_aac_packetizer(int aid,
                                    avi_demuxer_t &demuxer) {
  int profile, channels, sample_rate, output_sample_rate;
  bool is_sbr;

  bool aac_data_created  = false;
  bool headerless        = (AVI_audio_format(avi) != 0x706d);

  if (0 == ti.private_size) {
    aac_data_created     = true;
    channels             = AVI_audio_channels(avi);
    sample_rate          = AVI_audio_rate(avi);
    if (44100 > sample_rate) {
      profile            = AAC_PROFILE_SBR;
      output_sample_rate = sample_rate * 2;
      is_sbr             = true;
    } else {
      profile            = AAC_PROFILE_MAIN;
      output_sample_rate = sample_rate;
      is_sbr             = false;
    }

    unsigned char created_aac_data[AAC_MAX_PRIVATE_DATA_SIZE];

    ti.private_size      = create_aac_data(created_aac_data, profile, channels, sample_rate, output_sample_rate, is_sbr);
    ti.private_data      = created_aac_data;

  } else {
    if ((2 != ti.private_size) && (5 != ti.private_size))
      mxerror_tid(ti.fname, aid + 1,
                  boost::format(Y("This AAC track does not contain valid headers. The extra header size is %1% bytes, expected were 2 or 5 bytes.\n")) % ti.private_size);

    if (!parse_aac_data(ti.private_data, ti.private_size, profile, channels, sample_rate, output_sample_rate, is_sbr))
      mxerror_tid(ti.fname, aid + 1, Y("This AAC track does not contain valid headers. Could not parse the AAC information.\n"));

    if (is_sbr)
      profile = AAC_PROFILE_SBR;
  }

  demuxer.samples_per_second       = sample_rate;
  demuxer.channels                 = channels;

  generic_packetizer_c *packetizer = new aac_packetizer_c(this, AAC_ID_MPEG4, profile, demuxer.samples_per_second, demuxer.channels, ti, false, headerless);

  if (is_sbr)
    packetizer->set_audio_output_sampling_freq(output_sample_rate);

  if (aac_data_created) {
    ti.private_size = 0;
    ti.private_data = NULL;
  }

  if (verbose)
    mxinfo_tid(ti.fname, aid + 1, Y("Using the AAC audio output module.\n"));

  return packetizer;
}

generic_packetizer_c *
avi_reader_c::create_vorbis_packetizer(int aid) {
  try {
    if (!ti.private_data || !ti.private_size)
      throw error_c(Y("Invalid Vorbis headers in AVI audio track."));

    unsigned char *c = (unsigned char *)ti.private_data;

    if (2 != c[0])
      throw error_c(Y("Invalid Vorbis headers in AVI audio track."));

    int offset           = 1;
    const int laced_size = ti.private_size;
    int i;

    int header_sizes[3];
    unsigned char *headers[3];

    for (i = 0; 2 > i; ++i) {
      int size = 0;

      while ((offset < laced_size) && ((unsigned char)255 == c[offset])) {
        size += 255;
        ++offset;
      }
      if ((laced_size - 1) <= offset)
        throw error_c(Y("Invalid Vorbis headers in AVI audio track."));

      size            += c[offset];
      header_sizes[i]  = size;
      ++offset;
    }

    headers[0]      = &c[offset];
    headers[1]      = &c[offset + header_sizes[0]];
    headers[2]      = &c[offset + header_sizes[0] + header_sizes[1]];
    header_sizes[2] = laced_size - offset - header_sizes[0] - header_sizes[1];

    ti.private_data = NULL;
    ti.private_size = 0;

    vorbis_packetizer_c *ptzr = new vorbis_packetizer_c(this, headers[0], header_sizes[0], headers[1], header_sizes[1], headers[2], header_sizes[2], ti);

    if (verbose)
      mxinfo_tid(ti.fname, aid + 1, Y("Using the Vorbis output module.\n"));

    return ptzr;

  } catch (error_c &e) {
    mxerror_tid(ti.fname, aid + 1, boost::format("%1%\n") % e.get_error());

    // Never reached, but make the compiler happy:
    return NULL;
  }
}

// {{{ FUNCTION avi_reader_c::is_keyframe

int
avi_reader_c::is_keyframe(unsigned char *data,
                          long size,
                          int suggestion) {
  int i;

  if (!rederive_keyframes)
    return suggestion;

  switch (divx_type) {
    case DIVX_TYPE_V3:
      i = *((int *)data);
      return ((i & 0x40000000) ? 0 : 1);

    case DIVX_TYPE_MPEG4:
      for (i = 0; i < size - 5; i++) {
        if ((0x00 == data[i]) && (0x00 == data[i + 1]) && (0x01 == data[i + 2])) {
          if ((0 == data[i + 3]) || (0xb0 == data[i + 3]))
            return 1;

          if (0xb6 == data[i + 3])
            return 0x00 == (data[i + 4] & 0xc0);

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
avi_reader_c::read_video() {
  if (video_frames_read >= max_video_frames)
    return FILE_STATUS_DONE;

  unsigned char *chunk = NULL;
  int key              = 0;

  int size, nread;

  do {
    safefree(chunk);
    size  = AVI_frame_size(avi, video_frames_read);
    chunk = (unsigned char *)safemalloc(size);
    nread = AVI_read_frame(avi, (char *)chunk, &key);

    ++video_frames_read;

    if (0 > nread) {
      // Error reading the frame: abort
      video_frames_read = max_video_frames;
      safefree(chunk);
      return FILE_STATUS_DONE;

    } else if (0 == nread)
      ++dropped_video_frames;

  } while ((0 == nread) && (video_frames_read < max_video_frames));


  if (0 == nread) {
    // This is only the case if the AVI contains dropped frames only.
    safefree(chunk);
    return FILE_STATUS_DONE;
  }

  int i;
  for (i = video_frames_read; i < max_video_frames; ++i) {
    if (0 != AVI_frame_size(avi, i))
      break;

    int dummy_key;
    AVI_read_frame(avi, NULL, &dummy_key);
  }

  int64_t timestamp     = (int64_t)(((int64_t)video_frames_read - 1)     * 1000000000ll / fps);
  int64_t duration      = (int64_t)(((int64_t)i - video_frames_read + 1) * 1000000000ll / fps);

  dropped_video_frames += i - video_frames_read;
  video_frames_read     = i;

  PTZR(vptzr)->process(new packet_t(new memory_c(chunk, nread, true), timestamp, duration, key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME));

  bytes_processed += nread;

  if (video_frames_read >= max_video_frames) {
    PTZR(vptzr)->flush();
    return FILE_STATUS_DONE;
  }

  return FILE_STATUS_MOREDATA;
}

file_status_e
avi_reader_c::read_audio(avi_demuxer_t &demuxer) {
  AVI_set_audio_track(avi, demuxer.aid);
  int size = AVI_read_audio_chunk(avi, NULL);

  if (0 >= size) {
    PTZR(demuxer.ptzr)->flush();
    return FILE_STATUS_DONE;
  }

  unsigned char *chunk = (unsigned char *)safemalloc(size);
  size                 = AVI_read_audio_chunk(avi, (char *)chunk);

  if (0 >= size) {
    safefree(chunk);
    PTZR(demuxer.ptzr)->flush();
    return FILE_STATUS_DONE;
  }

  bool need_more_data = 0 != AVI_read_audio_chunk(avi, NULL);

  PTZR(demuxer.ptzr)->add_avi_block_size(size);
  PTZR(demuxer.ptzr)->process(new packet_t(new memory_c(chunk, size, true)));

  bytes_processed += size;

  if (need_more_data)
    return FILE_STATUS_MOREDATA;
  else {
    PTZR(demuxer.ptzr)->flush();
    return FILE_STATUS_DONE;
  }
}

file_status_e
avi_reader_c::read(generic_packetizer_c *ptzr,
                   bool force) {
  vector<avi_demuxer_t>::iterator demuxer;

  if ((-1 != vptzr) && (PTZR(vptzr) == ptzr))
    return read_video();

  mxforeach(demuxer, ademuxers)
    if ((-1 != demuxer->ptzr) && (PTZR(demuxer->ptzr) == ptzr))
      return read_audio(*demuxer);

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
avi_reader_c::extended_identify_mpeg4_l2(vector<string> &extended_info) {
  int size = AVI_frame_size(avi, 0);
  if (0 >= size)
    return;

  memory_cptr af_buffer(new memory_c(safemalloc(size), 0), true);
  unsigned char *buffer = af_buffer->get();
  int dummy_key;

  AVI_read_frame(avi, (char *)buffer, &dummy_key);

  uint32_t par_num, par_den;
  if (mpeg4::p2::extract_par(buffer, size, par_num, par_den)) {
    int width          = AVI_video_width(avi);
    int height         = AVI_video_height(avi);
    float aspect_ratio = (float)width / (float)height * (float)par_num / (float)par_den;

    int disp_width, disp_height;
    if (aspect_ratio > ((float)width / (float)height)) {
      disp_width  = irnd(height * aspect_ratio);
      disp_height = height;

    } else {
      disp_width  = width;
      disp_height = irnd(width / aspect_ratio);
    }

    extended_info.push_back((boost::format("display_dimensions:%1%x%2%") % disp_width % disp_height).str());
  }
}

void
avi_reader_c::identify() {
  vector<string> extended_info;

  id_result_container("AVI");

  string type = AVI_video_compressor(avi);

  if (IS_MPEG4_L2_FOURCC(type.c_str()))
    extended_identify_mpeg4_l2(extended_info);

  else if (mpeg4::p10::is_avc_fourcc(type.c_str()))
    extended_info.push_back("packetizer:mpeg4_p10_es_video");

  id_result_track(0, ID_RESULT_TRACK_VIDEO, type, join(" ", extended_info));

  int i;
  for (i = 0; i < AVI_audio_tracks(avi); i++) {
    AVI_set_audio_track(avi, i);
    unsigned int audio_format = AVI_audio_format(avi);

    switch (audio_format) {
      case 0x0001:
      case 0x0003:
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
      case 0x706d:
        type = "AAC";
        break;
      case 0x566f:
        type = "Vorbis";
        break;
      default:
        type = (boost::format("unsupported (0x%|1$04x|)") % audio_format).str();
    }

    id_result_track(i + 1, ID_RESULT_TRACK_AUDIO, type);
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
