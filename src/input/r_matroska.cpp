/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Matroska reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

// {{{ includes

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exception>
#include <typeinfo>

extern "C" {                    // for BITMAPINFOHEADER
#include "avilib.h"
}

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
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxContexts.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "hacks.h"
#include "matroska.h"
#include "output_control.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "r_matroska.h"

#include "p_aac.h"
#include "p_ac3.h"
#include "p_dts.h"
#if defined(HAVE_FLAC_FORMAT_H)
#include "p_flac.h"
#endif
#include "p_mp3.h"
#include "p_passthrough.h"
#include "p_pcm.h"
#include "p_textsubs.h"
#include "p_tta.h"
#include "p_video.h"
#include "p_vobsub.h"
#include "p_vobbtn.h"
#include "p_vorbis.h"
#include "p_wavpack.h"

using namespace std;
using namespace libmatroska;

#define PFX "matroska_reader: "
#define MAP_TRACK_TYPE(c) ((c) == 'a' ? track_audio : \
                           (c) == 'b' ? track_buttons : \
                           (c) == 'v' ? track_video : track_subtitle)
#define MAP_TRACK_TYPE_STRING(c) ((c) == 'a' ? "audio" : \
                                  (c) == 'b' ? "buttons" : \
                                  (c) == 'v' ? "video" : "subtitle")

#define in_parent(p) (in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

// }}}

#define is_ebmlvoid(e) (EbmlId(*e) == EbmlVoid::ClassInfos.GlobalId)

// {{{ FUNCTION kax_reader::probe_file()

/*
   Probes a file by simply comparing the first four bytes to the EBML
   head signature.
*/
int
kax_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size) {
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

kax_reader_c::kax_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  segment_duration = 0;
  first_timecode = -1;
  writing_app_ver = -1;

  if (!read_headers())
    throw error_c(PFX "Failed to read the headers.");
  if (verbose)
    mxinfo(FMT_FN "Using the Matroska demultiplexer.\n", ti->fname.c_str());
}

// }}}

// {{{ D'TOR

kax_reader_c::~kax_reader_c() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i] != NULL)
      delete tracks[i];

  for (i = 0; i < attachments.size(); i++)
    safefree(attachments[i].data);

  if (saved_l1 != NULL)
    delete saved_l1;
  if (in != NULL)
    delete in;
  if (es != NULL)
    delete es;
  if (segment != NULL)
    delete segment;
}

// }}}

// {{{ FUNCTIONS packets_available(), new_kax_track(), find_track*

int
kax_reader_c::packets_available() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ok && (!PTZR(tracks[i]->ptzr)->packet_available()))
      return 0;

  if (tracks.size() == 0)
    return 0;

  return 1;
}

kax_track_t *
kax_reader_c::new_kax_track() {
  kax_track_t *t;

  t = new kax_track_t;
  tracks.push_back(t);

  // Set some default values.
  t->default_track = true;
  t->ptzr = -1;

  return t;
}

kax_track_t *
kax_reader_c::find_track_by_num(uint32_t n,
                                kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i] != NULL) && (tracks[i]->tnum == n) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

kax_track_t *
kax_reader_c::find_track_by_uid(uint32_t uid,
                                kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i] != NULL) && (tracks[i]->tuid == uid) &&
        (tracks[i] != c))
      return tracks[i];

  return NULL;
}

// }}}

// {{{ FUNCTION kax_reader_c::verify_tracks()

void
kax_reader_c::verify_tracks() {
  int tnum, i;
  unsigned char *c;
  uint32_t u, offset, length;
  kax_track_t *t;
  alBITMAPINFOHEADER *bih;
  alWAVEFORMATEX *wfe;

  for (tnum = 0; tnum < tracks.size(); tnum++) {
    t = tracks[tnum];

    t->ok = t->content_decoder.is_ok();

    if (!t->ok)
      continue;
    t->ok = 0;

    if (t->private_data != NULL) {
      c = (unsigned char *)t->private_data;
      length = t->private_size;
      if (t->content_decoder.reverse(c, length,
                                     CONTENT_ENCODING_SCOPE_CODECPRIVATE)) {
        safefree(t->private_data);
        t->private_data = c;
        t->private_size = length;
      }
    }

    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == "")
          continue;
        if (t->codec_id == MKV_V_MSCOMP) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(alBITMAPINFOHEADER))) {
            if (verbose)
              mxwarn(PFX "CodecID for track %u is '" MKV_V_MSCOMP
                     "', but there was no BITMAPINFOHEADER struct present. "
                     "Therefore we don't have a FourCC to identify the video "
                     "codec used.\n", t->tnum);
            continue;
          } else {
            t->ms_compat = 1;

            bih = (alBITMAPINFOHEADER *)t->private_data;

            u = get_uint32_le(&bih->bi_width);
            if (t->v_width != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility "
                       "mode, track %u) Matrosa says video width is %u, but "
                       "the BITMAPINFOHEADER says %u.\n", t->tnum, t->v_width,
                       u);
              if (t->v_width == 0)
                t->v_width = u;
            }

            u = get_uint32_le(&bih->bi_height);
            if (t->v_height != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility "
                       "mode, track %u) Matrosa video height is %u, but the "
                       "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_height, u);
              if (t->v_height == 0)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->bi_compression, 4);
          }
        }

        if (t->v_width == 0) {
          if (verbose)
            mxwarn(PFX "The width for track %u was not set.\n", t->tnum);
          continue;
        }
        if (t->v_height == 0) {
          if (verbose)
            mxwarn(PFX "The height for track %u was not set.\n", t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 'a':                 // audio track
        if (t->codec_id == "")
          continue;
        if (t->codec_id == MKV_A_ACM) {
          if ((t->private_data == NULL) ||
              (t->private_size < sizeof(alWAVEFORMATEX))) {
            if (verbose)
              mxwarn(PFX "CodecID for track %u is '"
                     MKV_A_ACM "', but there was no WAVEFORMATEX struct "
                     "present. Therefore we don't have a format ID to "
                     "identify the audio codec used.\n", t->tnum);
            continue;

          } else {
            t->ms_compat = 1;

            wfe = (alWAVEFORMATEX *)t->private_data;
            t->a_formattag = get_uint16_le(&wfe->w_format_tag);
            u = get_uint32_le(&wfe->n_samples_per_sec);
            if (((uint32_t)t->a_sfreq) != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u samples per"
                       " second, but WAVEFORMATEX says that there are %u.\n",
                       t->tnum, (uint32_t)t->a_sfreq, u);
              if (t->a_sfreq == 0.0)
                t->a_sfreq = (float)u;
            }

            u = get_uint16_le(&wfe->n_channels);
            if (t->a_channels != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u channels, "
                       "but the WAVEFORMATEX says that there are %u.\n",
                       t->tnum, t->a_channels, u);
              if (t->a_channels == 0)
                t->a_channels = u;
            }

            u = get_uint16_le(&wfe->w_bits_per_sample);
            if (t->a_bps != u) {
              if (verbose && (t->a_formattag == 0x0001))
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u bits per "
                       "sample, but the WAVEFORMATEX says that there are %u."
                       "\n", t->tnum, t->a_bps, u);
              if (t->a_bps == 0)
                t->a_bps = u;
            }
          }
        } else {
          if (starts_with(t->codec_id, MKV_A_MP3, strlen(MKV_A_MP3) - 1))
            t->a_formattag = 0x0055;
          else if (starts_with(t->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)))
            t->a_formattag = 0x2000;
          else if (t->codec_id == MKV_A_DTS)
            t->a_formattag = 0x2001;
          else if (t->codec_id == MKV_A_PCM)
            t->a_formattag = 0x0001;
          else if (t->codec_id == MKV_A_VORBIS) {
            if (t->private_data == NULL) {
              if (verbose)
                mxwarn(PFX "CodecID for track %u is "
                       "'A_VORBIS', but there are no header packets present.",
                       t->tnum);
              continue;
            }

            c = (unsigned char *)t->private_data;
            if (c[0] != 2) {
              if (verbose)
                mxwarn(PFX "Vorbis track does not contain valid headers.\n");
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
                  mxwarn(PFX "Vorbis track does not contain valid headers.\n");
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

            // mkvmerge around 0.6.x had a bug writing a default duration
            // for Vorbis tracks but not the correct durations for the
            // individual packets. This comes back to haunt us because
            // when regenerating the timestamps from lacing those durations
            // are used and add up to too large a value. The result is the
            // usual "timecode < last_timecode" message.
            // Workaround: do not use durations for such tracks.
            if ((writing_app == "mkvmerge") && (writing_app_ver != -1) &&
                (writing_app_ver < 0x07000000))
              t->ignore_duration_hack = true;

          } else if ((t->codec_id == MKV_A_AAC_2MAIN) ||
                     (t->codec_id == MKV_A_AAC_2LC) ||
                     (t->codec_id == MKV_A_AAC_2SSR) ||
                     (t->codec_id == MKV_A_AAC_4MAIN) ||
                     (t->codec_id == MKV_A_AAC_4LC) ||
                     (t->codec_id == MKV_A_AAC_4SSR) ||
                     (t->codec_id == MKV_A_AAC_4LTP) ||
                     (t->codec_id == MKV_A_AAC_2SBR) ||
                     (t->codec_id == MKV_A_AAC_4SBR))
            t->a_formattag = FOURCC('M', 'P', '4', 'A');
          else if (starts_with(t->codec_id, MKV_A_REAL_COOK,
                                 strlen("A_REAL/")))
            t->a_formattag = FOURCC('r', 'e', 'a', 'l');
          else if (t->codec_id == MKV_A_FLAC) {
#if defined(HAVE_FLAC_FORMAT_H)
            t->a_formattag = FOURCC('f', 'L', 'a', 'C');
#else
            mxwarn(PFX "mkvmerge was not compiled with FLAC support. "
                   "Ignoring track %u.\n", t->tnum);
            continue;
#endif
          } else if (t->codec_id == MKV_A_TTA)
            t->a_formattag = FOURCC('T', 'T', 'A', '1');
          else if (t->codec_id == MKV_A_WAVPACK4)
            t->a_formattag = FOURCC('W', 'V', 'P', '4');
        }

        if (t->a_sfreq == 0.0)
          t->a_sfreq = 8000.0;

        if (t->a_channels == 0)
          t->a_channels = 1;

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 's':
        if (t->codec_id == MKV_S_VOBSUB) {
          if (t->private_data == NULL) {
            if (verbose)
              mxwarn(PFX "CodecID for track %u is '%s', but there was no "
                     "private data found.\n", t->tnum, t->codec_id.c_str());
            continue;
          }
        }
        t->ok = 1;
        break;

      case 'b':
        if (t->codec_id != MKV_B_VOBBTN) {
          if (verbose)
            mxwarn(PFX "CodecID '%s' for track %u is unknown.\n",
                   t->codec_id.c_str(), t->tnum);
          continue;
        }
        t->ok = 1;
        break;

      default:                  // unknown track type!? error in demuxer...
        if (verbose)
          mxwarn(PFX "matroska_reader: unknown "
                 "demuxer type for track %u: '%c'\n", t->tnum, t->type);
        continue;
    }

    if (t->ok && (verbose > 1))
      mxinfo(PFX "Track %u seems to be ok.\n", t->tnum);
  }
}

// }}}

// {{{ FUNCTION kax_reader_c::handle_attachments()

void
kax_reader_c::handle_attachments(mm_io_c *io,
                                 EbmlElement *l0,
                                 int64_t pos) {
  KaxAttachments *atts;
  KaxAttached *att;
  EbmlElement *l1, *l2;
  UTFstring description, name;
  int upper_lvl_el, i, k;
  string mime_type;
  int64_t size, id;
  unsigned char *data;
  bool found;
  kax_attachment_t matt;

  found = false;
  for (i = 0; i < handled_attachments.size(); i++)
    if (handled_attachments[i] == pos) {
      found = true;
      break;
    }
  if (found)
    return;
  handled_attachments.push_back(pos);

  data = NULL;
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
        name = L"";
        mime_type = "";
        description = L"";
        size = -1;
        id = -1;

        for (k = 0; k < att->ListSize(); k++) {
          l2 = (*att)[k];

          if (EbmlId(*l2) == KaxFileName::ClassInfos.GlobalId) {
            KaxFileName &fname = *static_cast<KaxFileName *>(l2);
            name = UTFstring(fname);

          } else if (EbmlId(*l2) == KaxFileDescription::ClassInfos.GlobalId) {
            KaxFileDescription &fdesc = *static_cast<KaxFileDescription *>(l2);
            description = UTFstring(fdesc);

          } else if (EbmlId(*l2) == KaxMimeType::ClassInfos.GlobalId) {
            KaxMimeType &mtype = *static_cast<KaxMimeType *>(l2);
            mime_type = string(mtype);

          } else if (EbmlId(*l2) == KaxFileUID::ClassInfos.GlobalId) {
            KaxFileUID &fuid = *static_cast<KaxFileUID *>(l2);
            id = uint32(fuid);

          } else if (EbmlId(*l2) == KaxFileData::ClassInfos.GlobalId) {
            KaxFileData &fdata = *static_cast<KaxFileData *>(l2);
            size = fdata.GetSize();
            data = (unsigned char *)fdata.GetBuffer();
          }
        }

        if ((id != -1) && (size != -1) && (mime_type.length() > 0) &&
            (name.length() > 0)) {
          found = false;

          for (k = 0; k < attachments.size(); k++)
            if (attachments[k].id == id) {
              found = true;
              break;
            }

          if (!found) {
            matt.name = name;
            matt.mime_type = mime_type;
            matt.description = description;
            matt.size = size;
            matt.id = id;
            matt.data = (unsigned char *)safememdup(data, size);
            attachments.push_back(matt);
          }
        }
      }
    }

    delete l1;
  } else if (l1 != NULL)
    delete l1;

  io->restore_pos();
}

void
kax_reader_c::handle_chapters(mm_io_c *io,
                              EbmlElement *l0,
                              int64_t pos) {
  KaxChapters *tmp_chapters;
  EbmlElement *l1, *l2;
  int upper_lvl_el, i;
  bool found;

  if (ti->no_chapters)
    return;

  found = false;
  for (i = 0; i < handled_chapters.size(); i++)
    if (handled_chapters[i] == pos) {
      found = true;
      break;
    }
  if (found)
    return;
  handled_chapters.push_back(pos);

  io->save_pos(pos);
  l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true);

  if ((l1 != NULL) && is_id(l1, KaxChapters)) {
    tmp_chapters = static_cast<KaxChapters *>(l1);
    l2 = NULL;
    upper_lvl_el = 0;
    tmp_chapters->Read(*es, KaxChapters::ClassInfos.Context, upper_lvl_el,
                   l2, true);

    if (chapters == NULL)
      chapters = new KaxChapters;
    for (i = 0; i < tmp_chapters->ListSize(); i++)
      chapters->PushElement(*(*tmp_chapters)[i]);
    tmp_chapters->RemoveAll();

    delete l1;
  } else if (l1 != NULL)
    delete l1;

  io->restore_pos();
}

void
kax_reader_c::handle_tags(mm_io_c *io,
                          EbmlElement *l0,
                          int64_t pos) {
  KaxTags *tags;
  KaxTag *tag;
  KaxTagTargets *target;
  KaxTagTrackUID *tuid;
  EbmlElement *l1, *l2;
  int upper_lvl_el, tid, i;
  bool is_global, found, contains_tag;
  kax_track_t *track;

  if (ti->no_tags)
    return;

  found = false;
  for (i = 0; i < handled_tags.size(); i++)
    if (handled_tags[i] == pos) {
      found = true;
      break;
    }
  if (found)
    return;
  handled_tags.push_back(pos);

  io->save_pos(pos);
  l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true);

  if ((l1 != NULL) && (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId)) {
    tags = (KaxTags *)l1;
    l2 = NULL;
    upper_lvl_el = 0;
    tags->Read(*es, KaxTags::ClassInfos.Context, upper_lvl_el, l2, true);

    while (tags->ListSize() > 0) {
      if (!(EbmlId(*(*tags)[0]) == KaxTag::ClassInfos.GlobalId)) {
        delete (*tags)[0];
        tags->Remove(0);
        continue;
      }

      is_global = false;
      tid = -1;
      tag = static_cast<KaxTag *>((*tags)[0]);
      target = FINDFIRST(tag, KaxTagTargets);
      if (target != NULL) {
        tuid = FINDFIRST(target, KaxTagTrackUID);
        if (tuid == NULL)
          is_global = true;
        else {
          found = false;
          track = find_track_by_uid(uint32(*tuid));
          if (track != NULL) {
            found = true;
            contains_tag = false;
            for (i = 0; i < tag->ListSize(); i++)
              if (dynamic_cast<KaxTagSimple *>((*tag)[i]) != NULL) {
                contains_tag = true;
                break;
              }
            if (contains_tag) {
              if (track->tags == NULL)
                track->tags = new KaxTags;
              track->tags->PushElement(*tag);
            }
          }
        }
      } else
        is_global = true;

      if (is_global)
        add_tags(tag);
      else if (!found)
        delete tag;
      tags->Remove(0);
    }

  } else if (l1 != NULL)
    delete l1;

  io->restore_pos();
}

// }}}

// {{{ FUNCTION kax_reader_c::read_headers()

int
kax_reader_c::read_headers() {
  int upper_lvl_el, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  kax_track_t *track;
  bool exit_loop;
  vector<int64_t> deferred_tags, deferred_chapters, deferred_attachments;

  exit_loop = false;
  try {
    in = new mm_file_io_c(ti->fname);
    file_size = in->get_size();
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL) {
      mxwarn(PFX "no EBML head found.\n");
      return 0;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;
    if (verbose > 1)
      mxinfo(PFX "Found the head...\n");

    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL) {
      if (verbose)
        mxwarn(PFX "No segment found.\n");
      return 0;
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      if (verbose)
        mxwarn(PFX "No segment found.\n");
      return 0;
    }
    if (verbose > 1)
      mxinfo(PFX "+ a segment...\n");

    segment = l0;

    upper_lvl_el = 0;
    exit_loop = false;
    tc_scale = TIMECODE_SCALE;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        KaxTimecodeScale *ktc_scale;
        KaxDuration *kduration;
        KaxTitle *ktitle;
        KaxWritingApp *kwriting_app;

        // General info about this Matroska file
        if (verbose > 1)
          mxinfo(PFX "|+ segment information...\n");

        l1->Read(*es, KaxInfo::ClassInfos.Context, upper_lvl_el, l2, true);

        ktc_scale = FINDFIRST(l1, KaxTimecodeScale);
        if (ktc_scale != NULL) {
          tc_scale = uint64(*ktc_scale);
            if (verbose > 1)
              mxinfo(PFX "| + timecode scale: %llu\n", tc_scale);

        } else
          tc_scale = 1000000;

        kduration = FINDFIRST(l1, KaxDuration);
        if (kduration != NULL) {
          segment_duration = irnd(double(*kduration) * tc_scale);
          if (verbose > 1)
            mxinfo(PFX "| + duration: %.3fs\n", segment_duration /
                   1000000000.0);
        }

        ktitle = FINDFIRST(l1, KaxTitle);
        if (ktitle != NULL) {
          title = UTFstring_to_cstr(UTFstring(*ktitle));
          if (verbose > 1)
            mxinfo(PFX "| + title: %s\n", title.c_str());
          title = UTFstring_to_cstrutf8(UTFstring(*ktitle));
        }

        // Let's try to parse the "writing application" string. This usually
        // contains the name and version number of the application used for
        // creating this Matroska file. Examples are:
        //
        // mkvmerge v0.6.6
        // mkvmerge v0.9.6 ('Every Little Kiss') built on Oct  7 2004 18:37:49
        // VirtualDubMod 1.5.4.1 (build 2178/release)
        // AVI-Mux GUI 1.16.8 MPEG test build 1, Aug 24 2004  12:42:57
        //
        // The idea is to first replace known application names that contain
        // spaces with one that doesn't. Then split the whole string up on
        // spaces into at most three parts. If the result is at least two parts
        // long then try to parse the version number from the second and
        // store a lower case version of the first as the application's name.
        kwriting_app = FINDFIRST(l1, KaxWritingApp);
        if (kwriting_app != NULL) {
          int idx;
          vector<string> parts, ver_parts;
          string s;

          s = UTFstring_to_cstrutf8(UTFstring(*kwriting_app));
          strip(s);
          if (verbose > 1)
            mxinfo(PFX "| + writing app: %s\n", s.c_str());
          if (starts_with_case(s, "avi-mux gui"))
            s.replace(0, strlen("avi-mux gui"), "avimuxgui");

          parts = split(s.c_str(), " ", 3);
          if (parts.size() < 2) {
            writing_app = "";
            for (idx = 0; idx < s.size(); idx++)
              writing_app += tolower(s[idx]);
            writing_app_ver = -1;

          } else {
            bool failed;

            writing_app = "";
            for (idx = 0; idx < parts[0].size(); idx++)
              writing_app += tolower(parts[0][idx]);
            s = "";
            for (idx = 0; idx < parts[1].size(); idx++)
              if (isdigit(parts[1][idx]) || (parts[1][idx] == '.'))
                s += parts[1][idx];
            ver_parts = split(s.c_str(), ".");
            for (idx = ver_parts.size(); idx < 4; idx++)
              ver_parts.push_back("0");

            failed = false;
            writing_app_ver = 0;
            for (idx = 0; idx < 4; idx++) {
              int num;

              if (!parse_int(ver_parts[idx], num) || (num < 0) ||
                  (num > 255)) {
                failed = true;
                break;
              }
              writing_app_ver <<= 8;
              writing_app_ver |= num;
            }
            if (failed)
              writing_app_ver = -1;
          }

          mxverb(3, PFX "|   (writing_app '%s', writing_app_ver 0x%08x)\n",
                 writing_app.c_str(), (uint32_t)writing_app_ver);
        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        KaxTrackEntry *ktentry;

        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        if (verbose > 1)
          mxinfo(PFX "|+ segment tracks...\n");

        l1->Read(*es, KaxTracks::ClassInfos.Context, upper_lvl_el, l2, true);

        ktentry = FINDFIRST(l1, KaxTrackEntry);
        while (ktentry != NULL) {
          // We actually found a track entry :) We're happy now.

          KaxTrackNumber *ktnum;
          KaxTrackUID *ktuid;
          KaxTrackDefaultDuration *kdefdur;
          KaxTrackType *kttype;
          KaxTrackAudio *ktaudio;
          KaxTrackVideo *ktvideo;
          KaxCodecID *kcodecid;
          KaxCodecPrivate *kcodecpriv;
          KaxTrackFlagDefault *ktfdefault;
          KaxTrackFlagLacing *ktflacing;
          KaxMaxBlockAdditionID *ktmax_blockadd_id;
          KaxTrackLanguage *ktlanguage;
          KaxTrackMinCache *ktmincache;
          KaxTrackMaxCache *ktmaxcache;
          KaxTrackName *ktname;

          if (verbose > 1)
            mxinfo(PFX "| + a track...\n");

          track = new_kax_track();
          if (track == NULL)
            return 0;

          ktnum = FINDFIRST(ktentry, KaxTrackNumber);
          if (ktnum == NULL)
            mxerror(PFX "A track is missing its track number.\n");
          if (verbose > 1)
            mxinfo(PFX "|  + Track number: %d\n", uint8(*ktnum));
          track->tnum = uint8(*ktnum);
          if ((find_track_by_num(track->tnum, track) != NULL) && (verbose > 1))
            mxwarn(PFX "|  + There's more than one track with "
                   "the number %u.\n", track->tnum);

          ktuid = FINDFIRST(ktentry, KaxTrackUID);
          if (ktuid == NULL)
            mxerror(PFX "A track is missing its track UID.\n");
          if (verbose > 1)
            mxinfo(PFX "|  + Track UID: %u\n", uint32(*ktuid));
          track->tuid = uint32(*ktuid);
          if ((find_track_by_uid(track->tuid, track) != NULL) && (verbose > 1))
            mxwarn(PFX "|  + There's more than one track with the UID %u.\n",
                   track->tnum);

          kdefdur = FINDFIRST(ktentry, KaxTrackDefaultDuration);
          if (kdefdur != NULL) {
            track->v_frate = 1000000000.0 / (float)uint64(*kdefdur);
            if (verbose > 1)
              mxinfo(PFX "|  + Default duration: %.3fms ( = %.3f fps)\n",
                     (float)uint64(*kdefdur) / 1000000.0, track->v_frate);
          }

          kttype = FINDFIRST(ktentry, KaxTrackType);
          if (kttype == NULL)
            mxerror(PFX "Track type was not found.\n");
          if (verbose > 1)
            mxinfo(PFX "|  + Track type: ");
          switch (uint8(*kttype)) {
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

          ktaudio = FINDFIRST(ktentry, KaxTrackAudio);
          if (ktaudio != NULL) {
            KaxAudioSamplingFreq *ka_sfreq;
            KaxAudioOutputSamplingFreq *ka_osfreq;
            KaxAudioChannels *ka_channels;
            KaxAudioBitDepth *ka_bitdepth;

            if (verbose > 1)
              mxinfo(PFX "|  + Audio track\n");

            ka_sfreq = FINDFIRST(ktaudio, KaxAudioSamplingFreq);
            if (ka_sfreq != NULL) {
              track->a_sfreq = float(*ka_sfreq);
              if (verbose > 1)
                mxinfo(PFX "|   + Sampling frequency: %f\n", track->a_sfreq);
            } else
              track->a_sfreq = 8000.0;

            ka_osfreq = FINDFIRST(ktaudio, KaxAudioOutputSamplingFreq);
            if (ka_osfreq != NULL) {
              track->a_osfreq = float(*ka_osfreq);
              if (verbose > 1)
                mxinfo(PFX "|   + Output sampling frequency: %f\n",
                       track->a_osfreq);
            }

            ka_channels = FINDFIRST(ktaudio, KaxAudioChannels);
            if (ka_channels != NULL) {
              track->a_channels = uint8(*ka_channels);
              if (verbose > 1)
                mxinfo(PFX "|   + Channels: %u\n", track->a_channels);
            } else
              track->a_channels = 1;


            ka_bitdepth = FINDFIRST(ktaudio, KaxAudioBitDepth);
            if (ka_bitdepth != NULL) {
              track->a_bps = uint8(*ka_bitdepth);
              if (verbose > 1)
                mxinfo(PFX "|   + Bit depth: %u\n", track->a_bps);
            }
          }

          ktvideo = FINDFIRST(ktentry, KaxTrackVideo);
          if (ktvideo != NULL) {
            KaxVideoPixelWidth *kv_pwidth;
            KaxVideoPixelHeight *kv_pheight;
            KaxVideoDisplayWidth *kv_dwidth;
            KaxVideoDisplayHeight *kv_dheight;
            KaxVideoFrameRate *kv_frate;
            KaxVideoPixelCropLeft *kv_pcleft;
            KaxVideoPixelCropTop *kv_pctop;
            KaxVideoPixelCropRight *kv_pcright;
            KaxVideoPixelCropBottom *kv_pcbottom;

            kv_pwidth = FINDFIRST(ktvideo, KaxVideoPixelWidth);
            if (kv_pwidth != NULL) {
              track->v_width = uint16(*kv_pwidth);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel width: %u\n", track->v_width);
            } else
              mxerror(PFX "Pixel width is missing.\n");

            kv_pheight = FINDFIRST(ktvideo, KaxVideoPixelHeight);
            if (kv_pheight != NULL) {
              track->v_height = uint16(*kv_pheight);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel height: %u\n", track->v_height);
            } else
              mxerror(PFX "Pixel height is missing.\n");

            kv_dwidth = FINDFIRST(ktvideo, KaxVideoDisplayWidth);
            if (kv_dwidth != NULL) {
              track->v_dwidth = uint16(*kv_dwidth);
              if (verbose > 1)
                mxinfo(PFX "|   + Display width: %u\n", track->v_dwidth);
            }

            kv_dheight = FINDFIRST(ktvideo, KaxVideoDisplayHeight);
            if (kv_dheight != NULL) {
              track->v_dheight = uint16(*kv_dheight);
              if (verbose > 1)
                mxinfo(PFX "|   + Display height: %u\n", track->v_dheight);
            }

            // For older files.
            kv_frate = FINDFIRST(ktvideo, KaxVideoFrameRate);
            if (kv_frate != NULL) {
              track->v_frate = float(*kv_frate);
              if (verbose > 1)
                mxinfo(PFX "|   + Frame rate: %f\n", track->v_frate);
            }

            kv_pcleft = FINDFIRST(ktvideo, KaxVideoPixelCropLeft);
            if (kv_pcleft != NULL) {
              track->v_pcleft = uint16(*kv_pcleft);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel crop left: %u\n", track->v_pcleft);
            }

            kv_pctop = FINDFIRST(ktvideo, KaxVideoPixelCropTop);
            if (kv_pctop != NULL) {
              track->v_pctop = uint16(*kv_pctop);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel crop top: %u\n", track->v_pctop);
            }

            kv_pcright = FINDFIRST(ktvideo, KaxVideoPixelCropRight);
            if (kv_pcright != NULL) {
              track->v_pcright = uint16(*kv_pcright);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel crop right: %u\n", track->v_pcright);
            }

            kv_pcbottom = FINDFIRST(ktvideo, KaxVideoPixelCropBottom);
            if (kv_pcbottom != NULL) {
              track->v_pcbottom = uint16(*kv_pcbottom);
              if (verbose > 1)
                mxinfo(PFX "|   + Pixel crop bottom: %u\n", track->v_pcbottom);
            }

          }

          kcodecid = FINDFIRST(ktentry, KaxCodecID);
          if (kcodecid != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Codec ID: %s\n", string(*kcodecid).c_str());
            track->codec_id = string(*kcodecid);
          } else
            mxerror(PFX "CodecID is missing.\n");

          kcodecpriv = FINDFIRST(ktentry, KaxCodecPrivate);
          if (kcodecpriv != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + CodecPrivate, length %llu\n",
                     kcodecpriv->GetSize());
            track->private_size = kcodecpriv->GetSize();
            if (track->private_size > 0)
              track->private_data = safememdup(&binary(*kcodecpriv),
                                               track->private_size);
          }

          ktmincache = FINDFIRST(ktentry, KaxTrackMinCache);
          if (ktmincache != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + MinCache: %u\n", uint32(*ktmincache));
            if (uint32(*ktmincache) > 1)
              track->v_bframes = true;
            track->min_cache = uint32(*ktmincache);
          }

          ktmaxcache = FINDFIRST(ktentry, KaxTrackMaxCache);
          if (ktmaxcache != NULL) {
            mxverb(2, PFX "|  + MaxCache: %u\n", uint32(*ktmaxcache));
            track->max_cache = uint32(*ktmaxcache);
          }

          ktfdefault = FINDFIRST(ktentry, KaxTrackFlagDefault);
          if (ktfdefault != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Default flag: %d\n", uint32(*ktfdefault));
            track->default_track = uint32(*ktfdefault);
          }

          ktflacing = FINDFIRST(ktentry, KaxTrackFlagLacing);
          if (ktflacing != NULL) {
            mxverb(2, PFX "|  + Lacing flag: %d\n", uint32(*ktflacing));
            track->lacing_flag = uint32(*ktflacing);
          } else
            track->lacing_flag = true;

          ktmax_blockadd_id = FINDFIRST(ktentry, KaxMaxBlockAdditionID);
          if (ktmax_blockadd_id != NULL) {
            mxverb(2, PFX "|  + Max Block Addition ID: %u\n",
                   uint32(*ktmax_blockadd_id));
            track->max_blockadd_id = uint32(*ktmax_blockadd_id);
          } else
            track->max_blockadd_id = 0;

          ktlanguage = FINDFIRST(ktentry, KaxTrackLanguage);
          if (ktlanguage != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Language: %s\n", string(*ktlanguage).c_str());
            track->language = string(*ktlanguage);
          }

          ktname = FINDFIRST(ktentry, KaxTrackName);
          if (ktname != NULL) {
            track->track_name = UTFstring_to_cstrutf8(UTFstring(*ktname));
            if (verbose > 1)
              mxinfo(PFX "|  + Name: %s\n",
                     UTFstring_to_cstr(UTFstring(*ktname)).c_str());
          }

          track->content_decoder.initialize(*ktentry);
          ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
        } // while (ktentry != NULL)

        l1->SkipData(*es, l1->Generic().Context);

      } else if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)
        deferred_attachments.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId)
        deferred_chapters.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId)
        deferred_tags.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        int k;
        EbmlElement *el;
        KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);
        int64_t pos;
        bool is_attachments, is_chapters, is_tags;

        i = 0;
        seek_head.Read(*es, KaxSeekHead::ClassInfos.Context, i, el, true);
        for (i = 0; i < seek_head.ListSize(); i++)
          if (EbmlId(*seek_head[i]) == KaxSeek::ClassInfos.GlobalId) {
            KaxSeek &seek = *static_cast<KaxSeek *>(seek_head[i]);
            pos = -1;
            is_attachments = false;
            is_chapters = false;
            is_tags = false;

            for (k = 0; k < seek.ListSize(); k++)
              if (EbmlId(*seek[k]) == KaxSeekID::ClassInfos.GlobalId) {
                KaxSeekID &sid = *static_cast<KaxSeekID *>(seek[k]);
                EbmlId id(sid.GetBuffer(), sid.GetSize());
                if (id == KaxAttachments::ClassInfos.GlobalId)
                  is_attachments = true;
                else if (id == KaxChapters::ClassInfos.GlobalId)
                  is_chapters = true;
                else if (id == KaxTags::ClassInfos.GlobalId)
                  is_tags = true;

              } else if (EbmlId(*seek[k]) ==
                         KaxSeekPosition::ClassInfos.GlobalId)
                pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

            if (pos != -1) {
              pos = ((KaxSegment *)l0)->GetGlobalPosition(pos);
              if (is_attachments)
                deferred_attachments.push_back(pos);
              else if (is_chapters)
                deferred_chapters.push_back(pos);
              else if (is_tags)
                deferred_tags.push_back(pos);
            }
          }

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        mxverb(2, PFX "|+ found cluster, headers are parsed completely :)\n");
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

    for (i = 0; i < deferred_attachments.size(); i++)
      handle_attachments(in, l0, deferred_attachments[i]);
    for (i = 0; i < deferred_chapters.size(); i++)
      handle_chapters(in, l0, deferred_chapters[i]);
    for (i = 0; i < deferred_tags.size(); i++)
      handle_tags(in, l0, deferred_tags[i]);

  } catch (exception &ex) {
    mxerror(PFX "caught exception\n");
  }

  if (!exit_loop)               // We have NOT found a cluster!
    return 0;

  verify_tracks();

  return 1;
}

// }}}

// {{{ FUNCTION kax_reader_c::create_packetizers()


void
kax_reader_c::init_passthrough_packetizer(kax_track_t *t) {
  passthrough_packetizer_c *ptzr;
  track_info_c *nti;

  mxinfo(FMT_TID "Using the passthrough output module for this %s "
         "track.\n", ti->fname.c_str(), (int64_t)t->tnum,
         MAP_TRACK_TYPE_STRING(t->type));

  nti = new track_info_c(*ti);
  nti->id = t->tnum;
  nti->language = t->language;
  nti->track_name = t->track_name;

  ptzr = new passthrough_packetizer_c(this, nti);
  t->ptzr = add_packetizer(ptzr);
  t->passthrough = true;

  ptzr->set_track_type(MAP_TRACK_TYPE(t->type));
  ptzr->set_codec_id(t->codec_id);
  ptzr->set_codec_private((unsigned char *)t->private_data,
                          t->private_size);
  if (t->v_frate > 0.0)
    ptzr->set_track_default_duration((int64_t)(1000000000.0 / t->v_frate));
  if (t->min_cache > 0)
    ptzr->set_track_min_cache(t->min_cache);
  if (t->max_cache > 0)
    ptzr->set_track_max_cache(t->max_cache);

  if (t->type == 'v') {
    ptzr->set_video_pixel_width(t->v_width);
    ptzr->set_video_pixel_height(t->v_height);
    if (!ptzr->ti->aspect_ratio_given) { // The user hasn't set it.
      if (t->v_dwidth != 0)
        ptzr->set_video_display_width(t->v_dwidth);
      if (t->v_dheight != 0)
        ptzr->set_video_display_height(t->v_dheight);
    }
    if ((ptzr->ti->pixel_cropping.id == -2) &&
        ((t->v_pcleft > 0) || (t->v_pctop > 0) ||
         (t->v_pcright > 0) || (t->v_pcbottom > 0)))
      ptzr->set_video_pixel_cropping(t->v_pcleft, t->v_pctop,
                                     t->v_pcright, t->v_pcbottom);
    if (ptzr->get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
      ptzr->set_cue_creation( CUE_STRATEGY_IFRAMES);

  } else if (t->type == 'a') {
    ptzr->set_audio_sampling_freq(t->a_sfreq);
    ptzr->set_audio_channels(t->a_channels);
    if (t->a_bps != 0)
      ptzr->set_audio_bit_depth(t->a_bps);
    if (t->a_osfreq != 0.0)
      ptzr->set_audio_output_sampling_freq(t->a_osfreq);

  } else {
    // Nothing to do for subs, I guess.
  }

}

void
kax_reader_c::set_packetizer_headers(kax_track_t *t) {
  if (appending)
    return;
  if (t->default_track)
    PTZR(t->ptzr)->set_as_default_track(t->type == 'v' ? DEFTRACK_TYPE_VIDEO :
                                        t->type == 'a' ? DEFTRACK_TYPE_AUDIO :
                                        DEFTRACK_TYPE_SUBS,
                                        DEFAULT_TRACK_PRIORITY_FROM_SOURCE);
  if (t->tuid != 0)
    if (!PTZR(t->ptzr)->set_uid(t->tuid))
      mxwarn(PFX "Could not keep the track UID %u because it is already "
             "allocated for the new file.\n", t->tuid);
}

void
kax_reader_c::create_packetizer(int64_t tid) {
  uint32_t i;
  kax_track_t *t;
  track_info_c *nti;

  t = NULL;
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->tnum == tid) {
      t = tracks[i];
      break;
    }
  if (t == NULL)
    return;

  if ((t->ptzr == -1) && t->ok && demuxing_requested(t->type, t->tnum)) {
    nti = new track_info_c(*ti);
    nti->private_data =
      (unsigned char *)safememdup(t->private_data, t->private_size);
    nti->private_size = t->private_size;
    if (nti->language == "")
      nti->language = t->language;
    if (nti->track_name == "")
      nti->track_name = t->track_name;
    nti->id = t->tnum;          // ID for this track.
    if (t->tags != NULL)
      nti->tags = dynamic_cast<KaxTags *>(t->tags->Clone());

    if (hack_engaged(ENGAGE_FORCE_PASSTHROUGH_PACKETIZER)) {
      init_passthrough_packetizer(t);
      set_packetizer_headers(t);
      delete nti;

      return;
    }

    switch (t->type) {
      case 'v':
        if (starts_with(t->codec_id, "V_MPEG4", 7) ||
            (t->codec_id == MKV_V_MSCOMP) ||
            starts_with(t->codec_id, "V_REAL", 6) ||
            (t->codec_id == MKV_V_QUICKTIME) ||
            (t->codec_id == MKV_V_MPEG1) ||
            (t->codec_id == MKV_V_MPEG2)) {
          const char *fourcc;

          if ((t->codec_id == MKV_V_MSCOMP) &&
              (t->private_data != NULL) &&
              (t->private_size >= sizeof(alBITMAPINFOHEADER)))
            fourcc = (const char *)
              &((alBITMAPINFOHEADER *)t->private_data)->bi_compression;
          else
            fourcc = NULL;

          if ((t->codec_id == MKV_V_MPEG1) || (t->codec_id == MKV_V_MPEG2)) {
            int version;

            version = t->codec_id[6] - '0';
            mxinfo(FMT_TID "Using the MPEG-%d video output module.\n",
                   ti->fname.c_str(), (int64_t)t->tnum, version);
            t->ptzr =
              add_packetizer(new mpeg1_2_video_packetizer_c(this,
                                                            version,
                                                            t->v_frate,
                                                            t->v_width,
                                                            t->v_height,
                                                            t->v_dwidth,
                                                            t->v_dheight,
                                                            true, nti));

          } else if (IS_MPEG4_L2_CODECID(t->codec_id) ||
                     ((fourcc != NULL) && IS_MPEG4_L2_FOURCC(fourcc))) {
            bool is_native;

            mxinfo(FMT_TID "Using the MPEG-4 part 2 video output module.\n",
                   ti->fname.c_str(), (int64_t)t->tnum);
            is_native = IS_MPEG4_L2_CODECID(t->codec_id);
            t->ptzr =
              add_packetizer(new mpeg4_p2_video_packetizer_c(this,
                                                             t->v_frate,
                                                             t->v_width,
                                                             t->v_height,
                                                             is_native,
                                                             nti));

          } else if (t->codec_id == MKV_V_MPEG4_AVC) {
            mxinfo(FMT_TID "Using the MPEG-4 part 10 (AVC) video output "
                   "module.\n", ti->fname.c_str(), (int64_t)t->tnum);
            t->ptzr =
              add_packetizer(new mpeg4_p10_video_packetizer_c(this,
                                                              t->v_frate,
                                                              t->v_width,
                                                              t->v_height,
                                                              nti));

          } else {
            mxinfo(FMT_TID "Using the video output module.\n",
                   ti->fname.c_str(), (int64_t)t->tnum);
            t->ptzr =
              add_packetizer(new video_packetizer_c(this,
                                                    t->codec_id.c_str(),
                                                    t->v_frate,
                                                    t->v_width,
                                                    t->v_height,
                                                    nti));
          }

          if (!PTZR(t->ptzr)->ti->aspect_ratio_given) {
            // The user hasn't set it.
            if (t->v_dwidth != 0)
              PTZR(t->ptzr)->set_video_display_width(t->v_dwidth);
            if (t->v_dheight != 0)
              PTZR(t->ptzr)->set_video_display_height(t->v_dheight);
          }
          if ((PTZR(t->ptzr)->ti->pixel_cropping.id == -2) &&
              ((t->v_pcleft > 0) || (t->v_pctop > 0) ||
               (t->v_pcright > 0) || (t->v_pcbottom > 0)))
            PTZR(t->ptzr)->set_video_pixel_cropping(t->v_pcleft, t->v_pctop,
                                                    t->v_pcright,
                                                    t->v_pcbottom);
        } else
          init_passthrough_packetizer(t);

        break;

      case 'a':
        if (t->a_formattag == 0x0001) {
          t->ptzr =
            add_packetizer(new pcm_packetizer_c(this, (int32_t)t->a_sfreq,
                                                t->a_channels, t->a_bps, nti));
          mxinfo(FMT_TID "Using the PCM output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);
        } else if (t->a_formattag == 0x0055) {
          t->ptzr =
            add_packetizer(new mp3_packetizer_c(this, (int32_t)t->a_sfreq,
                                                t->a_channels, true, nti));
          mxinfo(FMT_TID "Using the MPEG audio output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);
        } else if (t->a_formattag == 0x2000) {
          int bsid;

          if (t->codec_id == "A_AC3/BSID9")
            bsid = 9;
          else if (t->codec_id == "A_AC3/BSID10")
            bsid = 10;
          else
            bsid = 0;
          t->ptzr =
            add_packetizer(new ac3_packetizer_c(this, (int32_t)t->a_sfreq,
                                                t->a_channels, bsid, nti));
          mxinfo(FMT_TID "Using the AC3 output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);
        } else if (t->a_formattag == 0x2001) {
          dts_header_t dtsheader;

          dtsheader.core_sampling_frequency = (unsigned int)t->a_sfreq;
          dtsheader.audio_channels = t->a_channels;
          t->ptzr =
            add_packetizer(new dts_packetizer_c(this, dtsheader, nti, true));
          mxinfo(FMT_TID "Using the DTS output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);

        } else if (t->a_formattag == 0xFFFE) {
          t->ptzr =
            add_packetizer(new vorbis_packetizer_c(this,
                                                   t->headers[0],
                                                   t->header_sizes[0],
                                                   t->headers[1],
                                                   t->header_sizes[1],
                                                   t->headers[2],
                                                   t->header_sizes[2], nti));
          mxinfo(FMT_TID "Using the Vorbis output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);
        } else if ((t->a_formattag == FOURCC('M', 'P', '4', 'A')) ||
                   (t->a_formattag == 0x00ff)) {
          // A_AAC/MPEG2/MAIN
          // 0123456789012345
          int id, profile, sbridx;

          id = 0;
          profile = 0;
          if (t->a_formattag == FOURCC('M', 'P', '4', 'A')) {
            int channels, sfreq, osfreq;
            bool sbr;

            if ((t->private_data != NULL) && (ti->private_size >= 2)) {
              id = AAC_ID_MPEG4;
              if (!parse_aac_data((unsigned char *)t->private_data,
                                  t->private_size, profile, channels, sfreq,
                                  osfreq, sbr))
                mxerror(FMT_TID "Malformed AAC codec initialization data "
                        "found.\n", ti->fname.c_str(), (int64_t)t->tnum);
              if (sbr)
                profile = AAC_PROFILE_SBR;
            } else if (!parse_aac_codec_id(string(t->codec_id), id, profile))
              mxerror(FMT_TID "Malformed codec id '%s'.\n", ti->fname.c_str(),
                      (int64_t)t->tnum, t->codec_id.c_str());
          } else {
            int channels, sfreq, osfreq;
            bool sbr;

            id = AAC_ID_MPEG4;
            if (!parse_aac_data(((unsigned char *)t->private_data) +
                                sizeof(alWAVEFORMATEX),
                                t->private_size - sizeof(alWAVEFORMATEX),
                                profile, channels, sfreq, osfreq, sbr))
              mxerror(FMT_TID "Malformed AAC codec initialization data "
                      "found.\n", ti->fname.c_str(), (int64_t)t->tnum);
            if (sbr)
              profile = AAC_PROFILE_SBR;
          }

          for (sbridx = 0; sbridx < ti->aac_is_sbr.size(); sbridx++)
            if ((ti->aac_is_sbr[sbridx] == t->tnum) ||
                (ti->aac_is_sbr[sbridx] == -1)) {
              profile = AAC_PROFILE_SBR;
              break;
            }

          t->ptzr =
            add_packetizer(new aac_packetizer_c(this, id, profile,
                                                (int32_t)t->a_sfreq,
                                                t->a_channels, nti,
                                                false, true));
          mxinfo(FMT_TID "Using the AAC output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);

#if defined(HAVE_FLAC_FORMAT_H)
        } else if ((t->a_formattag == FOURCC('f', 'L', 'a', 'C')) ||
                   (t->a_formattag == 0xf1ac)) {
          safefree(nti->private_data);
          nti->private_data = NULL;
          nti->private_size = 0;
          if (t->a_formattag == FOURCC('f', 'L', 'a', 'C'))
            t->ptzr =
              add_packetizer(new flac_packetizer_c(this, (unsigned char *)
                                                   t->private_data,
                                                   t->private_size, nti));
          else {
            flac_packetizer_c *p;
            p = new flac_packetizer_c(this,
                                      ((unsigned char *)t->private_data) +
                                      sizeof(alWAVEFORMATEX),
                                      t->private_size - sizeof(alWAVEFORMATEX),
                                      nti);
            t->ptzr = add_packetizer(p);
          }
          mxinfo(FMT_TID "Using the FLAC output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);

#endif
        } else if (t->a_formattag == FOURCC('T', 'T', 'A', '1')) {
          safefree(nti->private_data);
          nti->private_data = NULL;
          nti->private_size = 0;
          t->ptzr =
            add_packetizer(new tta_packetizer_c(this, t->a_channels,
                                                t->a_bps, (int32_t)t->a_sfreq,
                                                nti));
          mxinfo(FMT_TID "Using the TTA output module.\n", ti->fname.c_str(),
                 (int64_t)t->tnum);

        } else if (t->a_formattag == FOURCC('W', 'V', 'P', '4')) {
          wavpack_meta_t meta;

          nti->private_data = (unsigned char *)t->private_data;
          nti->private_size = t->private_size;
          meta.bits_per_sample = t->a_bps;
          meta.channel_count = t->a_channels;
          meta.sample_rate = (uint32_t)t->a_sfreq;
          meta.has_correction = t->max_blockadd_id != 0;
          if (t->v_frate > 0.0)
            meta.samples_per_block = (uint32_t)(t->a_sfreq / t->v_frate);
          t->ptzr = add_packetizer(new wavpack_packetizer_c(this, meta, nti));
          nti->private_data = NULL;
          nti->private_size = 0;
          mxinfo(FMT_TID "Using the WAVPACK output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);

        } else
          init_passthrough_packetizer(t);

        if (t->a_osfreq != 0.0)
          PTZR(t->ptzr)->set_audio_output_sampling_freq(t->a_osfreq);

        break;

      case 's':
        if (t->codec_id == MKV_S_VOBSUB) {
          t->ptzr =
            add_packetizer(new vobsub_packetizer_c(this, t->private_data,
                                                   t->private_size, nti));
          mxinfo(FMT_TID "Using the VobSub output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);

          t->sub_type = 'v';

        } else if (starts_with(t->codec_id, "S_TEXT", 6) ||
                   (t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) {
          string new_codec_id;

          if ((t->codec_id == "S_SSA") || (t->codec_id == "S_ASS"))
            new_codec_id = string("S_TEXT/") + string(&t->codec_id[2]);
          else
            new_codec_id = t->codec_id;
          t->ptzr =
            add_packetizer(new textsubs_packetizer_c(this,
                                                     new_codec_id.c_str(),
                                                     t->private_data,
                                                     t->private_size, false,
                                                     true, nti));
          mxinfo(FMT_TID "Using the text subtitle output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);

          t->sub_type = 't';
        } else
          init_passthrough_packetizer(t);

        break;

      case 'b':
        if (t->codec_id == MKV_B_VOBBTN) {
          safefree(nti->private_data);
          nti->private_data = NULL;
          nti->private_size = 0;
          t->ptzr =
            add_packetizer(new vobbtn_packetizer_c(this, t->v_width,
                                                   t->v_height, nti));
          mxinfo(FMT_TID "Using the VobBtn output module.\n",
                 ti->fname.c_str(), (int64_t)t->tnum);

          t->sub_type = 'b';

        } else
          init_passthrough_packetizer(t);

        break;

      default:
        mxerror(FMT_TID "Unsupported track type for this track.\n",
                ti->fname.c_str(), (int64_t)t->tnum);
        break;
    }
    set_packetizer_headers(t);
    delete nti;
  }
}

void
kax_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    create_packetizer(tracks[i]->tnum);
  if (segment_title_set == false) {
    segment_title = title;
    segment_title_set = true;
  }
}

// }}}

// {{{ FUNCTION kax_reader_c::read()

file_status_e
kax_reader_c::read(generic_packetizer_c *,
                   bool force) {
  int upper_lvl_el, i, bgidx;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  KaxBlockGroup *block_group;
  KaxBlock *block;
  KaxBlockAdditions *blockadd;
  KaxBlockMore *blockmore;
  KaxBlockAdditional *blockadd_data;
  KaxReferenceBlock *ref_block;
  KaxBlockDuration *duration;
  KaxClusterTimecode *ctc;
  int64_t block_fref, block_bref, block_duration, frame_duration;
  uint64_t blockadd_id;
  kax_track_t *block_track;
  bool found_cluster, bref_found, fref_found;

  if (tracks.size() == 0)
    return FILE_STATUS_DONE;

  l0 = segment;

  if (saved_l1 == NULL)         // We're done.
    return FILE_STATUS_DONE;

  if (!force && (get_queued_bytes() > 20 * 1024 * 1024))
    return FILE_STATUS_HOLDING;

  debug_enter("kax_reader_c::read");

  found_cluster = false;
  upper_lvl_el = 0;
  l1 = saved_l1;
  saved_l1 = NULL;

  try {
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        found_cluster = true;
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
        if (first_timecode == -1) {
          first_timecode = cluster_tc * tc_scale;
          if ((chapters != NULL) && (first_timecode > 0))
            adjust_chapter_timecodes(*chapters, -first_timecode);
        }

        for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
          if (!(EbmlId(*(*cluster)[bgidx]) ==
                KaxBlockGroup::ClassInfos.GlobalId))
            continue;
          block_group = static_cast<KaxBlockGroup *>((*cluster)[bgidx]);
          block_duration = -1;
          block_bref = VFT_IFRAME;
          block_fref = VFT_NOBFRAME;
          block_track = NULL;
          block = NULL;
          bref_found = false;
          fref_found = false;

          ref_block = static_cast<KaxReferenceBlock *>
            (block_group->FindFirstElt(KaxReferenceBlock::ClassInfos, false));
          while (ref_block != NULL) {
            if (int64(*ref_block) <= 0) {
              block_bref = int64(*ref_block) * tc_scale;
              bref_found = true;
            } else {
              block_fref = int64(*ref_block) * tc_scale;
              fref_found = true;
            }

            ref_block = static_cast<KaxReferenceBlock *>
              (block_group->FindNextElt(*ref_block, false));
          }

          block = static_cast<KaxBlock *>
            (block_group->FindFirstElt(KaxBlock::ClassInfos, false));
          if (block != NULL) {
            block->SetParent(*cluster);
            block_track = find_track_by_num(block->TrackNum());
          }

          blockadd = static_cast<KaxBlockAdditions *>
            (block_group->FindFirstElt(KaxBlockAdditions::ClassInfos, false));

          duration = static_cast<KaxBlockDuration *>
            (block_group->FindFirstElt(KaxBlockDuration::ClassInfos, false));
          if (duration != NULL)
            block_duration = (int64_t)uint64(*duration) * tc_scale /
              block->NumberFrames();
          else if (block_track->v_frate != 0)
            block_duration = (int64_t)(1000000000.0 / block_track->v_frate);
          frame_duration = (block_duration == -1) ? 0 : block_duration;

          if ((block_track->type == 's') && (block_duration == -1))
            block_duration = 0;

          if (block_track->ignore_duration_hack) {
            frame_duration = 0;
            if (block_duration > 0)
              block_duration = 0;
          }

          last_timecode = block->GlobalTimecode();

          if ((block_track != NULL) && (block_track->ptzr != -1) &&
              block_track->passthrough) {
            // The handling for passthrough is a bit different. We don't have
            // any special cases, e.g. 0 terminating a string for the subs
            // and stuff. Just pass everything through as it is.
            if (bref_found)
                block_bref += last_timecode;
            if (fref_found)
              block_fref += last_timecode;

            for (i = 0; i < (int)block->NumberFrames(); i++) {
              DataBuffer &data = block->GetBuffer(i);
              memory_c mem((unsigned char *)data.Buffer(), data.Size(),
                           false);
              ((passthrough_packetizer_c *)PTZR(block_track->ptzr))->
                process(mem, last_timecode + i * frame_duration,
                        block_duration, block_bref, block_fref,
                        duration != NULL);
            }

          } else if ((block_track != NULL) &&
                     (block_track->ptzr != -1)) {
            if (bref_found) {
              if (block_track->a_formattag == FOURCC('r', 'e', 'a', 'l'))
                block_bref = block_track->previous_timecode;
              else
                block_bref += (int64_t)last_timecode;
            }
            if (fref_found)
              block_fref += (int64_t)last_timecode;

            for (i = 0; i < (int)block->NumberFrames(); i++) {
              unsigned char *re_buffer;
              uint32_t re_size;
              bool re_modified;

              DataBuffer &data = block->GetBuffer(i);
              re_buffer = data.Buffer();
              re_size = data.Size();
              re_modified = block_track->content_decoder.
                reverse(re_buffer, re_size, CONTENT_ENCODING_SCOPE_BLOCK);
              if ((block_track->type == 's') &&
                  (block_track->sub_type == 't')) {
                if ((re_size > 2) ||
                    ((re_size > 0) && (*re_buffer != ' ') &&
                     (*re_buffer != 0) && !iscr(*re_buffer))) {
                  char *lines;

                  lines = (char *)safemalloc(re_size + 1);
                  lines[re_size] = 0;
                  memcpy(lines, re_buffer, re_size);
                  memory_c mem((unsigned char *)lines, 0, true);
                  PTZR(block_track->ptzr)->
                    process(mem, (int64_t)last_timecode, block_duration,
                            block_bref, block_fref);
                  if (re_modified)
                    safefree(re_buffer);
                }
              } else {
                memory_c mem(re_buffer, re_size, re_modified);
                if (blockadd) {
                  memories_c mems;
                  mems.resize(blockadd->ListSize() + 1);
                  mems[0] = &mem;

                  for (i=0; i<blockadd->ListSize(); i++) {
                    if (!(is_id((*blockadd)[i], KaxBlockMore)))
                      continue;
                    blockmore = static_cast<KaxBlockMore *>((*blockadd)[i]);
                    blockadd_id =
                      uint64(*static_cast<EbmlUInteger *>
                             (&GetChild<KaxBlockAddID>(*blockmore)));
                    blockadd_data = &GetChild<KaxBlockAdditional>(*blockmore);
                    memory_c *blockadded =
                      new memory_c(blockadd_data->GetBuffer(),
                                   blockadd_data->GetSize(), false);
                    mems[blockadd_id] = blockadded;
                  }

                  PTZR(block_track->ptzr)->
                    process(mems, (int64_t)last_timecode + i * frame_duration,
                            block_duration, block_bref, block_fref);

                  for (i = 1; i<mems.size(); i++)
                    delete mems[i];
                } else {
                  PTZR(block_track->ptzr)->
                    process(mem, (int64_t)last_timecode + i * frame_duration,
                            block_duration, block_bref, block_fref);
                }
              }
            }

            block_track->previous_timecode = (int64_t)last_timecode;
            block_track->units_processed += block->NumberFrames();
          }

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
    mxwarn(PFX "exception caught\n");
    flush_packetizers();
    return FILE_STATUS_DONE;
  }

  debug_leave("kax_reader_c::read");

  if (found_cluster)
    return FILE_STATUS_MOREDATA;
  else {
    flush_packetizers();
    return FILE_STATUS_DONE;
  }
}

// }}}

// {{{ FUNCTIONS display_*

int
kax_reader_c::get_progress() {
  if (segment_duration != 0)
    return (last_timecode - first_timecode) * 100 / segment_duration;

  return 100 * in->getFilePointer() / file_size;
}

void
kax_reader_c::set_headers() {
  uint32_t i;

  generic_reader_c::set_headers();

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i]->ptzr != -1) && tracks[i]->passthrough)
      PTZR(tracks[i]->ptzr)->get_track_entry()->
        EnableLacing(tracks[i]->lacing_flag);
}

// }}}

// {{{ FUNCTION kax_reader_c::identify()

void
kax_reader_c::identify() {
  int i;
  string info;

  if (identify_verbose) {
    if (!title.empty())
      info = mxsprintf("title:%s", escape(title).c_str());
    if (0 != segment_duration) {
      if (!info.empty())
        info += " ";
      info += mxsprintf("duration:%lld", segment_duration);
    }
    if (!info.empty())
      info = mxsprintf(" [%s]", info.c_str());
  }
  mxinfo("File '%s': container: Matroska%s\n", ti->fname.c_str(),
         info.c_str());
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ok) {
      if (identify_verbose) {
        info = " [";
        if (tracks[i]->language != "")
          info += mxsprintf("language:%s ",
                            escape(tracks[i]->language).c_str());
        if (tracks[i]->track_name != "")
          info += mxsprintf("track_name:%s ",
                            escape(tracks[i]->track_name).c_str());
        if ((tracks[i]->v_dwidth != 0) && (tracks[i]->v_dheight != 0) &&
            ((tracks[i]->v_dwidth != tracks[i]->v_width) ||
             (tracks[i]->v_dheight != tracks[i]->v_height)))
          info += mxsprintf("display_dimensions:%ux%u ",
                            tracks[i]->v_dwidth, tracks[i]->v_dheight);
        info += "]";
      } else
        info = "";
      mxinfo("Track ID %d: %s (%s%s%s)%s\n", tracks[i]->tnum,
             tracks[i]->type == 'v' ? "video" :
             tracks[i]->type == 'a' ? "audio" :
             tracks[i]->type == 'b' ? "buttons" :
             tracks[i]->type == 's' ? "subtitles" : "unknown",
             tracks[i]->codec_id.c_str(),
             tracks[i]->ms_compat ? ", " : "",
             tracks[i]->ms_compat ?
             (tracks[i]->type == 'v' ? tracks[i]->v_fourcc :
              tracks[i]->a_formattag == 0x0001 ? "PCM" :
              tracks[i]->a_formattag == 0x0055 ? "MP3" :
              tracks[i]->a_formattag == 0x2000 ? "AC3" : "unknown") : "",
             info.c_str());
    }

  for (i = 0; i < attachments.size(); i++) {
    mxinfo("Attachment ID %lld: type '%s', size %lld bytes, ",
           attachments[i].id, attachments[i].mime_type.c_str(),
           attachments[i].size);
    if (attachments[i].description.length() > 0)
      mxinfo("description '%s', ",
             UTFstring_to_cstr(attachments[i].description).c_str());
    if (attachments[i].name.length() == 0)
      mxinfo("no file name given\n");
    else
      mxinfo("file name '%s'\n",
             UTFstring_to_cstr(attachments[i].name).c_str());
  }
}

// }}}

void
kax_reader_c::add_attachments(KaxAttachments *a) {
  uint32_t i;
  KaxAttached *attached;
  KaxFileData *fdata;
  binary *buffer;

  if (ti->no_attachments)
    return;

  for (i = 0; i < attachments.size(); i++) {
    attached = new KaxAttached;
    if (attachments[i].description.length() > 0)
      *static_cast<EbmlUnicodeString *>
        (&GetChild<KaxFileDescription>(*attached)) =
        attachments[i].description;

    *static_cast<EbmlString *>(&GetChild<KaxMimeType>(*attached)) =
      attachments[i].mime_type;

    *static_cast<EbmlUnicodeString *>(&GetChild<KaxFileName>(*attached)) =
      attachments[i].name;

    *static_cast<EbmlUInteger *>(&GetChild<KaxFileUID>(*attached)) =
      attachments[i].id;

    fdata = &GetChild<KaxFileData>(*attached);
    buffer = new binary[attachments[i].size];
    memcpy(buffer, attachments[i].data, attachments[i].size);
    fdata->SetBuffer(buffer, attachments[i].size);

    a->PushElement(*attached);
  }
}

void
kax_reader_c::flush_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ptzr != -1)
      PTZR(tracks[i]->ptzr)->flush();
}

void
kax_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    available_track_ids.push_back(tracks[i]->tnum);
}

