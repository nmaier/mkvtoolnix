/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_matroska.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_matroska.cpp,v 1.47 2003/06/12 23:05:49 mosu Exp $
    \brief Matroska reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exception>
#include <typeinfo>

extern "C" {                    // for BITMAPINFOHEADER
#include "avilib.h"
}

#include "matroska.h"
#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "r_matroska.h"
#include "p_vorbis.h"
#include "p_video.h"
#include "p_pcm.h"
#include "p_textsubs.h"
#include "p_mp3.h"
#include "p_ac3.h"
#include "p_dts.h"
#include "p_aac.h"

#include "EbmlContexts.h"
#include "EbmlHead.h"
#include "EbmlStream.h"
#include "EbmlSubHead.h"
#include "EbmlVoid.h"
#include "FileKax.h"

#include "KaxBlock.h"
#include "KaxBlockData.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxContexts.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTracks.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "StdIOCallback.h"

using namespace std;
using namespace LIBMATROSKA_NAMESPACE;

#define is_ebmlvoid(e) (EbmlId(*e) == EbmlVoid::ClassInfos.GlobalId)

/*
 * Probes a file by simply comparing the first four bytes to the EBML
 * head signature.
 */
int mkv_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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
  if ((data[0] != 0x1A) || (data[1] != 0x45) ||
      (data[2] != 0xDF) || (data[3] != 0xA3))
    return 0;
  return 1;
}

mkv_reader_c::mkv_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  tracks = NULL;
  num_tracks = 0;
  num_buffers = 0;
  buffers = NULL;

  segment_duration = 0.0;
  first_timecode = -1.0;

  if (!read_headers())
    throw error_c("matroska_reader: Failed to read the headers.");

  create_packetizers();
}

mkv_reader_c::~mkv_reader_c() {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i] != NULL) {
      if (tracks[i]->private_data != NULL)
        safefree(tracks[i]->private_data);
      safefree(tracks[i]);
    }

  if (es != NULL)
    delete es;
  if (saved_l1 != NULL)
    delete saved_l1;
  if (in != NULL)
    delete in;
  if (segment != NULL)
    delete segment;
}

int mkv_reader_c::packets_available() {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->ok && (!tracks[i]->packetizer->packet_available()))
      return 0;

  if (num_tracks == 0)
    return 0;

  return 1;
}

mkv_track_t *mkv_reader_c::new_mkv_track() {
  mkv_track_t *t;

  t = (mkv_track_t *)safemalloc(sizeof(mkv_track_t));
  memset(t, 0, sizeof(mkv_track_t));
  tracks = (mkv_track_t **)saferealloc(tracks, (num_tracks + 1) *
                                       sizeof(mkv_track_t *));
  tracks[num_tracks] = t;
  num_tracks++;

  // Set some default values.
  t->default_track = 1;

  return t;
}

mkv_track_t *mkv_reader_c::find_track_by_num(uint32_t n, mkv_track_t *c) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if ((tracks[i] != NULL) && (tracks[i]->tnum == n) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

mkv_track_t *mkv_reader_c::find_track_by_uid(uint32_t uid, mkv_track_t *c) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if ((tracks[i] != NULL) && (tracks[i]->tuid == uid) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

void mkv_reader_c::verify_tracks() {
  int tnum, i;
  unsigned char *c;
  uint32_t u, offset, length;
  mkv_track_t *t;
  BITMAPINFOHEADER *bih;
  WAVEFORMATEX *wfe;

  for (tnum = 0; tnum < num_tracks; tnum++) {
    t = tracks[tnum];
    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_V_MSCOMP)) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(BITMAPINFOHEADER))) {
            if (verbose)
              printf("matroska_reader: WARNING: CodecID for track %u is '"
                     MKV_V_MSCOMP
                     "', but there was no BITMAPINFOHEADER struct present. "
                     "Therefore we don't have a FourCC to identify the video "
                     "codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            bih = (BITMAPINFOHEADER *)t->private_data;
            
            u = get_uint32(&bih->bi_width);
            if (t->v_width != u) {
              if (verbose)
                printf("matroska_reader: WARNING: (MS compatibility mode, "
                       "track %u) Matrosa says video width is %u, but the "
                       "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_width, u);
              if (t->v_width == 0)
                t->v_width = u;
            }

            u = get_uint32(&bih->bi_height);
            if (t->v_height != u) {
              if (verbose)
                printf("matroska_reader: WARNING: (MS compatibility mode, "
                       "track %u) Matrosa video height is %u, but the "
                       "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_height, u);
              if (t->v_height == 0)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->bi_compression, 4);

            if (t->v_frate == 0.0) {
              if (verbose)
                printf("matroska_reader: ERROR: (MS compatibility mode, track "
                       "%u) No VideoFrameRate element was found.\n", t->tnum);
              continue;
            }
          }
        } else {
          if (verbose)
            printf("matroska_reader: Native CodecIDs for video tracks are not "
                   "supported yet (track %u).\n", t->tnum);
          continue;
        }

        if (t->v_width == 0) {
          if (verbose)
            printf("matroska_reader: The width for track %u was not set.\n",
                   t->tnum);
          continue;
        }
        if (t->v_height == 0) {
          if (verbose)
            printf("matroska_reader: The height for track %u was not set.\n",
                   t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 'a':                 // audio track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_A_ACM)) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(WAVEFORMATEX))) {
            if (verbose)
              printf("matroska_reader: WARNING: CodecID for track %u is '"
                     MKV_A_ACM "', but there was no WAVEFORMATEX struct "
                     "present. Therefore we don't have a format ID to "
                     "identify the audio codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            wfe = (WAVEFORMATEX *)t->private_data;
            u = get_uint32(&wfe->n_samples_per_sec);
            if (((uint32_t)t->a_sfreq) != u) {
              if (verbose)
                printf("matroska_reader: WARNING: (MS compatibility mode for "
                       "track %u) Matroska says that there are %u samples per "
                       "second, but WAVEFORMATEX says that there are %u.\n",
                       t->tnum, (uint32_t)t->a_sfreq, u);
              if (t->a_sfreq == 0.0)
                t->a_sfreq = (float)u;
            }

            u = get_uint16(&wfe->n_channels);
            if (t->a_channels != u) {
              if (verbose)
                printf("matroska_reader: WARNING: (MS compatibility mode for "
                       "track %u) Matroska says that there are %u channels, "
                       "but the WAVEFORMATEX says that there are %u.\n",
                       t->tnum, t->a_channels, u);
              if (t->a_channels == 0)
                t->a_channels = u;
            }

            u = get_uint16(&wfe->w_bits_per_sample);
            if (t->a_bps != u) {
              if (verbose)
                printf("matroska_reader: WARNING: (MS compatibility mode for "
                       "track %u) Matroska says that there are %u bits per "
                       "sample, but the WAVEFORMATEX says that there are %u."
                       "\n", t->tnum, t->a_bps, u);
              if (t->a_bps == 0)
                t->a_bps = u;
            }

            t->a_formattag = get_uint16(&wfe->w_format_tag);
          }
        } else {
          if (!strcmp(t->codec_id, MKV_A_MP3))
            t->a_formattag = 0x0055;
          else if (!strcmp(t->codec_id, MKV_A_AC3))
            t->a_formattag = 0x2000;
          else if (!strcmp(t->codec_id, MKV_A_DTS))
            t->a_formattag = 0x2001;
          else if (!strcmp(t->codec_id, MKV_A_PCM))
            t->a_formattag = 0x0001;
          else if (!strcmp(t->codec_id, MKV_A_VORBIS)) {
            if (t->private_data == NULL) {
              if (verbose)
                printf("matroska_reader: WARNING: CodecID for track %u is "
                       "'A_VORBIS', but there are no header packets present.",
                       t->tnum);
              continue;
            }

            c = (unsigned char *)t->private_data;
            if (c[0] != 2) {
              if (verbose)
                printf("matroska_reader: Vorbis track does not contain valid "
                       "headers.\n");
              continue;
            }

            offset = 1;
            for (i = 0; i < 2; i++) {
              length = 0;
              while ((c[offset] == (unsigned char )255) &&
                     (length < t->private_size)) {
                length += 255;
                offset++;
              }
              if (offset >= (t->private_size - 1)) {
                if (verbose)
                  printf("matroska_reader: Vorbis track does not contain valid"
                         " headers.\n");
                continue;
              }
              length += c[offset];
              offset++;
              t->header_sizes[i] = length;
            }

            t->headers[0] = &c[offset];
            t->headers[1] = &c[offset + t->header_sizes[0]];
            t->headers[2] = &c[offset + t->header_sizes[0] +
                               t->header_sizes[1]];
            t->header_sizes[2] = t->private_size - offset -
              t->header_sizes[0] - t->header_sizes[1];

            t->a_formattag = 0xFFFE;
          } else if (!strcmp(t->codec_id, MKV_A_AAC_2MAIN) ||
                     !strcmp(t->codec_id, MKV_A_AAC_2LC) ||
                     !strcmp(t->codec_id, MKV_A_AAC_2SSR) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4MAIN) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4LC) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4SSR) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4LTP) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4SBR))
            t->a_formattag = FOURCC('M', 'P', '4', 'A');
          else {
            if (verbose)
              printf("matroska_reader: Unknown/unsupported audio codec ID '%s'"
                     " for track %u.\n", t->codec_id, t->tnum);
            continue;
          }
        }

        if (t->a_sfreq == 0.0) {
          if (verbose)
            printf("matroska_reader: The sampling frequency was not set for "
                   "track %u.\n", t->tnum);
          continue;
        }

        if (t->a_channels == 0) {
          if (verbose)
            printf("matroska_reader: The number of channels was not set for "
                   "track %u.\n", t->tnum);
          continue;
        }

        if (t->a_formattag == 0) {
          if (verbose)
            printf("matroska_reader: The audio format tag was not set for "
                   "track %u.\n", t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 's':                 // Text subtitles do not need any data
        t->ok = 1;              // except the CodecID.
        break;

      default:                  // unknown track type!? error in demuxer...
        if (verbose)
          printf("matroska_reader: Error: matroska_reader: unknown demuxer "
                 "type for track %u: '%c'\n", t->tnum, t->type);
        continue;
    }

    if (t->ok && (verbose > 1))
      printf("matroska_reader: Track %u seems to be ok.\n", t->tnum);
  }
}

int mkv_reader_c::read_headers() {
  int upper_lvl_el, exit_loop;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  mkv_track_t *track;

  try {
    // Create the interface between MPlayer's IO system and
    // libmatroska's IO system.
    in = new StdIOCallback(ti->fname, MODE_READ);
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      fprintf(stderr, "Error: matroska_reader: no EBML head found.\n");
      return 0;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;
    if (verbose > 1)
      fprintf(stdout, "matroska_reader: Found the head...\n");

    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      if (verbose)
        fprintf(stdout, "matroska_reader: No segment found.\n");
      return 0;
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      if (verbose)
        fprintf(stdout, "matroska_reader: No segment found.\n");
      return 0;
    }
    if (verbose > 1)
      fprintf(stdout, "matroska_reader: + a segment...\n");

    segment = l0;

    upper_lvl_el = 0;
    exit_loop = 0;
    tc_scale = TIMECODE_SCALE;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while (l1 != NULL) {
      if ((upper_lvl_el != 0) || exit_loop)
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        if (verbose > 1)
          fprintf(stdout, "matroska_reader: |+ segment information...\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if ((upper_lvl_el != 0) || exit_loop)
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            if (verbose > 1)
              fprintf(stdout, "matroska_reader: | + timecode scale: %llu\n",
                      tc_scale);

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());

            segment_duration = float(duration) * tc_scale / 1000000000.0;
            if (verbose > 1)
              fprintf(stdout, "matroska_reader: | + duration: %.3fs\n",
                      segment_duration);

          }

          if (upper_lvl_el > 0) {  // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(*es, l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        if (verbose > 1)
          fprintf(stdout, "matroska_reader: |+ segment tracks...\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if ((upper_lvl_el != 0) || exit_loop)
            break;

          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            if (verbose > 1)
              fprintf(stdout, "matroska_reader: | + a track...\n");

            track = new_mkv_track();
            if (track == NULL)
              return 0;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Track number: %d\n",
                          uint8(tnum));
                track->tnum = uint8(tnum);
                if (find_track_by_num(track->tnum, track) != NULL)
                  if (verbose > 1)
                    fprintf(stdout, "matroska_reader: |  + WARNING: There's "
                            "more than one track with the number %u.\n",
                            track->tnum);

              } else if (EbmlId(*l3) == KaxTrackUID::ClassInfos.GlobalId) {
                KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
                tuid.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Track UID: %u\n",
                          uint32(tuid));
                track->tuid = uint32(tuid);
                if (find_track_by_uid(track->tuid, track) != NULL)
                  if (verbose > 1)
                    fprintf(stdout, "matroska_reader: |  + WARNING: There's "
                            "more than one track with the UID %u.\n",
                            track->tnum);

              } else if (EbmlId(*l3) ==
                         KaxTrackDefaultDuration::ClassInfos.GlobalId) {
                KaxTrackDefaultDuration &def_duration =
                  *static_cast<KaxTrackDefaultDuration *>(l3);
                def_duration.ReadData(es->I_O());
                track->v_frate = 1000000000.0 / (float)uint64(def_duration);
                if (verbose > 1) 
                  fprintf(stdout, "matroska_reader: |  + Default duration: "
                          "%.3fms ( = %.3f fps)\n",
                          (float)uint64(def_duration) / 1000000.0,
                          track->v_frate);

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Track type: ");

                switch (uint8(ttype)) {
                  case track_audio:
                    if (verbose > 1)
                      printf("Audio\n");
                    track->type = 'a';
                    break;
                  case track_video:
                    if (verbose > 1)
                      printf("Video\n");
                    track->type = 'v';
                    break;
                  case track_subtitle:
                    if (verbose > 1)
                      printf("Subtitle\n");
                    track->type = 's';
                    break;
                  default:
                    if (verbose > 1)
                      printf("unknown\n");
                    track->type = '?';
                    break;
                }

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Audio track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    track->a_sfreq = float(freq);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Sampling "
                              "frequency: %f\n", track->a_sfreq);

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    track->a_channels = uint8(channels);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Channels: %u\n",
                              track->a_channels);

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    track->a_bps = uint8(bps);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Bit depth: %u\n",
                              track->a_bps);

                  } else if (!is_ebmlvoid(l4))
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + unknown "
                              "element@4: %s\n", typeid(*l4).name());

                  if (upper_lvl_el > 0) {
                    assert(1 == 0);  // this should never happen
                  } else {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Video track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_width = uint16(width);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Pixel width: "
                              "%u\n", track->v_width);

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_height = uint16(height);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Pixel height: "
                              "%u\n", track->v_height);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayWidth::ClassInfos.GlobalId) {
                    KaxVideoDisplayWidth &width =
                      *static_cast<KaxVideoDisplayWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_dwidth = uint16(width);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Display width: "
                              "%u\n", track->v_dwidth);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayHeight::ClassInfos.GlobalId) {
                    KaxVideoDisplayHeight &height =
                      *static_cast<KaxVideoDisplayHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_dheight = uint16(height);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Display height: "
                              "%u\n", track->v_dheight);

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    // For compatibility with older files.
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    track->v_frate = float(framerate);
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + Frame rate: "
                              "%f\n", float(framerate));

                  } else if (!is_ebmlvoid(l4))
                    if (verbose > 1)
                      fprintf(stdout, "matroska_reader: |   + unknown "
                              "element@4: %s\n", typeid(*l4).name());

                  if (upper_lvl_el > 0) {
                    assert(1 == 0);  // this should never happen
                  } else {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O()); 
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Codec ID: %s\n",
                          string(codec_id).c_str());
                track->codec_id = safestrdup(string(codec_id).c_str());

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + CodecPrivate, length "
                          "%llu\n", c_priv.GetSize());
                track->private_size = c_priv.GetSize();
                if (track->private_size > 0)
                  track->private_data = safememdup(&binary(c_priv),
                                                   track->private_size);

              } else if (EbmlId(*l3) ==
                         KaxTrackMinCache::ClassInfos.GlobalId) {
                KaxTrackMinCache &min_cache =
                  *static_cast<KaxTrackMinCache*>(l3);
                min_cache.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + MinCache: %u\n",
                          uint32(min_cache));

              } else if (EbmlId(*l3) ==
                         KaxTrackMaxCache::ClassInfos.GlobalId) {
                KaxTrackMaxCache &max_cache =
                  *static_cast<KaxTrackMaxCache*>(l3);
                max_cache.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + MaxCache: %u\n",
                          uint32(max_cache));

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagDefault::ClassInfos.GlobalId) {
                KaxTrackFlagDefault &f_default =
                  *static_cast<KaxTrackFlagDefault *>(l3);
                f_default.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Default flag: %d\n",
                          uint32(f_default));
                track->default_track = uint32(f_default);

              } else if (EbmlId(*l3) ==
                         KaxTrackLanguage::ClassInfos.GlobalId) {
                KaxTrackLanguage &lang =
                  *static_cast<KaxTrackLanguage *>(l3);
                lang.ReadData(es->I_O());
                if (verbose > 1)
                  fprintf(stdout, "matroska_reader: |  + Language: %s\n",
                          string(lang).c_str());
                track->language = safestrdup(string(lang).c_str());

              }

              if (upper_lvl_el > 0) {  // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;
              } else {
                l3->SkipData(*es, l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          }

          if (upper_lvl_el > 0) {  // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(*es, l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        if (verbose > 1)
          fprintf(stdout, "matroska_reader: |+ found cluster, headers are "
                  "parsed completely :)\n");
        saved_l1 = l1;
        exit_loop = 1;

      }

      if (exit_loop)      // we've found the first cluster, so get out
        break;

      if (upper_lvl_el > 0) {    // we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(*es, l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
      }
    } // while (l1 != NULL)

  } catch (exception &ex) {
    fprintf(stdout, "Error: matroska_reader: caught exception\n");
    return 0;
  }

  if (!exit_loop)               // We have NOT found a cluster!
    return 0;

  verify_tracks();

  return 1;
}

void mkv_reader_c::create_packetizers() {
  int i;
  mkv_track_t *t;
  track_info_t nti;

  for (i = 0; i < num_tracks; i++) {
    t = tracks[i];

    memcpy(&nti, ti, sizeof(track_info_t));
    nti.private_data = (unsigned char *)t->private_data;
    nti.private_size = t->private_size;
    if (nti.fourcc[0] == 0)
      memcpy(nti.fourcc, t->v_fourcc, 5);
    if (nti.default_track == 0)
      nti.default_track = t->default_track;
    if (nti.language == 0)
      nti.language = t->language;

    if (t->ok && demuxing_requested(t->type, t->tnum)) {
      switch (t->type) {

        case 'v':
          if (nti.fourcc[0] == 0)
            memcpy(nti.fourcc, t->v_fourcc, 5);
          if (verbose)
            fprintf(stdout, "Matroska demultiplexer (%s): using video output "
                    "module for track ID %u.\n", ti->fname, t->tnum);
          t->packetizer = new video_packetizer_c(this, t->v_frate, t->v_width,
                                                 t->v_height, 24, 1, &nti);
          if (nti.aspect_ratio == 1.0) { // The user didn't set it.
            if (t->v_dwidth == 0)
              t->v_dwidth = t->v_width;
            if (t->v_dheight == 0)
              t->v_dheight = t->v_height;
            if ((t->v_dwidth != t->v_width) || (t->v_dheight != t->v_height))
              t->packetizer->set_video_aspect_ratio((float)t->v_dwidth /
                                                    (float)t->v_dheight);
          }
          break;

        case 'a':
          if (t->a_formattag == 0x0001) {
            t->packetizer = new pcm_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, t->a_bps,
                                                 &nti);
            if (verbose)
              fprintf(stdout, "Matroska demultiplexer (%s): using the PCM "
                      "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x0055) {
            t->packetizer = new mp3_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti);
            if (verbose)
              fprintf(stdout, "Matroska demultiplexer (%s): using the MP3 "
                      "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x2000) {
            t->packetizer = new ac3_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti);
            if (verbose)
              fprintf(stdout, "Matroska demultiplexer (%s): using the AC3 "
                      "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x2001) {
            fprintf(stderr, "Reading DTS from Matroska not implemented yet,"
                    "cannot we get a complete DTS_Header here for construction"
                    "of the packetizer?");
            assert(0);
            /*
              t->packetizer = new dts_packetizer_c(this,
                                                   (unsigned long)t->a_sfreq,
                                                   &nti);
            */
          } else if (t->a_formattag == 0xFFFE) {
            t->packetizer = new vorbis_packetizer_c(this,
                                                    t->headers[0],
                                                    t->header_sizes[0],
                                                    t->headers[1],
                                                    t->header_sizes[1],
                                                    t->headers[2],
                                                    t->header_sizes[2], &nti);
            if (verbose)
              fprintf(stdout, "Matroska demultiplexer (%s): using the Vorbis "
                      " output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == FOURCC('M', 'P', '4', 'A')) {
            // A_AAC/MPEG2/MAIN
            // 0123456789012345
            int id, profile;

            if (t->codec_id[10] == '2')
              id = AAC_ID_MPEG2;
            else if (t->codec_id[10] == '4')
              id = AAC_ID_MPEG4;
            else {
              fprintf(stderr, "Error: matroska_reader: Malformed codec id "
                      "%s for track %d.\n", t->codec_id, t->tnum);
              exit(1);
            }
            if (!strcmp(&t->codec_id[12], "MAIN"))
              profile = AAC_PROFILE_MAIN;
            else if (!strcmp(&t->codec_id[12], "LC"))
              profile = AAC_PROFILE_LC;
            else if (!strcmp(&t->codec_id[12], "SSR"))
              profile = AAC_PROFILE_SSR;
            else if (!strcmp(&t->codec_id[12], "LTP"))
              profile = AAC_PROFILE_LTP;
            else {
              fprintf(stderr, "Error: matroska_reader: Malformed codec id "
                      "%s for track %d.\n", t->codec_id, t->tnum);
              exit(1);
            }

            t->packetizer = new aac_packetizer_c(this, id, profile,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti, true);
            if (verbose)
              fprintf(stdout, "Matroska demultiplexer (%s): using the AAC "
                      "output module for track ID %u.\n", ti->fname, t->tnum);
          } else {
            fprintf(stderr, "Error: matroska_reader: Unsupported track type "
                    "for track %d.\n", t->tnum);
            exit(1);
          }
          break;

        default:
          fprintf(stderr, "Error: matroska_reader: Unsupported track type "
                  "for track %d.\n", t->tnum);
          exit(1);
          break;
      }
      if (t->tuid != 0)
        if (!t->packetizer->set_uid(t->tuid))
          fprintf(stdout, "Info: matroska_reader: Could not keep the track "
                  "UID %u because it is already allocated for the new "
                  "file.\n", t->tuid);
    }
  }
}

int mkv_reader_c::read() {
  int upper_lvl_el, exit_loop, found_data, i, delete_element;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  KaxBlock *block;
  int64_t fref, bref, block_duration, block_ref1, block_ref2;
  mkv_track_t *block_track;

  if (num_tracks == 0)
    return 0;

  l0 = segment;

  if (saved_l1 == NULL)         // We're done.
    return 0;

  debug_enter("mkv_reader_c::read");

  exit_loop = 0;
  upper_lvl_el = 0;
  l1 = saved_l1;
  saved_l1 = NULL;
  saved_l2 = NULL;
  delete_element = 1;
  found_data = 0;
  try {
    while (l1 != NULL)  {
      if ((upper_lvl_el != 0) || exit_loop)
        break;

      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {

        cluster = (KaxCluster *)l1;

        if (saved_l2 != NULL) {
          l2 = saved_l2;
          saved_l2 = NULL;
        } else
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if ((found_data >= 1) && packets_available()) {
            saved_l2 = l2;
            saved_l1 = l1;
            exit_loop = 1;
            break;
          }

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            cluster->InitTimecode(cluster_tc, tc_scale);
            if (first_timecode == -1.0)
              first_timecode = (float)cluster_tc * (float)tc_scale /
                1000000.0;

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {

            block_duration = -1;
            block_ref1 = -1;
            block_ref2 = -1;
            block_track = NULL;
            block = NULL;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                block = (KaxBlock *)l3;
                block->ReadData(es->I_O());
                block->SetParent(*cluster);

                block_track = find_track_by_num(block->TrackNum());
                if ((block_track != NULL) &&
                    demuxing_requested(block_track->type, block_track->tnum))
                  delete_element = 0;
                else
                  block = NULL;

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &ref = *static_cast<KaxReferenceBlock *>(l3);
                ref.ReadData(es->I_O());

                if (block_ref1 == -1)
                  block_ref1 = int64(ref);
                else
                  block_ref2 = int64(ref);

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());

                block_duration = (int64_t)uint64(duration);

              } else if (!(EbmlId(*l3) ==
                           KaxBlockVirtual::ClassInfos.GlobalId))
                 printf("matroska_reader:   Uknown element@3: %s\n",
                        typeid(*l3).name());

              l3->SkipData(*es, l3->Generic().Context);
              if (delete_element)
                delete l3;
              else
                delete_element = 1;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)

            if (block != NULL) {
              if ((block_track->type == 't') && (block_duration == -1)) {
                fprintf(stdout, "Warning: Text subtitle block does not "
                        "contain a block duration element. This file is "
                        "broken.\n");
                delete block;

                return EMOREDATA;
              }

              last_timecode = block->GlobalTimecode() / 1000000;

              if (block_ref1 > 0) {
                fref = block_ref1;
                bref = block_ref2;
              } else {
                fref = block_ref2;
                bref = block_ref1;
              }

              for (i = 0; i < (int)block->NumberFrames(); i++) {
                DataBuffer &data = block->GetBuffer(i);
                block_track->packetizer->process((unsigned char *)
                                                 data.Buffer(), data.Size(),
                                                 (int64_t)last_timecode,
                                                 block_duration, bref, fref);
              }
              block_track->units_processed += block->NumberFrames();

              delete block;
            }
          }

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(*es, l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);

          }
        } // while (l2 != NULL)
      }

      if (exit_loop)
        break;

      if (upper_lvl_el > 0) {    // we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(*es, l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
      }
    } // while (l1 != NULL)
  } catch (exception ex) {
    printf("matroska_reader: exception caught\n");
    return 0;
  }

  debug_leave("mkv_reader_c::read");

  if (found_data)
    return EMOREDATA;
  else
    return 0;
}

packet_t *mkv_reader_c::get_packet() {
  generic_packetizer_c *winner;
  mkv_track_t *t;
  int i;

  winner = NULL;

  for (i = 0; i < num_tracks; i++) {
    t = tracks[i];
    if (winner == NULL) {
      if (t->packetizer->packet_available())
        winner = t->packetizer;
    } else if (winner->packet_available() &&
               (winner->get_smallest_timecode() >
                t->packetizer->get_smallest_timecode()))
      winner = t->packetizer;
  }

  if (winner != NULL)
    return winner->get_packet();
  else
    return NULL;
}

int mkv_reader_c::display_priority() {
  int i;

  if (segment_duration != 0.0)
    return DISPLAYPRIORITY_HIGH;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->type == 'v')
      return DISPLAYPRIORITY_MEDIUM;

  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void mkv_reader_c::display_progress() {
  int i;

  if (segment_duration != 0.0) {
    fprintf(stdout, "progress: %.3fs/%.3fs (%d%%)\r",
            (last_timecode - first_timecode) / 1000.0, segment_duration,
            (int)((last_timecode - first_timecode) / 10 / segment_duration));
    fflush(stdout);
    return;
  }

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->type == 'v') {
      fprintf(stdout, "progress: %llu frames\r", tracks[i]->units_processed);
      fflush(stdout);
      return;
    }

  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void mkv_reader_c::set_headers() {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (demuxing_requested(tracks[i]->type, tracks[i]->tnum))
      tracks[i]->packetizer->set_headers();
}

void mkv_reader_c::identify() {
  int i;

  fprintf(stdout, "File '%s': container: Matroska\n", ti->fname);
  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->ok)
      fprintf(stdout, "Track ID %d: %s (%s%s%s)\n", tracks[i]->tnum,
              tracks[i]->type == 'v' ? "video" :
              tracks[i]->type == 'a' ? "audio" :
              tracks[i]->type == 's' ? "subtitles" : "unknown",
              tracks[i]->codec_id,
              tracks[i]->ms_compat ? ", " : "",
              tracks[i]->ms_compat ? tracks[i]->v_fourcc : "");
}
