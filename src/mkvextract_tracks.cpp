/*
  mkvextract -- extract tracks from Matroska files into other files

  mkvextract_tracks.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief extracts tracks from Matroska files into other files
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlVoid.h>

#include <matroska/FileKax.h>
#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "librmff.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

int conv_utf8;
vector<rmff_file_t *> rmfiles;

static rmff_file_t *
open_rmff_file(const char *name) {
  rmff_file_t *file;
  int i;

  for (i = 0; i < rmfiles.size(); i++)
    if (!strcmp(name, rmfiles[i]->name))
      return rmfiles[i];

  file = rmff_open_file(name, RMFF_OPEN_MODE_WRITING);
  if (file == NULL)
    mxerror("Could not create '%s'. Reason: %d (%s). Aborting.\n", name,
            errno, strerror(errno));
  rmfiles.push_back(file);

  return file;
}

static void
set_rmff_headers() {
  int i;

  for (i = 0; i < rmfiles.size(); i++) {
    rmfiles[i]->cont_header_present = 1;
    rmff_write_headers(rmfiles[i]);
  }
}

static void
close_rmff_files() {
  int i;

  for (i = 0; i < rmfiles.size(); i++) {
    rmff_write_index(rmfiles[i]);
    rmff_fix_headers(rmfiles[i]);
    rmff_close_file(rmfiles[i]);
  }
}

static void
flush_ogg_pages(kax_track_t &track) {
  ogg_page page;

  while (ogg_stream_flush(&track.osstate, &page)) {
    track.out->write(page.header, page.header_len);
    track.out->write(page.body, page.body_len);
  }
}

static void
write_ogg_pages(kax_track_t &track) {
  ogg_page page;

  while (ogg_stream_pageout(&track.osstate, &page)) {
    track.out->write(page.header, page.header_len);
    track.out->write(page.body, page.body_len);
  }
}

// }}}

// {{{ FUNCTION create_output_files()

static void
check_output_files() {
  int i, k, offset;
  bool something_to_do, is_ok;
  unsigned char *c;

  something_to_do = false;

  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      tracks[i].in_use = false;

      if (tracks[i].codec_id == NULL) {
        mxwarn("Track ID %lld is missing the CodecID. Skipping track.\n",
               tracks[i].tid);
        continue;
      }

      // Video tracks: 
      if (tracks[i].track_type == 'v') {
        if (!strcmp(tracks[i].codec_id, MKV_V_MSCOMP)) {
          if ((tracks[i].v_width == 0) || (tracks[i].v_height == 0) ||
              (tracks[i].v_fps == 0.0) || (tracks[i].private_data == NULL) ||
              (tracks[i].private_size < sizeof(alBITMAPINFOHEADER))) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }
          tracks[i].type = TYPEAVI;

        } else if (!strncmp(tracks[i].codec_id, "V_REAL/", 7)) {
          if ((tracks[i].v_width == 0) || (tracks[i].v_height == 0) ||
              (tracks[i].private_data == NULL) ||
              (tracks[i].private_size < sizeof(real_video_props_t))) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }
          tracks[i].type = TYPEREAL;

        } else {
          mxwarn("The extraction of video tracks is only supported for "
                 "RealVideo (CodecID: V_REAL/RVxx) and AVI compatibility "
                 "tracks (CodecID: " MKV_V_MSCOMP "). Skipping track %lld.\n",
                 tracks[i].tid);
          continue;
        }

      } else if (tracks[i].track_type == 'a') {
        if (tracks[i].a_sfreq == 0.0)
          tracks[i].a_sfreq = 8000.0;
        if (tracks[i].a_channels == 0)
          tracks[i].a_channels = 1;

        if (!strcmp(tracks[i].codec_id, MKV_A_VORBIS)) {
          tracks[i].type = TYPEOGM; // Yeah, I know, I know...
          if (tracks[i].private_data == NULL) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }

          c = (unsigned char *)tracks[i].private_data;
          if (c[0] != 2) {
            mxwarn("Vorbis track ID %lld does not contain "
                   "valid headers. Skipping track.\n", tracks[i].tid);
            continue;
          }

          is_ok = true;
          offset = 1;
          for (k = 0; k < 2; k++) {
            int length = 0;
            while ((c[offset] == (unsigned char)255) &&
                   (length < tracks[i].private_size)) {
              length += 255;
              offset++;
            }
            if (offset >= (tracks[i].private_size - 1)) {
              mxwarn("Vorbis track ID %lld does not contain "
                     "valid headers. Skipping track.\n", tracks[i].tid);
              is_ok = false;
              break;
            }
            length += c[offset];
            offset++;
            tracks[i].header_sizes[k] = length;
          }

          if (!is_ok)
            continue;

          tracks[i].headers[0] = &c[offset];
          tracks[i].headers[1] = &c[offset + tracks[i].header_sizes[0]];
          tracks[i].headers[2] = &c[offset + tracks[i].header_sizes[0] +
                                    tracks[i].header_sizes[1]];
          tracks[i].header_sizes[2] = tracks[i].private_size -
            offset - tracks[i].header_sizes[0] - tracks[i].header_sizes[1];

        } else if (!strcmp(tracks[i].codec_id, MKV_A_MP3))
          tracks[i].type = TYPEMP3;
        else if (!strcmp(tracks[i].codec_id, MKV_A_AC3))
          tracks[i].type = TYPEAC3;
        else if (!strcmp(tracks[i].codec_id, MKV_A_PCM)) {
          tracks[i].type = TYPEWAV; // Yeah, I know, I know...
          if (tracks[i].a_bps == 0) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }

        } else if (!strncmp(tracks[i].codec_id, "A_AAC", 5)) {
          if (strlen(tracks[i].codec_id) < strlen("A_AAC/MPEG4/LC")) {
            mxwarn("Track ID %lld has an unknown AAC "
                   "type. Skipping track.\n", tracks[i].tid);
            continue;
          }

          // A_AAC/MPEG4/MAIN
          // 0123456789012345
          if (tracks[i].codec_id[10] == '4')
            tracks[i].aac_id = 0;
          else if (tracks[i].codec_id[10] == '2')
            tracks[i].aac_id = 1;
          else {
            mxwarn("Track ID %lld has an unknown AAC "
                   "type. Skipping track.\n", tracks[i].tid);
            continue;
          }

          if (!strcmp(&tracks[i].codec_id[12], "MAIN"))
            tracks[i].aac_profile = 0;
          else if (!strcmp(&tracks[i].codec_id[12], "LC") ||
                   (strstr(&tracks[i].codec_id[12], "SBR") != NULL))
            tracks[i].aac_profile = 1;
          else if (!strcmp(&tracks[i].codec_id[12], "SSR"))
            tracks[i].aac_profile = 2;
          else if (!strcmp(&tracks[i].codec_id[12], "LTP"))
            tracks[i].aac_profile = 3;
          else {
            mxwarn("Track ID %lld has an unknown AAC "
                   "type. Skipping track.\n", tracks[i].tid);
            continue;
          }

          if (92017 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 0;
          else if (75132 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 1;
          else if (55426 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 2;
          else if (46009 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 3;
          else if (37566 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 4;
          else if (27713 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 5;
          else if (23004 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 6;
          else if (18783 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 7;
          else if (13856 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 8;
          else if (11502 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 9;
          else if (9391 <= tracks[i].a_sfreq)
            tracks[i].aac_srate_idx = 10;
          else
            tracks[i].aac_srate_idx = 11;

          tracks[i].type = TYPEAAC;

        } else if (!strcmp(tracks[i].codec_id, MKV_A_DTS)) {
          mxwarn("Extraction of DTS is not supported - yet. "
                 "I promise I'll implement it. Really Soon Now (tm)! "
                 "Skipping track.\n");
          continue;

        } else if (!strcmp(tracks[i].codec_id, MKV_A_FLAC)) {
          if (tracks[i].private_data == NULL) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }

          tracks[i].type = TYPEFLAC;

        } else if (!strncmp(tracks[i].codec_id, "A_REAL/", 7)) {
          if ((tracks[i].private_data == NULL) ||
              (tracks[i].private_size < sizeof(real_audio_v4_props_t))) {
            mxwarn("Track ID %lld is missing some critical "
                   "information. Skipping track.\n", tracks[i].tid);
            continue;
          }

          tracks[i].type = TYPEREAL;

        } else {
          mxwarn("Unsupported CodecID '%s' for ID %lld. "
                 "Skipping track.\n", tracks[i].codec_id, tracks[i].tid);
          continue;
        }

      } else if (tracks[i].track_type == 's') {
        if (!strcmp(tracks[i].codec_id, MKV_S_TEXTUTF8))
          tracks[i].type = TYPESRT;
        else if (!strcmp(tracks[i].codec_id, MKV_S_TEXTSSA) ||
                 !strcmp(tracks[i].codec_id, MKV_S_TEXTASS) ||
                 !strcmp(tracks[i].codec_id, "S_SSA") ||
                 !strcmp(tracks[i].codec_id, "S_ASS"))
          tracks[i].type = TYPESSA;
        else {
          mxwarn("Unsupported CodecID '%s' for ID %lld. "
                 "Skipping track.\n", tracks[i].codec_id, tracks[i].tid);
          continue;
        }

      } else {
        mxwarn("Unknown track type for ID %lld. Skipping "
               "track.\n", tracks[i].tid);
        continue;
      }

      tracks[i].in_use = true;
      something_to_do = true;
    } else
      mxwarn("There is no track with the ID '%lld' in the "
             "input file.\n", tracks[i].tid);
  }

  if (!something_to_do) {
    mxinfo("Nothing to do. Exiting.\n");
    mxexit(0);
  }
}

static void
create_output_files() {
  int i;

  check_output_files();

  // Now finally create some files. Hell, I *WANT* to create something now.
  // RIGHT NOW! Or I'll go insane...
  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      if (tracks[i].type == TYPEAVI) {
        alBITMAPINFOHEADER *bih;
        char ccodec[5];

        tracks[i].avi = AVI_open_output_file(tracks[i].out_name);
        if (tracks[i].avi == NULL)
          mxerror("Could not create '%s'. Reason: %s.\n",
                  tracks[i].out_name, AVI_strerror());

        bih = (alBITMAPINFOHEADER *)tracks[i].private_data;
        memcpy(ccodec, &bih->bi_compression, 4);
        ccodec[4] = 0;
        AVI_set_video(tracks[i].avi, tracks[i].v_width, tracks[i].v_height,
                      tracks[i].v_fps, ccodec);

        mxinfo("Extracting track ID %lld to an AVI file '%s'.\n",
               tracks[i].tid, tracks[i].out_name);

      } else if (tracks[i].type == TYPEREAL) {
        rmff_file_t *file;

        file = open_rmff_file(tracks[i].out_name);
        tracks[i].rmtrack = rmff_add_track(file, 1);
        if (tracks[i].rmtrack == NULL)
          mxerror("Memory allocation error: %d (%s).\n",
                  rmff_last_error, rmff_last_error_msg);
        rmff_set_type_specific_data(tracks[i].rmtrack,
                                    (unsigned char *)tracks[i].private_data,
                                    tracks[i].private_size);
        if (tracks[i].track_type == 'v')
          rmff_set_track_data(tracks[i].rmtrack, "Video",
                              "video/x-pn-realvideo");
        else
          rmff_set_track_data(tracks[i].rmtrack, "Audio",
                              "audio/x-pn-realaudio");

      } else {

        try {
          tracks[i].out = new mm_io_c(tracks[i].out_name, MODE_CREATE);
        } catch (exception &ex) {
          mxerror("Could not create '%s'. Reason: %d (%s). "
                  "Aborting.\n", tracks[i].out_name, errno, strerror(errno));
        }

        mxinfo("Extracting track ID %lld to a %s file '%s'.\n",
               tracks[i].tid, typenames[tracks[i].type],
               tracks[i].out_name);

        if (tracks[i].type == TYPEOGM) {
          ogg_packet op;

          ogg_stream_init(&tracks[i].osstate, rand());

          // Handle the three header packets: Headers, comments, codec
          // setup data.
          op.b_o_s = 1;
          op.e_o_s = 0;
          op.packetno = 0;
          op.packet = tracks[i].headers[0];
          op.bytes = tracks[i].header_sizes[0];
          op.granulepos = 0;
          ogg_stream_packetin(&tracks[i].osstate, &op);
          flush_ogg_pages(tracks[i]);
          op.b_o_s = 0;
          op.packetno = 1;
          op.packet = tracks[i].headers[1];
          op.bytes = tracks[i].header_sizes[1];
          ogg_stream_packetin(&tracks[i].osstate, &op);
          op.packetno = 2;
          op.packet = tracks[i].headers[2];
          op.bytes = tracks[i].header_sizes[2];
          ogg_stream_packetin(&tracks[i].osstate, &op);
          flush_ogg_pages(tracks[i]);
          tracks[i].packetno = 3;

        } else if (tracks[i].type == TYPEWAV) {
          wave_header *wh = &tracks[i].wh;

          // Write the WAV header.
          memcpy(&wh->riff.id, "RIFF", 4);
          memcpy(&wh->riff.wave_id, "WAVE", 4);
          memcpy(&wh->format.id, "fmt ", 4);
          put_uint32(&wh->format.len, 16);
          put_uint16(&wh->common.wFormatTag, 1);
          put_uint16(&wh->common.wChannels, tracks[i].a_channels);
          put_uint32(&wh->common.dwSamplesPerSec, (int)tracks[i].a_sfreq);
          put_uint32(&wh->common.dwAvgBytesPerSec,
                     tracks[i].a_channels * (int)tracks[i].a_sfreq *
                     tracks[i].a_bps / 8);
          put_uint16(&wh->common.wBlockAlign, 4);
          put_uint16(&wh->common.wBitsPerSample, tracks[i].a_bps);
          memcpy(&wh->data.id, "data", 4);

          tracks[i].out->write(wh, sizeof(wave_header));

        }  else if (tracks[i].type == TYPESRT) {
          tracks[i].srt_num = 1;
          tracks[i].out->write_bom(tracks[i].sub_charset);

        } else if (tracks[i].type == TYPESSA) {
          char *s;
          unsigned char *pd;
          int bom_len;
          string sconv;

          pd = (unsigned char *)tracks[i].private_data;
          bom_len = 0;
          // Skip any BOM that might be present.
          if ((tracks[i].private_size > 3) &&
              (pd[0] == 0xef) && (pd[1] == 0xbb) && (pd[2] == 0xbf))
            bom_len = 3;
          else if ((tracks[i].private_size > 4) &&
                   (pd[0] == 0xff) && (pd[1] == 0xfe) &&
                   (pd[2] == 0x00) && (pd[3] == 0x00))
            bom_len = 4;
          else if ((tracks[i].private_size > 4) &&
                   (pd[0] == 0x00) && (pd[1] == 0x00) &&
                   (pd[2] == 0xfe) && (pd[3] == 0xff))
            bom_len = 4;
          else if ((tracks[i].private_size > 2) &&
                   (pd[0] == 0xff) && (pd[1] == 0xfe))
            bom_len = 2;
          else if ((tracks[i].private_size > 2) &&
                   (pd[0] == 0xfe) && (pd[1] == 0xff))
            bom_len = 2;
          pd += bom_len;
          tracks[i].private_size -= bom_len;

          s = (char *)safemalloc(tracks[i].private_size + 1);
          memcpy(s, pd, tracks[i].private_size);
          s[tracks[i].private_size] = 0;
          sconv = s;
          safefree(s);
          tracks[i].out->write_bom(tracks[i].sub_charset);
          sconv += "\n[Events]\nFormat: Marked, Start, End, "
            "Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
          from_utf8(tracks[i].conv_handle, sconv);
          tracks[i].out->puts_unl(sconv.c_str());

        } else if (tracks[i].type == TYPEFLAC) {
          if (!tracks[i].embed_in_ogg)
            tracks[i].out->write(tracks[i].private_data,
                                 tracks[i].private_size);
          else {
            unsigned char *ptr;
            ogg_packet op;

            ogg_stream_init(&tracks[i].osstate, rand());

            // Handle the three header packets: Headers, comments, codec
            // setup data.
            op.b_o_s = 1;
            op.e_o_s = 0;
            op.packetno = 0;
            op.packet = (unsigned char *)"fLaC";
            op.bytes = 4;
            op.granulepos = 0;
            ogg_stream_packetin(&tracks[i].osstate, &op);
            flush_ogg_pages(tracks[i]);
            op.b_o_s = 0;
            op.packetno = 1;
            ptr = (unsigned char *)tracks[i].private_data;
            if ((tracks[i].private_size >= 4) && (ptr[0] == 'f') &&
                (ptr[1] == 'L') && (ptr[2] == 'a') && (ptr[3] == 'C')) {
              op.packet = ptr + 4;
              op.bytes = tracks[i].private_size - 4;
            } else {
              op.packet = (unsigned char *)tracks[i].private_data;
              op.bytes = tracks[i].private_size;
            }
            ogg_stream_packetin(&tracks[i].osstate, &op);
            flush_ogg_pages(tracks[i]);
            tracks[i].packetno = 2;
          }
        }
      }
    }
  }

  set_rmff_headers();
}

static void
unpack_real_video_frames(kax_track_t *track,
                         unsigned char *src,
                         uint32_t size,
                         uint32_t timecode,
                         bool is_key) {
  unsigned char *src_ptr, *ptr, *dst;
  int num_subpackets, i, offset, total_len;
  vector<uint32_t> packet_offsets, packet_lengths;
  rmff_frame_t *frame;

  track->packetno++;
  src_ptr = src;
  num_subpackets = *src_ptr + 1;
  src_ptr++;
  if (size < (num_subpackets * 8 + 1)) {
    mxwarn("RealVideo unpacking failed: frame size too small. Could not "
           "extract sub packet offsets. Skipping a frame.\n");
    return;
  }
  for (i = 0; i < num_subpackets; i++) {
    src_ptr += 4;
    packet_offsets.push_back(get_uint32(src_ptr));
    src_ptr += 4;
  }
  if ((packet_offsets[packet_offsets.size() - 1] + (src_ptr - src)) >= size) {
    mxwarn("RealVideo unpacking failed: frame size too small. Expected at "
           "least %u bytes, but the frame contains only %u. Skipping this "
           "frame.\n", packet_offsets[packet_offsets.size() - 1] +
           (src_ptr - src), size);
    return;
  }
  total_len = size - (src_ptr - src);
  for (i = 0; i < (num_subpackets - 1); i++)
    packet_lengths.push_back(packet_offsets[i + 1] - packet_offsets[i]);
  packet_lengths.push_back(total_len -
                           packet_offsets[packet_offsets.size() - 1]);

  dst = (unsigned char *)safemalloc(size * 2);
  for (i = 0; i < num_subpackets; i++) {
    ptr = dst;
    if (num_subpackets == 1) {
      *ptr = 0xc0;              // complete frame
      ptr++;

    } else {
      *ptr = (num_subpackets >> 1) & 0x7f; // number of subpackets
      if (i == (num_subpackets - 1)) // last fragment?
        *ptr |= 0x80;
      ptr++;

      *ptr = i + 1;             // fragment number
      *ptr |= ((num_subpackets & 0x01) << 7); // number of subpackets
      ptr++;
    }

    // total packet length:
    if (total_len > 0x3fff) {
      put_uint16_be(ptr, ((total_len & 0x3fff0000) >> 16));
      ptr += 2;
      put_uint16_be(ptr, total_len & 0x0000ffff);
    } else
      put_uint16_be(ptr, 0x4000 | total_len);
    ptr += 2;

    // fragment offset from beginning/end:
    if (num_subpackets == 1)
      offset = timecode;
    else if (i < (num_subpackets - 1))
      offset = packet_offsets[i];
    else
      // If it's the last packet then the 'offset' is the fragment's length.
      offset = packet_lengths[i];

    if (offset > 0x3fff) {
      put_uint16_be(ptr, ((offset & 0x3fff0000) >> 16));
      ptr += 2;
      put_uint16_be(ptr, offset & 0x0000ffff);
    } else
      put_uint16_be(ptr, 0x4000 | offset);
    ptr += 2;

    // sequence number = frame number & 0xff
    *ptr = (track->packetno - 1) & 0xff;
    ptr++;

    memcpy(ptr, src_ptr, packet_lengths[i]);
    src_ptr += packet_lengths[i];
    ptr += packet_lengths[i];

    frame = rmff_allocate_frame(ptr - dst, dst);
    if (frame == NULL)
      mxerror("Memory allocation error: Could not get a rmff_frame_t.\n");
    frame->timecode = timecode;
    if (is_key)
      frame->flags = RMFF_FRAME_FLAG_KEYFRAME;
    if (rmff_write_frame(track->rmtrack, frame) != RMFF_ERR_OK)
      mxwarn("Could not write a RealVideo fragment.\n");
    rmff_release_frame(frame);
    track->subpacketno++;
  }
  safefree(dst);
}

static void
handle_data(KaxBlock *block,
            int64_t block_duration,
            bool has_ref) {
  kax_track_t *track;
  int i, k, len, num;
  int64_t start, end;
  char *s, buffer[200], *s2, adts[56 / 8];
  vector<string> fields;
  string line, comma = ",";
  ssa_line_c ssa_line;
  rmff_frame_t *rmf_frame;

  track = find_track(block->TrackNum());
  if ((track == NULL) || !track->in_use){
    delete block;
    return;
  }

  if (block_duration == -1)
    block_duration = track->default_duration;

  start = block->GlobalTimecode() / 1000000; // in ms
  end = start + block_duration;

  for (i = 0; i < block->NumberFrames(); i++) {
    DataBuffer &data = block->GetBuffer(i);
    switch (track->type) {
      case TYPEAVI:
        AVI_write_frame(track->avi, (char *)data.Buffer(), data.Size(),
                        has_ref ? 0 : 1);
        if (iabs(block_duration - (int64_t)(1000.0 / track->v_fps)) > 1) {
          int nfr = irnd(block_duration * track->v_fps / 1000.0);
          for (k = 2; k <= nfr; k++)
            AVI_write_frame(track->avi, "", 0, 0);
        }
        break;

      case TYPEOGM:
        if (track->buffered_data != NULL) {
          ogg_packet op;

          op.b_o_s = 0;
          op.e_o_s = 0;
          op.packetno = track->packetno;
          op.packet = track->buffered_data;
          op.bytes = track->buffered_size;
          op.granulepos = start * (int64_t)track->a_sfreq / 1000;
          ogg_stream_packetin(&track->osstate, &op);
          safefree(track->buffered_data);

          write_ogg_pages(*track);

          track->packetno++;
        }

        track->buffered_data = (unsigned char *)safememdup(data.Buffer(),
                                                           data.Size());
        track->buffered_size = data.Size();
        track->last_end = end;

        break;

      case TYPESRT:
        if ((end == start) && !track->warning_printed) {
          mxwarn("Subtitle track %lld is missing some duration elements. "
                 "Please check the resulting SRT file for entries that "
                 "have the same start and end time.\n", track->tid);
          track->warning_printed = true;
        }

        buffer[0] = *data.Buffer();
        if ((data.Size() < 2) &&
            ((buffer[0] != ' ') || (buffer[0] != 0) || !iscr(buffer[0])))
          break;

        // Do the charset conversion.
        s = (char *)safemalloc(data.Size() + 1);
        memcpy(s, data.Buffer(), data.Size());
        s[data.Size()] = 0;
        s2 = from_utf8(track->conv_handle, s);
        safefree(s);
        len = strlen(s2);
        s = (char *)safemalloc(len + 3);
        strcpy(s, s2);
        safefree(s2);
        s[len] = '\n';
        s[len + 1] = '\n';
        s[len + 2] = 0;

        // Print the entry's number.
        mxprints(buffer, "%d\n", track->srt_num);
        track->srt_num++;
        track->out->write(buffer, strlen(buffer));

        // Print the timestamps.
        mxprints(buffer, "%02lld:%02lld:%02lld,%03lld --> %02lld:%02lld:"
                 "%02lld,%03lld\n",
                 start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                 (start / 1000) % 60, start % 1000,
                 end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                 (end / 1000) % 60, end % 1000);
        track->out->write(buffer, strlen(buffer));

        // Print the text itself.
        track->out->puts_unl(s);
        safefree(s);
        break;

      case TYPESSA:
        if ((end == start) && !track->warning_printed) {
          mxwarn("Subtitle track %lld is missing some duration elements. "
                 "Please check the resulting SSA/ASS file for entries that "
                 "have the same start and end time.\n", track->tid);
          track->warning_printed = true;
        }

        s = (char *)safemalloc(data.Size() + 1);
        memcpy(s, data.Buffer(), data.Size());
        s[data.Size()] = 0;
        
        // Split the line into the fields.
        // Specs say that the following fields are to put into the block:
        // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
        // 6: MarginV, 7: Effect, 8: Text
        fields = split(s, ",", 9);
        safefree(s);
        if (fields.size() != 9) {
          mxwarn("Invalid format for a SSA line ('%s'). Ignoring this entry."
                 "\n", s);
          continue;
        }

        // Convert the ReadOrder entry so that we can re-order the entries
        // later.
        if (!parse_int(fields[0].c_str(), num)) {
          mxwarn("Invalid format for a SSA line ('%s'). "
                 "Ignoring this entry.\n", s);
          continue;
        }

        // Reconstruct the 'original' line. It'll look like this for SSA:
        // Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect,
        // Text

        line = string("Dialogue: Marked=0,");

        // Append the start and end time.
        mxprints(buffer, "%lld:%02lld:%02lld.%02lld",
                 start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                 (start / 1000) % 60, (start % 1000) / 10);
        line += string(buffer) + comma;

        mxprints(buffer, "%lld:%02lld:%02lld.%02lld",
                 end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                 (end / 1000) % 60, (end % 1000) / 10);
        line += string(buffer) + comma;

        // Append the other fields.
        line += fields[2] + comma + // Style
          fields[3] + comma +   // Name
          fields[4] + comma +   // MarginL
          fields[5] + comma +   // MarginR
          fields[6] + comma +   // MarginV
          fields[7] + comma;    // Effect

        // Do the charset conversion.
        line += fields[8] + "\n";
        from_utf8(track->conv_handle, line);

        // Now store that entry.
        ssa_line.num = num;
        ssa_line.line = safestrdup(line.c_str());
        track->ssa_lines.push_back(ssa_line);

        break;

      case TYPEAAC:
        // Recreate the ADTS headers. What a fun. Like runing headlong into
        // a solid wall. But less painful. Well such is life, you know.
        // But then again I've just seen a beautiful girl walking by my
        // window, and suddenly the world is a bright place. Everything's
        // a matter of perspective. And if I didn't enjoy writing even this
        // code then I wouldn't do it at all. So let's get to it!

        memset(adts, 0, 56 / 8);

        // sync word, 12 bits
        adts[0] = 0xff;
        adts[1] = 0xf0;

        // ID, 1 bit
        adts[1] |= track->aac_id << 3;
        // layer: 2 bits = 00

        // protection absent: 1 bit = 1 (ASSUMPTION!)
        adts[1] |= 1;

        // profile, 2 bits
        adts[2] = track->aac_profile << 6;

        // sampling frequency index, 4 bits
        adts[2] |= track->aac_srate_idx << 2;

        // private, 1 bit = 0 (ASSUMPTION!)

        // channels, 3 bits
        adts[2] |= (track->a_channels & 4) >> 2;
        adts[3] |= (track->a_channels & 3) << 6;

        // original/copy, 1 bit = 0(ASSUMPTION!)

        // home, 1 bit = 0 (ASSUMPTION!)

        // copyright id bit, 1 bit = 0 (ASSUMPTION!)

        // copyright id start, 1 bit = 0 (ASSUMPTION!)

        // frame length, 13 bits
        len = data.Size() + 7;
        adts[3] |= len >> 11;
        adts[4] = (len >> 3) & 0xff;
        adts[5] = (len & 7) << 5;

        // adts buffer fullness, 11 bits, 0x7ff = VBR (ASSUMPTION!)
        adts[5] |= 0x1f;
        adts[6] = 0xfc;

        // number of raw frames, 2 bits, 0 (meaning 1 frame) (ASSUMPTION!)

        // Write the ADTS header and the data itself.
        track->out->write(adts, 56 / 8);
        track->out->write(data.Buffer(), data.Size());

        break;

      case TYPEFLAC:
        if (track->embed_in_ogg) {
          if (track->buffered_data != NULL) {
            ogg_packet op;

            op.b_o_s = 0;
            op.e_o_s = 0;
            op.packetno = track->packetno;
            op.packet = track->buffered_data;
            op.bytes = track->buffered_size;
            op.granulepos = start * (int64_t)track->a_sfreq / 1000;
            ogg_stream_packetin(&track->osstate, &op);
            safefree(track->buffered_data);

            write_ogg_pages(*track);
            track->packetno++;
          }

          track->buffered_data = (unsigned char *)safememdup(data.Buffer(),
                                                             data.Size());
          track->buffered_size = data.Size();
          track->last_end = end;

        } else {
          track->out->write(data.Buffer(), data.Size());
          track->bytes_written += data.Size();
        }

        break;

      case TYPEREAL:
        if (track->track_type == 'v') {
          unpack_real_video_frames(track, data.Buffer(), data.Size(), start,
                                   !has_ref);
          break;
        }

        rmf_frame = rmff_allocate_frame(data.Size(), data.Buffer());
        if (rmf_frame == NULL)
          mxerror("Could not allocate memory for a RealAudio frame.\n");
        rmf_frame->timecode = start;
        if (!has_ref)
          rmf_frame->flags = RMFF_FRAME_FLAG_KEYFRAME;
        rmff_write_frame(track->rmtrack, rmf_frame);
        rmff_release_frame(rmf_frame);
        break;

      default:
        track->out->write(data.Buffer(), data.Size());
        track->bytes_written += data.Size();

    }
  }

  delete block;
}

// }}}

// {{{ FUNCTION close_files()

static void
close_files() {
  int i, k;
  ogg_packet op;

  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      switch (tracks[i].type) {
        case TYPEAVI:
          AVI_close(tracks[i].avi);
          break;

        case TYPEOGM:
          // Set the "end of stream" marker on the last packet, handle it
          // and flush all remaining Ogg pages.
          op.b_o_s = 0;
          op.e_o_s = 1;
          op.packetno = tracks[i].packetno;
          op.packet = tracks[i].buffered_data;
          op.bytes = tracks[i].buffered_size;
          op.granulepos = tracks[i].last_end * (int64_t)tracks[i].a_sfreq /
            1000;
          ogg_stream_packetin(&tracks[i].osstate, &op);
          safefree(tracks[i].buffered_data);

          flush_ogg_pages(tracks[i]);

          delete tracks[i].out;

          break;

        case TYPEFLAC:
          if (tracks[i].embed_in_ogg) {
            // Set the "end of stream" marker on the last packet, handle it
            // and flush all remaining Ogg pages.
            op.b_o_s = 0;
            op.e_o_s = 1;
            op.packetno = tracks[i].packetno;
            op.packet = tracks[i].buffered_data;
            op.bytes = tracks[i].buffered_size;
            op.granulepos = tracks[i].last_end * (int64_t)tracks[i].a_sfreq /
              1000;
            ogg_stream_packetin(&tracks[i].osstate, &op);
            safefree(tracks[i].buffered_data);

            flush_ogg_pages(tracks[i]);
          }

          delete tracks[i].out;

          break;

        case TYPEWAV:
          // Fix the header with the real number of bytes written.
          tracks[i].out->setFilePointer(0);
          tracks[i].wh.riff.len = tracks[i].bytes_written + 36;
          tracks[i].wh.data.len = tracks[i].bytes_written;
          tracks[i].out->write(&tracks[i].wh, sizeof(wave_header));
          delete tracks[i].out;

          break;

        case TYPESSA:
          // Sort the SSA lines according to their ReadOrder number and
          // write them.
          sort(tracks[i].ssa_lines.begin(), tracks[i].ssa_lines.end());
          for (k = 0; k < tracks[i].ssa_lines.size(); k++) {
            tracks[i].out->puts_unl(tracks[i].ssa_lines[k].line);
            safefree(tracks[i].ssa_lines[k].line);
          }
          delete tracks[i].out;

          break;

        default:
          delete tracks[i].out;
      }
    }
  }

  close_rmff_files();
}

// }}}

// {{{ FUNCTION extract_tracks()

bool
extract_tracks(const char *file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  KaxBlock *block;
  kax_track_t *track;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size;
  char kax_track_type;
  bool ms_compat, delete_element, has_reference;
  int64_t block_duration;
  mm_io_c *in;

  block = NULL;
  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error("Error: Couldn't open input file %s (%s).", file_name,
               strerror(errno));
    return false;
  }

  in->setFilePointer(0, seek_end);
  file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error("Error: No EBML head found.");
      delete es;

      return false;
    }
      
    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error("No segment/level 0 element found.");
        return false;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, "Segment");
        break;
      }

      show_element(l0, 0, "Next level 0 element is not a segment but %s",
                   typeid(*l0).name());

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        show_element(l1, 1, "Segment information");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            show_element(l2, 2, "Timecode scale: %llu", tc_scale);
          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        show_element(l1, 1, "Segment tracks");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            show_element(l2, 2, "A track");

            track = NULL;
            kax_track_type = '?';
            ms_compat = false;

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while ((l3 != NULL) && (upper_lvl_el <= 0)) {

              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                char *msg;

                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                if ((track = find_track(uint32(tnum))) == NULL)
                  msg = "extraction not requested";
                else {
                  msg = "will extract this track";
                  track->in_use = true;
                }
                show_element(l3, 3, "Track number: %u (%s)", uint32(tnum),
                             msg);

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());

                switch (uint8(ttype)) {
                  case track_audio:
                    kax_track_type = 'a';
                    break;
                  case track_video:
                    kax_track_type = 'v';
                    break;
                  case track_subtitle:
                    kax_track_type = 's';
                    break;
                  default:
                    kax_track_type = '?';
                    break;
                }
                show_element(l3, 3, "Track type: %s",
                             kax_track_type == 'a' ? "audio" :
                             kax_track_type == 'v' ? "video" :
                             kax_track_type == 's' ? "subtitles" :
                             "unknown");
                if (track != NULL)
                  track->track_type = kax_track_type;

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                show_element(l3, 3, "Audio track");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while ((l4 != NULL) && (upper_lvl_el <= 0)) {

                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    show_element(l4, 4, "Sampling frequency: %f",
                                 float(freq));
                    if (track != NULL)
                      track->a_sfreq = float(freq);

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    show_element(l4, 4, "Channels: %u", uint8(channels));
                    if (track != NULL)
                      track->a_channels = uint8(channels);

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    show_element(l4, 4, "Bit depth: %u", uint8(bps));
                    if (track != NULL)
                      track->a_bps = uint8(bps);
                  } else
                    l4->SkipData(*es, l4->Generic().Context);

                  if (!in_parent(l3)) {
                    delete l4;
                    break;
                  }

                  if (upper_lvl_el < 0) {
                    upper_lvl_el++;
                    if (upper_lvl_el < 0)
                      break;

                  }

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true);

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                show_element(l3, 3, "Video track");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while ((l4 != NULL) && (upper_lvl_el <= 0)) {

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel width: %u", uint16(width));
                    if (track != NULL)
                      track->v_width = uint16(width);

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel height: %u", uint16(height));
                    if (track != NULL)
                      track->v_height = uint16(height);

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    show_element(l4, 4, "Frame rate: %f", float(framerate));
                    if (track != NULL)
                      track->v_fps = float(framerate);

                  } else
                    l4->SkipData(*es, l4->Generic().Context);

                  if (!in_parent(l3)) {
                    delete l4;
                    break;
                  }

                  if (upper_lvl_el < 0) {
                    upper_lvl_el++;
                    if (upper_lvl_el < 0)
                      break;

                  }

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true);

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                show_element(l3, 3, "Codec ID: %s", string(codec_id).c_str());
                if ((!strcmp(string(codec_id).c_str(), MKV_V_MSCOMP) &&
                     (kax_track_type == 'v')) ||
                    (!strcmp(string(codec_id).c_str(), MKV_A_ACM) &&
                     (kax_track_type == 'a')))
                  ms_compat = true;
                if (track != NULL)
                  track->codec_id = safestrdup(string(codec_id).c_str());

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                char pbuffer[100];
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
#if LIBEBML_VERSION >= 000603
                c_priv.ReadData(es->I_O(), SCOPE_ALL_DATA);
#else
                c_priv.ReadData(es->I_O());
#endif
                if (ms_compat && (kax_track_type == 'v') &&
                    (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
                  alBITMAPINFOHEADER *bih =
                    (alBITMAPINFOHEADER *)&binary(c_priv);
                  unsigned char *fcc = (unsigned char *)&bih->bi_compression;
                  mxprints(pbuffer, " (FourCC: %c%c%c%c, 0x%08x)",
                           fcc[0], fcc[1], fcc[2], fcc[3],
                           get_uint32(&bih->bi_compression));
                } else if (ms_compat && (kax_track_type == 'a') &&
                           (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
                  alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)&binary(c_priv);
                  mxprints(pbuffer, " (format tag: 0x%04x)",
                           get_uint16(&wfe->w_format_tag));
                } else
                  pbuffer[0] = 0;
                show_element(l3, 3, "CodecPrivate, length %llu%s",
                             c_priv.GetSize(), pbuffer);
                if (track != NULL) {
                  track->private_data = safememdup(&binary(c_priv),
                                                   c_priv.GetSize());
                  track->private_size = c_priv.GetSize();
                }

              } else if (EbmlId(*l3) ==
                         KaxTrackDefaultDuration::ClassInfos.GlobalId) {
                KaxTrackDefaultDuration &def_duration =
                  *static_cast<KaxTrackDefaultDuration*>(l3);
                def_duration.ReadData(es->I_O());
                show_element(l3, 3, "Default duration: %.3fms (%.3f fps for "
                             "a video track)",
                             (float)uint64(def_duration) / 1000000.0,
                             1000000000.0 / (float)uint64(def_duration));
                if (track != NULL) {
                  track->v_fps = 1000000000.0 / (float)uint64(def_duration);
                  track->default_duration = uint64(def_duration) / 1000000;
                }

              } else
                l3->SkipData(*es, l3->Generic().Context);

              if (!in_parent(l2)) {
                delete l3;
                break;
              }

              if (upper_lvl_el > 0) {
                upper_lvl_el--;
                if (upper_lvl_el > 0)
                  break;
                delete l3;
                l3 = l4;
                continue;

              } else if (upper_lvl_el < 0) {
                upper_lvl_el++;
                if (upper_lvl_el < 0)
                  break;

              }

              l3->SkipData(*es, l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true);

            }

          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el > 0) {
            upper_lvl_el--;
            if (upper_lvl_el > 0)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        } // while (l2 != NULL)

        // Headers have been parsed completely. Now create the output files
        // and stuff.
        create_output_files();

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        show_element(l1, 1, "Cluster");
        cluster = (KaxCluster *)l1;

        if (verbose == 0)
          mxinfo("progress: %d%%\r", (int)(in->getFilePointer() * 100 /
                                           file_size));

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            show_element(l2, 2, "Cluster timecode: %.3fs", 
                         (float)cluster_tc * (float)tc_scale / 1000000000.0);
            cluster->InitTimecode(cluster_tc, tc_scale);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            show_element(l2, 2, "Block group");

            block_duration = -1;
            has_reference = false;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, false, 1);
            while ((l3 != NULL) && (upper_lvl_el <= 0)) {

              delete_element = true;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                block = (KaxBlock *)l3;
                block->ReadData(es->I_O());
                block->SetParent(*cluster);
                show_element(l3, 3, "Block (track number %u, %d frame(s), "
                             "timecode %.3fs)", block->TrackNum(),
                             block->NumberFrames(),
                             (float)block->GlobalTimecode() / 1000000000.0);
                delete_element = false;

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());
                show_element(l3, 3, "Block duration: %.3fms",
                             ((float)uint64(duration)) * tc_scale / 1000000.0);
                block_duration = uint64(duration) * tc_scale / 1000000;

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &reference =
                  *static_cast<KaxReferenceBlock *>(l3);
                reference.ReadData(es->I_O());
                show_element(l3, 3, "Reference block: %.3fms", 
                             ((float)int64(reference)) * tc_scale / 1000000.0);

                has_reference = true;
              } else
                l3->SkipData(*es, l3->Generic().Context);

              if (!in_parent(l2)) {
                if (delete_element)
                  delete l3;
                break;
              }

              if (upper_lvl_el < 0) {
                upper_lvl_el++;
                if (upper_lvl_el < 0)
                  break;

              }

              l3->SkipData(*es, l3->Generic().Context);
              if (delete_element)
                delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true);

            } // while (l3 != NULL)

            // Now write the stuff to the file. Or not. Or get something to
            // drink. Or read a good book. Dunno, your choice, really.
            handle_data(block, block_duration, has_reference);

          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el > 0) {
            upper_lvl_el--;
            if (upper_lvl_el > 0)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        } // while (l2 != NULL)

      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    // Now just close the files and go to sleep. Mummy will sing you a
    // lullaby. Just close your eyes, listen to her sweet voice, singing,
    // singing, fading... fad... ing...
    close_files();

    return true;
  } catch (exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;

    return false;
  }
}

// }}}
