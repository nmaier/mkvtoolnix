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
    \version $Id$
    \brief Matroska reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

// {{{ includes

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
#include "mm_io.h"
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

#include <ebml/EbmlContexts.h>
#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxContexts.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

using namespace std;
using namespace libmatroska;

// }}}

#define is_ebmlvoid(e) (EbmlId(*e) == EbmlVoid::ClassInfos.GlobalId)

// {{{ FUNCTION kax_reader::probe_file()

/*
 * Probes a file by simply comparing the first four bytes to the EBML
 * head signature.
 */
int kax_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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

// }}}

// {{{ C'TOR

kax_reader_c::kax_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  num_buffers = 0;
  buffers = NULL;

  segment_duration = 0.0;
  first_timecode = -1.0;

  if (!read_headers())
    throw error_c("matroska_reader: Failed to read the headers.");

  create_packetizers();
}

// }}}

// {{{ D'TOR

kax_reader_c::~kax_reader_c() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i] != NULL) {
      if (tracks[i]->packetizer != NULL)
        delete tracks[i]->packetizer;
      safefree(tracks[i]->private_data);
      safefree(tracks[i]->codec_id);
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

// }}}

// {{{ FUNCTIONS packets_available(), new_kax_track(), find_track*

int kax_reader_c::packets_available() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ok && (!tracks[i]->packetizer->packet_available()))
      return 0;

  if (tracks.size() == 0)
    return 0;

  return 1;
}

kax_track_t *kax_reader_c::new_kax_track() {
  kax_track_t *t;

  t = (kax_track_t *)safemalloc(sizeof(kax_track_t));
  memset(t, 0, sizeof(kax_track_t));
  tracks.push_back(t);

  // Set some default values.
  t->default_track = 1;

  return t;
}

kax_track_t *kax_reader_c::find_track_by_num(uint32_t n, kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i] != NULL) && (tracks[i]->tnum == n) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

kax_track_t *kax_reader_c::find_track_by_uid(uint32_t uid, kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i] != NULL) && (tracks[i]->tuid == uid) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

// }}}

// {{{ FUNCTION kax_reader_c::verify_tracks()

void kax_reader_c::verify_tracks() {
  int tnum, i;
  unsigned char *c;
  uint32_t u, offset, length;
  kax_track_t *t;
  alBITMAPINFOHEADER *bih;
  alWAVEFORMATEX *wfe;

  for (tnum = 0; tnum < tracks.size(); tnum++) {
    t = tracks[tnum];
    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_V_MSCOMP)) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(alBITMAPINFOHEADER))) {
            if (verbose)
              mxwarn("matroska_reader: CodecID for track %u is '" MKV_V_MSCOMP
                     "', but there was no BITMAPINFOHEADER struct present. "
                     "Therefore we don't have a FourCC to identify the video "
                     "codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            bih = (alBITMAPINFOHEADER *)t->private_data;
            
            u = get_uint32(&bih->bi_width);
            if (t->v_width != u) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility "
                       "mode, track %u) Matrosa says video width is %u, but "
                       "the BITMAPINFOHEADER says %u.\n", t->tnum, t->v_width,
                       u);
              if (t->v_width == 0)
                t->v_width = u;
            }

            u = get_uint32(&bih->bi_height);
            if (t->v_height != u) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility "
                       "mode, track %u) Matrosa video height is %u, but the "
                       "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_height, u);
              if (t->v_height == 0)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->bi_compression, 4);

            if (t->v_frate == 0.0) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility "
                       "mode, track %u) No VideoFrameRate/DefaultDuration "
                       "element was found.\n", t->tnum);
              continue;
            }
          }
        }

        if (t->v_width == 0) {
          if (verbose)
            mxwarn("matroska_reader: The width for track %u was not set.\n",
                   t->tnum);
          continue;
        }
        if (t->v_height == 0) {
          if (verbose)
            mxwarn("matroska_reader: The height for track %u was not set.\n",
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
              (t->private_size < sizeof(alWAVEFORMATEX))) {
            if (verbose)
              mxwarn("matroska_reader: CodecID for track %u is '"
                     MKV_A_ACM "', but there was no WAVEFORMATEX struct "
                     "present. Therefore we don't have a format ID to "
                     "identify the audio codec used.\n", t->tnum);
            continue;

          } else {
            t->ms_compat = 1;

            wfe = (alWAVEFORMATEX *)t->private_data;
            u = get_uint32(&wfe->n_samples_per_sec);
            if (((uint32_t)t->a_sfreq) != u) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility mode for "
                       "track %u) Matroska says that there are %u samples per"
                       " second, but WAVEFORMATEX says that there are %u.\n",
                       t->tnum, (uint32_t)t->a_sfreq, u);
              if (t->a_sfreq == 0.0)
                t->a_sfreq = (float)u;
            }

            u = get_uint16(&wfe->n_channels);
            if (t->a_channels != u) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility mode for "
                       "track %u) Matroska says that there are %u channels, "
                       "but the WAVEFORMATEX says that there are %u.\n",
                       t->tnum, t->a_channels, u);
              if (t->a_channels == 0)
                t->a_channels = u;
            }

            u = get_uint16(&wfe->w_bits_per_sample);
            if (t->a_bps != u) {
              if (verbose)
                mxwarn("matroska_reader: (MS compatibility mode for "
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
          else if (!strncmp(t->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
            t->a_formattag = 0x2000;
          else if (!strcmp(t->codec_id, MKV_A_DTS))
            t->a_formattag = 0x2001;
          else if (!strcmp(t->codec_id, MKV_A_PCM))
            t->a_formattag = 0x0001;
          else if (!strcmp(t->codec_id, MKV_A_VORBIS)) {
            if (t->private_data == NULL) {
              if (verbose)
                mxwarn("matroska_reader: CodecID for track %u is "
                       "'A_VORBIS', but there are no header packets present.",
                       t->tnum);
              continue;
            }

            c = (unsigned char *)t->private_data;
            if (c[0] != 2) {
              if (verbose)
                mxwarn("matroska_reader: Vorbis track does not "
                       "contain valid headers.\n");
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
                  mxwarn("matroska_reader: Vorbis track does not "
                         "contain valid headers.\n");
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
                     !strcmp(t->codec_id, MKV_A_AAC_2SBR) ||
                     !strcmp(t->codec_id, MKV_A_AAC_4SBR))
            t->a_formattag = FOURCC('M', 'P', '4', 'A');
          else {
            if (verbose)
              mxwarn("matroska_reader: Unknown/unsupported audio "
                     "codec ID '%s' for track %u.\n", t->codec_id, t->tnum);
            continue;
          }
        }

        if (t->a_sfreq == 0.0)
          t->a_sfreq = 8000.0;

        if (t->a_channels == 0)
          t->a_channels = 1;

        if (t->a_formattag == 0) {
          if (verbose)
            mxwarn("matroska_reader: The audio format tag was not "
                   "set for track %u.\n", t->tnum);
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
          mxwarn("matroska_reader: matroska_reader: unknown "
                 "demuxer type for track %u: '%c'\n", t->tnum, t->type);
        continue;
    }

    if (t->ok && (verbose > 1))
      mxinfo("matroska_reader: Track %u seems to be ok.\n", t->tnum);
  }
}

// }}}

// {{{ FUNCTION kax_reader_c::handle_attachments()

void kax_reader_c::handle_attachments(mm_io_c *io, EbmlStream *es,
                                      EbmlElement *l0, int64_t pos) {
  KaxAttachments *atts;
  KaxAttached *att;
  EbmlElement *l1, *l2;
  int upper_lvl_el, i, k;
  string name, type;
  int64_t size, id;
  char *str;
  bool found;
  kax_attachment_t matt;

  io->save_pos(pos);
  l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true);

  if ((l1 != NULL) && (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)) {
    atts = (KaxAttachments *)l1;
    l2 = NULL;
    upper_lvl_el = 0;
    atts->Read(*es, KaxAttachments::ClassInfos.Context, upper_lvl_el, l2,
               true);
    for (i = 0; i < atts->ListSize(); i++) {
      att = (KaxAttached *)(*atts)[i];
      if (EbmlId(*att) == KaxAttached::ClassInfos.GlobalId) {
        name = "";
        type = "";
        size = -1;
        id = -1;

        for (k = 0; k < att->ListSize(); k++) {
          l2 = (*att)[k];

          if (EbmlId(*l2) == KaxFileName::ClassInfos.GlobalId) {
            KaxFileName &fname = *static_cast<KaxFileName *>(l2);
            str = UTFstring_to_cstr(UTFstring(fname));
            name = str;
            safefree(str);

          } else if (EbmlId(*l2) == KaxMimeType::ClassInfos.GlobalId) {
            KaxMimeType &mtype = *static_cast<KaxMimeType *>(l2);
            type = string(mtype);

          } else if (EbmlId(*l2) == KaxFileUID::ClassInfos.GlobalId) {
            KaxFileUID &fuid = *static_cast<KaxFileUID *>(l2);
            id = uint32(fuid);

          } else if (EbmlId(*l2) == KaxFileData::ClassInfos.GlobalId) {
            KaxFileData &fdata = *static_cast<KaxFileData *>(l2);
            size = fdata.GetSize();

          }
        }

        if ((id != -1) && (size != -1) && (type.length() != 0)) {
          found = false;

          for (k = 0; k < attachments.size(); k++)
            if (attachments[k].id == id) {
              found = true;
              break;
            }

          if (!found) {
            matt.name = name;
            matt.type = type;
            matt.size = size;
            matt.id = id;
            attachments.push_back(matt);
          }
        }
      }
    }

    delete l1;
  }

  io->restore_pos();
}

// }}}

// {{{ FUNCTION kax_reader_c::read_headers()

#define in_parent(p) (in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

int kax_reader_c::read_headers() {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  kax_track_t *track;
  bool exit_loop;

  try {
    // Create the interface between MPlayer's IO system and
    // libmatroska's IO system.
    in = new mm_io_c(ti->fname, MODE_READ);
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL) {
      mxwarn("matroska_reader: no EBML head found.\n");
      return 0;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;
    if (verbose > 1)
      mxinfo("matroska_reader: Found the head...\n");

    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL) {
      if (verbose)
        mxwarn("matroska_reader: No segment found.\n");
      return 0;
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      if (verbose)
        mxwarn("matroska_reader: No segment found.\n");
      return 0;
    }
    if (verbose > 1)
      mxinfo("matroska_reader: + a segment...\n");

    segment = l0;

    upper_lvl_el = 0;
    exit_loop = 0;
    tc_scale = TIMECODE_SCALE;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        if (verbose > 1)
          mxinfo("matroska_reader: |+ segment information...\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            if (verbose > 1)
              mxinfo("matroska_reader: | + timecode scale: %llu\n", tc_scale);

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());

            segment_duration = float(duration) * tc_scale / 1000000000.0;
            if (verbose > 1)
              mxinfo("matroska_reader: | + duration: %.3fs\n",
                     segment_duration);

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

        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        if (verbose > 1)
          mxinfo("matroska_reader: |+ segment tracks...\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            if (verbose > 1)
              mxinfo("matroska_reader: | + a track...\n");

            track = new_kax_track();
            if (track == NULL)
              return 0;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while ((l3 != NULL) && (upper_lvl_el <= 0)) {

              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Track number: %d\n",
                         uint8(tnum));
                track->tnum = uint8(tnum);
                if (find_track_by_num(track->tnum, track) != NULL)
                  if (verbose > 1)
                    mxwarn("matroska_reader: |  + There's "
                           "more than one track with the number %u.\n",
                           track->tnum);

              } else if (EbmlId(*l3) == KaxTrackUID::ClassInfos.GlobalId) {
                KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
                tuid.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Track UID: %u\n",
                         uint32(tuid));
                track->tuid = uint32(tuid);
                if (find_track_by_uid(track->tuid, track) != NULL)
                  if (verbose > 1)
                    mxwarn("matroska_reader: |  + There's "
                           "more than one track with the UID %u.\n",
                           track->tnum);

              } else if (EbmlId(*l3) ==
                         KaxTrackDefaultDuration::ClassInfos.GlobalId) {
                KaxTrackDefaultDuration &def_duration =
                  *static_cast<KaxTrackDefaultDuration *>(l3);
                def_duration.ReadData(es->I_O());
                track->v_frate = 1000000000.0 / (float)uint64(def_duration);
                if (verbose > 1) 
                  mxinfo("matroska_reader: |  + Default duration: "
                         "%.3fms ( = %.3f fps)\n",
                         (float)uint64(def_duration) / 1000000.0,
                         track->v_frate);

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Track type: ");

                switch (uint8(ttype)) {
                  case track_audio:
                    if (verbose > 1)
                      mxinfo("Audio\n");
                    track->type = 'a';
                    break;
                  case track_video:
                    if (verbose > 1)
                      mxinfo("Video\n");
                    track->type = 'v';
                    break;
                  case track_subtitle:
                    if (verbose > 1)
                      mxinfo("Subtitle\n");
                    track->type = 's';
                    break;
                  default:
                    if (verbose > 1)
                      mxinfo("unknown\n");
                    track->type = '?';
                    break;
                }

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Audio track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while ((l4 != NULL) && (upper_lvl_el <= 0)) {

                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    track->a_sfreq = float(freq);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Sampling "
                             "frequency: %f\n", track->a_sfreq);

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    track->a_channels = uint8(channels);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Channels: %u\n",
                             track->a_channels);

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    track->a_bps = uint8(bps);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Bit depth: %u\n",
                             track->a_bps);

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
                  l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                           0xFFFFFFFFL, true);

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Video track\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while ((l4 != NULL) && (upper_lvl_el <= 0)) {

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_width = uint16(width);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Pixel width: "
                             "%u\n", track->v_width);

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_height = uint16(height);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Pixel height: "
                             "%u\n", track->v_height);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayWidth::ClassInfos.GlobalId) {
                    KaxVideoDisplayWidth &width =
                      *static_cast<KaxVideoDisplayWidth *>(l4);
                    width.ReadData(es->I_O());
                    track->v_dwidth = uint16(width);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Display width: "
                             "%u\n", track->v_dwidth);

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayHeight::ClassInfos.GlobalId) {
                    KaxVideoDisplayHeight &height =
                      *static_cast<KaxVideoDisplayHeight *>(l4);
                    height.ReadData(es->I_O());
                    track->v_dheight = uint16(height);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Display height: "
                             "%u\n", track->v_dheight);

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    // For compatibility with older files.
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    track->v_frate = float(framerate);
                    if (verbose > 1)
                      mxinfo("matroska_reader: |   + Frame rate: "
                             "%f\n", float(framerate));

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
                  l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                           0xFFFFFFFFL, true);

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O()); 
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Codec ID: %s\n",
                         string(codec_id).c_str());
                track->codec_id = safestrdup(string(codec_id).c_str());

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + CodecPrivate, length "
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
                  mxinfo("matroska_reader: |  + MinCache: %u\n",
                         uint32(min_cache));
                if (uint32(min_cache) > 1)
                  track->v_bframes = true;

              } else if (EbmlId(*l3) ==
                         KaxTrackMaxCache::ClassInfos.GlobalId) {
                KaxTrackMaxCache &max_cache =
                  *static_cast<KaxTrackMaxCache*>(l3);
                max_cache.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + MaxCache: %u\n",
                         uint32(max_cache));

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagDefault::ClassInfos.GlobalId) {
                KaxTrackFlagDefault &f_default =
                  *static_cast<KaxTrackFlagDefault *>(l3);
                f_default.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Default flag: %d\n",
                         uint32(f_default));
                track->default_track = uint32(f_default);

              } else if (EbmlId(*l3) ==
                         KaxTrackLanguage::ClassInfos.GlobalId) {
                KaxTrackLanguage &lang =
                  *static_cast<KaxTrackLanguage *>(l3);
                lang.ReadData(es->I_O());
                if (verbose > 1)
                  mxinfo("matroska_reader: |  + Language: %s\n",
                         string(lang).c_str());
                safefree(track->language);
                track->language = safestrdup(string(lang).c_str());

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


            } // while (l3 != NULL)

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

      } else if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)
        handle_attachments(in, es, l0, l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        int i, k;
        EbmlElement *el;
        KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);
        int64_t pos;
        bool is_attachments;

        seek_head.Read(*es, KaxSeekHead::ClassInfos.Context, i, el, true);
        for (i = 0; i < seek_head.ListSize(); i++)
          if (EbmlId(*seek_head[i]) == KaxSeek::ClassInfos.GlobalId) {
            KaxSeek &seek = *static_cast<KaxSeek *>(seek_head[i]);
            pos = -1;
            is_attachments = false;

            for (k = 0; k < seek.ListSize(); k++)
              if (EbmlId(*seek[k]) == KaxSeekID::ClassInfos.GlobalId) {
                KaxSeekID &sid = *static_cast<KaxSeekID *>(seek[k]);
                EbmlId id(sid.GetBuffer(), sid.GetSize());
                if (id == KaxAttachments::ClassInfos.GlobalId)
                  is_attachments = true;

              } else if (EbmlId(*seek[k]) ==
                         KaxSeekPosition::ClassInfos.GlobalId)
                pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

            if ((pos != -1) && is_attachments)
              handle_attachments(in, es, l0,
                                 ((KaxSegment *)l0)->GetGlobalPosition(pos));
          }

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        if (verbose > 1)
          mxinfo("matroska_reader: |+ found cluster, headers are "
                 "parsed completely :)\n");
        saved_l1 = l1;
        exit_loop = true;

      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (exit_loop)      // we've found the first cluster, so get out
        break;

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

  } catch (exception &ex) {
    mxerror("matroska_reader: caught exception\n");
  }

  if (!exit_loop)               // We have NOT found a cluster!
    return 0;

  verify_tracks();

  return 1;
}

// }}}

// {{{ FUNCTION kax_reader_c::create_packetizers()

void kax_reader_c::create_packetizers() {
  int i;
  kax_track_t *t;
  track_info_t nti;

  for (i = 0; i < tracks.size(); i++) {
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
      nti.id = t->tnum;         // ID for this track.
      switch (t->type) {

        case 'v':
          if (nti.fourcc[0] == 0)
            memcpy(nti.fourcc, t->v_fourcc, 5);
          if (verbose)
            mxinfo("Matroska demultiplexer (%s): using video output "
                   "module for track ID %u.\n", ti->fname, t->tnum);
          t->packetizer = new video_packetizer_c(this, t->codec_id, t->v_frate,
                                                 t->v_width,
                                                 t->v_height,
                                                 t->v_bframes, &nti);
          if (!nti.aspect_ratio_given) { // The user didn't set it.
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
              mxinfo("Matroska demultiplexer (%s): using the PCM "
                     "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x0055) {
            t->packetizer = new mp3_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti);
            if (verbose)
              mxinfo("Matroska demultiplexer (%s): using the MP3 "
                     "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x2000) {
            int bsid;

            if (!strcmp(t->codec_id, "A_AC3/BSID9"))
              bsid = 9;
            else if (!strcmp(t->codec_id, "A_AC3/BSID10"))
              bsid = 10;
            else
              bsid = 0;
            t->packetizer = new ac3_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, bsid, &nti);
            if (verbose)
              mxinfo("Matroska demultiplexer (%s): using the AC3 "
                     "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == 0x2001) {
            mxerror("Reading DTS from Matroska not implemented yet,"
                    "cannot we get a complete DTS_Header here for construction"
                    "of the packetizer?");
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
              mxinfo("Matroska demultiplexer (%s): using the Vorbis "
                     "output module for track ID %u.\n", ti->fname, t->tnum);
          } else if (t->a_formattag == FOURCC('M', 'P', '4', 'A')) {
            // A_AAC/MPEG2/MAIN
            // 0123456789012345
            int id, profile, sbridx;

            if (t->codec_id[10] == '2')
              id = AAC_ID_MPEG2;
            else if (t->codec_id[10] == '4')
              id = AAC_ID_MPEG4;
            else
              mxerror("matroska_reader: Malformed codec id "
                      "%s for track %d.\n", t->codec_id, t->tnum);

            if (!strcmp(&t->codec_id[12], "MAIN"))
              profile = AAC_PROFILE_MAIN;
            else if (!strcmp(&t->codec_id[12], "LC"))
              profile = AAC_PROFILE_LC;
            else if (!strcmp(&t->codec_id[12], "SSR"))
              profile = AAC_PROFILE_SSR;
            else if (!strcmp(&t->codec_id[12], "LTP"))
              profile = AAC_PROFILE_LTP;
            else if (!strcmp(&t->codec_id[12], "LC/SBR"))
              profile = AAC_PROFILE_SBR;
            else
              mxerror("matroska_reader: Malformed codec id "
                      "%s for track %d.\n", t->codec_id, t->tnum);

            for (sbridx = 0; sbridx < ti->aac_is_sbr->size(); sbridx++)
              if (((*ti->aac_is_sbr)[sbridx] == t->tnum) ||
                  ((*ti->aac_is_sbr)[sbridx] == -1)) {
                profile = AAC_PROFILE_SBR;
                break;
              }

            t->packetizer = new aac_packetizer_c(this, id, profile,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti,
                                                 false, true);
            if (verbose)
              mxinfo("Matroska demultiplexer (%s): using the AAC "
                     "output module for track ID %u.\n", ti->fname, t->tnum);
          } else
            mxerror("matroska_reader: Unsupported track type "
                    "for track %d.\n", t->tnum);

          break;

        case 's':
          t->packetizer = new textsubs_packetizer_c(this, t->codec_id,
                                                    t->private_data,
                                                    t->private_size, false,
                                                    true, &nti);
          if (verbose)
            mxinfo("Matroska demultiplexer (%s): using the text "
                   "subtitle output module for track ID %u.\n", ti->fname,
                   t->tnum);
          break;

        default:
          mxerror("matroska_reader: Unsupported track type "
                  "for track %d.\n", t->tnum);
          break;
      }
      if (t->tuid != 0)
        if (!t->packetizer->set_uid(t->tuid))
          mxwarn("matroska_reader: Could not keep the track "
                 "UID %u because it is already allocated for the new "
                 "file.\n", t->tuid);
    }
  }
}

// }}}

// {{{ FUNCTION kax_reader_c::read()

int kax_reader_c::read() {
  int upper_lvl_el, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  KaxBlockGroup *block_group;
  KaxBlock *block;
  KaxReferenceBlock *ref_block;
  KaxBlockDuration *duration;
  KaxClusterTimecode *ctc;
  int64_t block_fref, block_bref, block_duration;
  kax_track_t *block_track;
  bool found_data;

  if (tracks.size() == 0)
    return 0;

  l0 = segment;

  if (saved_l1 == NULL)         // We're done.
    return 0;

  debug_enter("kax_reader_c::read");

  found_data = false;
  upper_lvl_el = 0;
  l1 = saved_l1;
  saved_l1 = NULL;

  try {
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        cluster = (KaxCluster *)l1;
        cluster->Read(*es, KaxCluster::ClassInfos.Context, upper_lvl_el, l2,
                      true);

        ctc = static_cast<KaxClusterTimecode *>
          (cluster->FindFirstElt(KaxClusterTimecode::ClassInfos, false));
        if (ctc == NULL)
          die("r_matroska: Cluster does not contain a cluster timecode. "
              "File is broken. Aborting.");

        cluster_tc = uint64(*ctc);
        cluster->InitTimecode(cluster_tc, tc_scale);
        if (first_timecode == -1.0)
          first_timecode = (float)cluster_tc * (float)tc_scale /
            1000000.0;

        block_group = static_cast<KaxBlockGroup *>
          (cluster->FindFirstElt(KaxBlockGroup::ClassInfos, false));
        while (block_group != NULL) {
          block_duration = -1;
          block_bref = VFT_IFRAME;
          block_fref = VFT_NOBFRAME;
          block_track = NULL;
          block = NULL;

          ref_block = static_cast<KaxReferenceBlock *>
            (block_group->FindFirstElt(KaxReferenceBlock::ClassInfos, false));
          while (ref_block != NULL) {
            if (int64(*ref_block) < 0)
              block_bref = int64(*ref_block) * tc_scale / 1000000;
            else
              block_fref = int64(*ref_block) * tc_scale / 1000000;

            ref_block = static_cast<KaxReferenceBlock *>
              (block_group->FindNextElt(*ref_block, false));
          }

          duration = static_cast<KaxBlockDuration *>
            (block_group->FindFirstElt(KaxBlockDuration::ClassInfos, false));
          if (duration != NULL)
            block_duration = (int64_t)uint64(*duration);

          block = static_cast<KaxBlock *>
            (block_group->FindFirstElt(KaxBlock::ClassInfos, false));
          if (block != NULL) {
            block->SetParent(*cluster);
            block_track = find_track_by_num(block->TrackNum());
          }

          if ((block_track != NULL) && (block_track->packetizer != NULL)) {
            if ((block_track->type == 's') && (block_duration == -1)) {
              mxwarn("Text subtitle block does not "
                     "contain a block duration element. This file is "
                     "broken.\n");
            } else {

              last_timecode = block->GlobalTimecode() / 1000000;
              if (block_bref != VFT_IFRAME)
                block_bref += (int64_t)last_timecode;
              if (block_fref != VFT_NOBFRAME)
                block_fref += (int64_t)last_timecode;

              for (i = 0; i < (int)block->NumberFrames(); i++) {
                DataBuffer &data = block->GetBuffer(i);
                if (block_track->type == 's') {
                  char *lines;

                  lines = (char *)safemalloc(data.Size() + 1);
                  lines[data.Size()] = 0;
                  memcpy(lines, data.Buffer(), data.Size());
                  block_track->packetizer->process((unsigned char *)lines, 0,
                                                   (int64_t)last_timecode,
                                                   block_duration,
                                                   block_bref,
                                                   block_fref);
                  safefree(lines);
                } else
                  block_track->packetizer->process((unsigned char *)
                                                   data.Buffer(), data.Size(),
                                                   (int64_t)last_timecode,
                                                   block_duration,
                                                   block_bref,
                                                   block_fref);
              }
              block_track->units_processed += block->NumberFrames();


            }
          }

          found_data = true;

          block_group = static_cast<KaxBlockGroup *>
            (cluster->FindNextElt(*block_group, false));
        }

      }

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
        saved_l1 = l2;
        break;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;

      }

      delete l1;
      saved_l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true);
      break;

    } // while (l1 != NULL)


  } catch (exception ex) {
    mxwarn("matroska_reader: exception caught\n");
    return 0;
  }

  debug_leave("kax_reader_c::read");

  if (found_data)
    return EMOREDATA;
  else
    return 0;
}

// }}}

// {{{ FUNCTIONS get_packet(), display_*, set_headers()

packet_t *kax_reader_c::get_packet() {
  generic_packetizer_c *winner;
  kax_track_t *t;
  int i;

  winner = NULL;

  for (i = 0; i < tracks.size(); i++) {
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

int kax_reader_c::display_priority() {
  int i;

  if (segment_duration != 0.0)
    return DISPLAYPRIORITY_HIGH;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->type == 'v')
      return DISPLAYPRIORITY_MEDIUM;

  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void kax_reader_c::display_progress() {
  int i;

  if (segment_duration != 0.0) {
    mxinfo("Progress: %.3fs/%.3fs (%d%%)\r",
           (last_timecode - first_timecode) / 1000.0, segment_duration,
           (int)((last_timecode - first_timecode) / 10 / segment_duration));
    return;
  }

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->type == 'v') {
      mxinfo("progress: %llu frames\r", tracks[i]->units_processed);
      return;
    }

  mxinfo("working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void kax_reader_c::set_headers() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (demuxing_requested(tracks[i]->type, tracks[i]->tnum) &&
        tracks[i]->ok)
      tracks[i]->packetizer->set_headers();
}

// }}}

// {{{ FUNCTION kax_reader_c::identify()

void kax_reader_c::identify() {
  int i;

  mxinfo("File '%s': container: Matroska\n", ti->fname);
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ok)
      mxinfo("Track ID %d: %s (%s%s%s)\n", tracks[i]->tnum,
             tracks[i]->type == 'v' ? "video" :
             tracks[i]->type == 'a' ? "audio" :
             tracks[i]->type == 's' ? "subtitles" : "unknown",
             tracks[i]->codec_id,
             tracks[i]->ms_compat ? ", " : "",
             tracks[i]->ms_compat ? tracks[i]->v_fourcc : "");

  for (i = 0; i < attachments.size(); i++) {
    mxinfo("Attachment ID %lld: type '%s', size %lld bytes, ",
           attachments[i].id, attachments[i].type.c_str(),
           attachments[i].size);
    if (attachments[i].name.length() == 0)
      mxinfo("no file name given\n");
    else
      mxinfo("file name '%s'\n", attachments[i].name.c_str());
  }
}

// }}}
