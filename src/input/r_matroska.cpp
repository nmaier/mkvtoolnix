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
#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxContexts.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "matroska.h"
#include "mkvmerge.h"
#include "mm_io.h"
#include "chapters.h"
#include "common.h"
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
#include "p_video.h"
#include "p_vobsub.h"
#include "p_vorbis.h"

using namespace std;
using namespace libmatroska;

#define PFX "matroska_reader: "

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

  title = "";
  segment_duration = 0.0;
  first_timecode = -1.0;

  if (!read_headers())
    throw error_c(PFX "Failed to read the headers.");

  create_packetizers();
}

// }}}

// {{{ D'TOR

kax_reader_c::~kax_reader_c() {
  int i, k;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i] != NULL) {
      if (tracks[i]->packetizer != NULL)
        delete tracks[i]->packetizer;
      safefree(tracks[i]->private_data);
      safefree(tracks[i]->codec_id);
      safefree(tracks[i]->language);
      safefree(tracks[i]->track_name);
      for (k = 0; k < tracks[i]->c_encodings->size(); k++) {
        safefree((*tracks[i]->c_encodings)[k].comp_settings);
        safefree((*tracks[i]->c_encodings)[k].enc_keyid);
        safefree((*tracks[i]->c_encodings)[k].sig_keyid);
        safefree((*tracks[i]->c_encodings)[k].signature);
      }
      delete tracks[i]->c_encodings;
      if (tracks[i]->zlib_compressor != NULL)
        delete tracks[i]->zlib_compressor;
      if (tracks[i]->bzlib_compressor != NULL)
        delete tracks[i]->bzlib_compressor;
      if (tracks[i]->lzo1x_compressor != NULL)
        delete tracks[i]->lzo1x_compressor;
      safefree(tracks[i]);
    }

  for (i = 0; i < attachments.size(); i++)
    safefree(attachments[i].data);

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
  t->c_encodings = new vector<kax_content_encoding_t>;
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
  kax_content_encoding_t *ce;
  alBITMAPINFOHEADER *bih;
  alWAVEFORMATEX *wfe;

  for (tnum = 0; tnum < tracks.size(); tnum++) {
    t = tracks[tnum];

    t->ok = 1;
    for (i = 0; i < t->c_encodings->size(); i++) {
      ce = &(*t->c_encodings)[i];

      if (ce->type == 1) {
        mxwarn(PFX "Track number %u has been encrypted and decryption has "
               "not yet been implemented. Skipping track.\n", t->tnum);
        t->ok = 0;
        break;
      }

      if (ce->type != 0) {
        mxwarn(PFX "Unknown content encoding type %u for track %u. Skipping "
               "track.\n", ce->type, t->tnum);
        t->ok = 0;
        break;
      }

      if (ce->comp_algo == 0) {
#if !defined(HAVE_ZLIB_H)
        mxwarn(PFX "Track %u was compressed with zlib but mkvmerge has not "
               "been compiled with support for zlib compression. Skipping "
               "track.\n", t->tnum);
        t->ok = 0;
        break;
#else
        if (t->zlib_compressor == NULL)
          t->zlib_compressor = new zlib_compression_c();
#endif
      } else if (ce->comp_algo == 1) {
#if !defined(HAVE_BZLIB_H)
        mxwarn(PFX "Track %u was compressed with bzlib but mkvmerge has not "
               "been compiled with support for bzlib compression. Skipping "
               "track.\n", t->tnum);
        t->ok = 0;
        break;
#else
        if (t->bzlib_compressor == NULL)
          t->bzlib_compressor = new bzlib_compression_c();
#endif
      } else if (ce->comp_algo == 2) {
#if !defined(HAVE_LZO1X_H)
        mxwarn(PFX "Track %u was compressed with lzo1x but mkvmerge has not "
               "been compiled with support for lzo1x compression. Skipping "
               "track.\n", t->tnum);
        t->ok = 0;
        break;
#else
        if (t->lzo1x_compressor == NULL)
          t->lzo1x_compressor = new lzo_compression_c();
#endif
      } else {
        mxwarn(PFX "Track %u has been compressed with an unknown/unsupported "
               "compression algorithm (%u). Skipping track.\n", t->tnum,
               ce->comp_algo);
        t->ok = 0;
        break;
      }
    }

    if (!t->ok)
      continue;
    t->ok = 0;

    if (t->private_data != NULL) {
      c = (unsigned char *)t->private_data;
      length = t->private_size;
      if (reverse_encodings(t, c, length, 2)) {
        safefree(t->private_data);
        t->private_data = c;
        t->private_size = length;
      }
    }

    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == NULL)
          continue;
        if (!strcmp(t->codec_id, MKV_V_MSCOMP)) {
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
            
            u = get_uint32(&bih->bi_width);
            if (t->v_width != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility "
                       "mode, track %u) Matrosa says video width is %u, but "
                       "the BITMAPINFOHEADER says %u.\n", t->tnum, t->v_width,
                       u);
              if (t->v_width == 0)
                t->v_width = u;
            }

            u = get_uint32(&bih->bi_height);
            if (t->v_height != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility "
                       "mode, track %u) Matrosa video height is %u, but the "
                       "BITMAPINFOHEADER says %u.\n", t->tnum, t->v_height, u);
              if (t->v_height == 0)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->bi_compression, 4);

            if (t->v_frate == 0.0) {
              if (verbose)
                mxwarn(PFX "(MS compatibility "
                       "mode, track %u) No VideoFrameRate/DefaultDuration "
                       "element was found.\n", t->tnum);
              continue;
            }
          }
        }

        if (t->v_width == 0) {
          if (verbose)
            mxwarn(PFX "The width for track %u was not set.\n",
                   t->tnum);
          continue;
        }
        if (t->v_height == 0) {
          if (verbose)
            mxwarn(PFX "The height for track %u was not set.\n",
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
              mxwarn(PFX "CodecID for track %u is '"
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
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u samples per"
                       " second, but WAVEFORMATEX says that there are %u.\n",
                       t->tnum, (uint32_t)t->a_sfreq, u);
              if (t->a_sfreq == 0.0)
                t->a_sfreq = (float)u;
            }

            u = get_uint16(&wfe->n_channels);
            if (t->a_channels != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u channels, "
                       "but the WAVEFORMATEX says that there are %u.\n",
                       t->tnum, t->a_channels, u);
              if (t->a_channels == 0)
                t->a_channels = u;
            }

            u = get_uint16(&wfe->w_bits_per_sample);
            if (t->a_bps != u) {
              if (verbose)
                mxwarn(PFX "(MS compatibility mode for "
                       "track %u) Matroska says that there are %u bits per "
                       "sample, but the WAVEFORMATEX says that there are %u."
                       "\n", t->tnum, t->a_bps, u);
              if (t->a_bps == 0)
                t->a_bps = u;
            }

            t->a_formattag = get_uint16(&wfe->w_format_tag);
          }
        } else {
          if (!strncmp(t->codec_id, MKV_A_MP3, strlen(MKV_A_MP3) - 1))
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
                mxwarn(PFX "CodecID for track %u is "
                       "'A_VORBIS', but there are no header packets present.",
                       t->tnum);
              continue;
            }

            c = (unsigned char *)t->private_data;
            if (c[0] != 2) {
              if (verbose)
                mxwarn(PFX "Vorbis track does not "
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
                  mxwarn(PFX "Vorbis track does not "
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
          else if (!strncmp(t->codec_id, MKV_A_REAL_COOK, strlen("A_REAL/")))
            t->a_formattag = FOURCC('r', 'e', 'a', 'l');
          else if (!strcmp(t->codec_id, MKV_A_FLAC)) {
#if defined(HAVE_FLAC_FORMAT_H)
            t->a_formattag = FOURCC('f', 'L', 'a', 'C');
#else
            mxwarn(PFX "mkvmerge was not compiled with FLAC support. "
                   "Ignoring track %u.\n", t->tnum);
            continue;
#endif
          } else {
            if (verbose)
              mxwarn(PFX "Unknown/unsupported audio "
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
            mxwarn(PFX "The audio format tag was not "
                   "set for track %u.\n", t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 's':
        if (!strcmp(t->codec_id, MKV_S_VOBSUB)) {
          if (t->private_data == NULL) {
            if (verbose)
              mxwarn(PFX "CodecID for track %u is '%s', but there was no "
                     "private data found.\n", t->tnum, t->codec_id);
            continue;
          }
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

void kax_reader_c::handle_attachments(mm_io_c *io, EbmlStream *es,
                                      EbmlElement *l0, int64_t pos) {
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

void kax_reader_c::handle_chapters(mm_io_c *io, EbmlStream *es,
                                   EbmlElement *l0, int64_t pos) {
  KaxChapters *chapters;
  EbmlElement *l1, *l2;
  int upper_lvl_el;

  if (ti->no_chapters || (kax_chapters != NULL))
    return;

  io->save_pos(pos);
  l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true);

  if ((l1 != NULL) && (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId)) {
    chapters = (KaxChapters *)l1;
    l2 = NULL;
    upper_lvl_el = 0;
    chapters->Read(*es, KaxChapters::ClassInfos.Context, upper_lvl_el,
                   l2, true);
    kax_chapters = copy_chapters(chapters);

    delete l1;
  } else if (l1 != NULL)
    delete l1;

  io->restore_pos();
}

// }}}

// {{{ FUNCTION kax_reader_c::read_headers()

#define in_parent(p) (in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))
#define FINDFIRST(p, c) (static_cast<c *> \
  (((EbmlMaster *)p)->FindFirstElt(c::ClassInfos, false)))
#define FINDNEXT(p, c, e) (static_cast<c *> \
  (((EbmlMaster *)p)->FindNextElt(*e, false)))

int kax_reader_c::read_headers() {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  kax_track_t *track;
  bool exit_loop;

  exit_loop = false;
  try {
    in = new mm_io_c(ti->fname, MODE_READ);
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
          segment_duration = float(*kduration) * tc_scale / 1000000000.0;
          if (verbose > 1)
            mxinfo(PFX "| + duration: %.3fs\n", segment_duration);
        }

        ktitle = FINDFIRST(l1, KaxTitle);
        if (ktitle != NULL) {
          char *tmp;
          tmp = UTFstring_to_cstr(UTFstring(*ktitle));
          if (verbose > 1)
            mxinfo(PFX "| + title: %s\n", tmp);
          safefree(tmp);
          tmp = UTFstring_to_cstrutf8(UTFstring(*ktitle));
          title = tmp;
          safefree(tmp);
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
          KaxTrackLanguage *ktlanguage;
          KaxTrackMinCache *ktmincache;
          KaxTrackName *ktname;
          KaxContentEncodings *kcencodings;
          int kcenc_idx;
          vector<kax_content_encoding_t>::iterator ce_ins_it;

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

          }

          kcodecid = FINDFIRST(ktentry, KaxCodecID);
          if (kcodecid != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Codec ID: %s\n", string(*kcodecid).c_str());
            track->codec_id = safestrdup(string(*kcodecid).c_str());
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
          }

          ktfdefault = FINDFIRST(ktentry, KaxTrackFlagDefault);
          if (ktfdefault != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Default flag: %d\n", uint32(*ktfdefault));
            track->default_track = uint32(*ktfdefault);
          }

          ktlanguage = FINDFIRST(ktentry, KaxTrackLanguage);
          if (ktlanguage != NULL) {
            if (verbose > 1)
              mxinfo(PFX "|  + Language: %s\n", string(*ktlanguage).c_str());
            safefree(track->language);
            track->language = safestrdup(string(*ktlanguage).c_str());
          }

          ktname = FINDFIRST(ktentry, KaxTrackName);
          if (ktname != NULL) {
            char *tmp;
            safefree(track->track_name);
            track->track_name =
              UTFstring_to_cstrutf8(UTFstring(*ktname));
            tmp = UTFstring_to_cstr(UTFstring(*ktname));
            if (verbose > 1)
              mxinfo(PFX "|  + Name: %s\n", tmp);
            safefree(tmp);
          }

          kcencodings = FINDFIRST(ktentry, KaxContentEncodings);
          if (kcencodings != NULL) {
            for (kcenc_idx = 0; kcenc_idx < kcencodings->ListSize();
                 kcenc_idx++) {
              l3 = (*kcencodings)[kcenc_idx];
              if (EbmlId(*l3) == KaxContentEncoding::ClassInfos.GlobalId) {
                KaxContentEncoding *kcenc;
                KaxContentEncodingOrder *ce_order;
                KaxContentEncodingType *ce_type;
                KaxContentEncodingScope *ce_scope;
                KaxContentCompression *ce_comp;
                KaxContentEncryption *ce_enc;
                kax_content_encoding_t enc;

                memset(&enc, 0, sizeof(kax_content_encoding_t));
                kcenc = static_cast<KaxContentEncoding *>(l3);

                ce_order = FINDFIRST(kcenc, KaxContentEncodingOrder);
                if (ce_order != NULL)
                  enc.order = uint32(*ce_order);

                ce_type = FINDFIRST(kcenc, KaxContentEncodingType);
                if (ce_type != NULL)
                  enc.type = uint32(*ce_type);

                ce_scope = FINDFIRST(kcenc, KaxContentEncodingScope);
                if (ce_scope != NULL)
                  enc.scope = uint32(*ce_scope);
                else
                  enc.scope = 1;

                ce_comp = FINDFIRST(kcenc, KaxContentCompression);
                if (ce_comp != NULL) {
                  KaxContentCompAlgo *cc_algo;
                  KaxContentCompSettings *cc_settings;

                  cc_algo = FINDFIRST(ce_comp, KaxContentCompAlgo);
                  if (cc_algo != NULL)
                    enc.comp_algo = uint32(*cc_algo);

                  cc_settings = FINDFIRST(ce_comp, KaxContentCompSettings);
                  if (cc_settings != NULL) {
                    enc.comp_settings =
                      (unsigned char *)safememdup(&binary(*cc_settings),
                                                  cc_settings->GetSize());
                    enc.comp_settings_len = cc_settings->GetSize();
                  }
                }

                ce_enc = FINDFIRST(kcenc, KaxContentEncryption);
                if (ce_enc != NULL) {
                  KaxContentEncAlgo *ce_ealgo;
                  KaxContentEncKeyID *ce_ekeyid;
                  KaxContentSigAlgo *ce_salgo;
                  KaxContentSigHashAlgo *ce_shalgo;
                  KaxContentSigKeyID *ce_skeyid;
                  KaxContentSignature *ce_signature;

                  ce_ealgo = FINDFIRST(ce_enc, KaxContentEncAlgo);
                  if (ce_ealgo != NULL)
                    enc.enc_algo = uint32(*ce_ealgo);

                  ce_ekeyid = FINDFIRST(ce_enc, KaxContentEncKeyID);
                  if (ce_ekeyid != NULL) {
                    enc.enc_keyid =
                      (unsigned char *)safememdup(&binary(*ce_ekeyid),
                                                  ce_ekeyid->GetSize());
                    enc.enc_keyid_len = ce_ekeyid->GetSize();
                  }

                  ce_salgo = FINDFIRST(ce_enc, KaxContentSigAlgo);
                  if (ce_salgo != NULL)
                    enc.enc_algo = uint32(*ce_salgo);

                  ce_shalgo = FINDFIRST(ce_enc, KaxContentSigHashAlgo);
                  if (ce_shalgo != NULL)
                    enc.enc_algo = uint32(*ce_shalgo);

                  ce_skeyid = FINDFIRST(ce_enc, KaxContentSigKeyID);
                  if (ce_skeyid != NULL) {
                    enc.sig_keyid =
                      (unsigned char *)safememdup(&binary(*ce_skeyid),
                                                  ce_skeyid->GetSize());
                    enc.sig_keyid_len = ce_skeyid->GetSize();
                  }

                  ce_signature = FINDFIRST(ce_enc, KaxContentSignature);
                  if (ce_signature != NULL) {
                    enc.signature =
                      (unsigned char *)safememdup(&binary(*ce_signature),
                                                  ce_signature->GetSize());
                    enc.signature_len = ce_signature->GetSize();
                  }

                }

                ce_ins_it = track->c_encodings->begin();
                while ((ce_ins_it != track->c_encodings->end()) &&
                       (enc.order <= (*ce_ins_it).order))
                  ce_ins_it++;
                track->c_encodings->insert(ce_ins_it, enc);
              }
            }
          }

          ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
        } // while (ktentry != NULL)
          
        l1->SkipData(*es, l1->Generic().Context);

      } else if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)
        handle_attachments(in, es, l0, l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId)
        handle_chapters(in, es, l0, l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        int i, k;
        EbmlElement *el;
        KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);
        int64_t pos;
        bool is_attachments, is_chapters;

        seek_head.Read(*es, KaxSeekHead::ClassInfos.Context, i, el, true);
        for (i = 0; i < seek_head.ListSize(); i++)
          if (EbmlId(*seek_head[i]) == KaxSeek::ClassInfos.GlobalId) {
            KaxSeek &seek = *static_cast<KaxSeek *>(seek_head[i]);
            pos = -1;
            is_attachments = false;
            is_chapters = false;

            for (k = 0; k < seek.ListSize(); k++)
              if (EbmlId(*seek[k]) == KaxSeekID::ClassInfos.GlobalId) {
                KaxSeekID &sid = *static_cast<KaxSeekID *>(seek[k]);
                EbmlId id(sid.GetBuffer(), sid.GetSize());
                if (id == KaxAttachments::ClassInfos.GlobalId)
                  is_attachments = true;
                else if (id == KaxChapters::ClassInfos.GlobalId)
                  is_chapters = true;

              } else if (EbmlId(*seek[k]) ==
                         KaxSeekPosition::ClassInfos.GlobalId)
                pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

            if (pos != -1) {
              if (is_attachments)
                handle_attachments(in, es, l0,
                                   ((KaxSegment *)l0)->GetGlobalPosition(pos));
              else if (is_chapters)
                handle_chapters(in, es, l0,
                                ((KaxSegment *)l0)->GetGlobalPosition(pos));
            }
          }

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        if (verbose > 1)
          mxinfo(PFX "|+ found cluster, headers are "
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
    mxerror(PFX "caught exception\n");
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
    if (nti.language == NULL)
      nti.language = t->language;
    if (nti.track_name == NULL)
      nti.track_name = t->track_name;

    if (t->ok && demuxing_requested(t->type, t->tnum)) {
      nti.id = t->tnum;         // ID for this track.
      switch (t->type) {

        case 'v':
          if (nti.fourcc[0] == 0)
            memcpy(nti.fourcc, t->v_fourcc, 5);
          if (verbose)
            mxinfo("+-> Using video output module for track ID %u.\n",
                   t->tnum);
          t->packetizer = new video_packetizer_c(this, t->codec_id, t->v_frate,
                                                 t->v_width,
                                                 t->v_height,
                                                 t->v_bframes, &nti);
          if (!nti.aspect_ratio_given) { // The user didn't set it.
            if (t->v_dwidth != 0)
              t->packetizer->set_video_display_width(t->v_dwidth);
            if (t->v_dheight != 0)
              t->packetizer->set_video_display_height(t->v_dheight);
          }
          break;

        case 'a':
          if (t->a_formattag == 0x0001) {
            t->packetizer = new pcm_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, t->a_bps,
                                                 &nti);
            if (verbose)
              mxinfo("+-> Using the PCM output module for track ID %u.\n",
                     t->tnum);
          } else if (t->a_formattag == 0x0055) {
            t->packetizer = new mp3_packetizer_c(this,
                                                 (unsigned long)t->a_sfreq,
                                                 t->a_channels, &nti);
            if (verbose)
              mxinfo("+-> Using the MPEG audio output module for track ID %u."
                     "\n", t->tnum);
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
              mxinfo("+-> Using the AC3 output module for track ID %u.\n",
                     t->tnum);
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
              mxinfo("+-> Using the Vorbis output module for track ID %u.\n",
                     t->tnum);
          } else if (t->a_formattag == FOURCC('M', 'P', '4', 'A')) {
            // A_AAC/MPEG2/MAIN
            // 0123456789012345
            int id, profile, sbridx;

            id = 0;
            profile = 0;
            if (t->codec_id[10] == '2')
              id = AAC_ID_MPEG2;
            else if (t->codec_id[10] == '4')
              id = AAC_ID_MPEG4;
            else
              mxerror(PFX "Malformed codec id '%s' for track %d.\n",
                      t->codec_id, t->tnum);

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
              mxerror(PFX "Malformed codec id %s for track %d.\n",
                      t->codec_id, t->tnum);

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
              mxinfo("+-> Using the AAC output module for track ID %u.\n",
                     t->tnum);

          } else if (t->a_formattag == FOURCC('r', 'e', 'a', 'l')) {
            passthrough_packetizer_c *ptzr;

            ptzr = new passthrough_packetizer_c(this, ti);
            t->packetizer = ptzr;

            ptzr->set_track_type(track_audio);
            ptzr->set_codec_id(t->codec_id);
            ptzr->set_codec_private((unsigned char *)t->private_data,
                                    t->private_size);
            ptzr->set_audio_sampling_freq(t->a_sfreq);
            ptzr->set_audio_channels(t->a_channels);
            if (t->a_bps != 0)
              ptzr->set_audio_bit_depth(t->a_bps);

            if (verbose)
              mxinfo("+-> Using the generic audio output module for stream "
                     "%u (CodecID: %s).\n", t->tnum, t->codec_id);
#if defined(HAVE_FLAC_FORMAT_H)
          } else if ((t->a_formattag == FOURCC('f', 'L', 'a', 'C')) ||
                     (t->a_formattag == 0xf1ac)) {
            nti.private_data = NULL;
            nti.private_size = 0;
            if (t->a_formattag == FOURCC('f', 'L', 'a', 'C'))
              t->packetizer =
                new flac_packetizer_c(this, (int)t->a_sfreq, t->a_channels,
                                      t->a_bps,
                                      (unsigned char *)t->private_data,
                                      t->private_size, &nti);
            else
              t->packetizer =
                new flac_packetizer_c(this, (int)t->a_sfreq, t->a_channels,
                                      t->a_bps,
                                      ((unsigned char *)t->private_data) +
                                      sizeof(alWAVEFORMATEX),
                                      t->private_size - sizeof(alWAVEFORMATEX),
                                      &nti);
            if (verbose)
              mxinfo("+-> Using the FLAC output module for track ID %u.\n",
                     t->tnum);

#endif
          } else
            mxerror(PFX "Unsupported audio track %s for track %d.\n",
                    t->codec_id, t->tnum);

          if ((t->packetizer != NULL) && (t->a_osfreq != 0.0))
            t->packetizer->set_audio_output_sampling_freq(t->a_osfreq);

          break;

        case 's':
          if (!strcmp(t->codec_id, MKV_S_VOBSUB)) {
            t->packetizer =
              new vobsub_packetizer_c(this, t->private_data, t->private_size,
                                      false, &nti);
            if (verbose)
              mxinfo("+-> Using the VobSub output module for track ID %u.\n",
                     t->tnum);

            t->sub_type = 'v';

          } else {
            t->packetizer = new textsubs_packetizer_c(this, t->codec_id,
                                                      t->private_data,
                                                      t->private_size, false,
                                                      true, &nti);
            if (verbose)
              mxinfo("+-> Using the text subtitle output module for track ID "
                     "%u.\n", t->tnum);

            t->sub_type = 't';
          }
          break;

        default:
          mxerror(PFX "Unsupported track type for track %d.\n", t->tnum);
          break;
      }
      if (t->tuid != 0)
        if (!t->packetizer->set_uid(t->tuid))
          mxwarn(PFX "Could not keep the track "
                 "UID %u because it is already allocated for the new "
                 "file.\n", t->tuid);
    }
  }

  if (segment_title.length() == 0)
    segment_title = title;
}

// }}}

// {{{ FUNCTION kax_reader_c::read()

int kax_reader_c::read(generic_packetizer_c *) {
  int upper_lvl_el, i, bgidx;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  KaxBlockGroup *block_group;
  KaxBlock *block;
  KaxReferenceBlock *ref_block;
  KaxBlockDuration *duration;
  KaxClusterTimecode *ctc;
  int64_t block_fref, block_bref, block_duration;
  kax_track_t *block_track;
  bool found_data, bref_found, fref_found;

  if (tracks.size() == 0)
    return 0;

  l0 = segment;

  if (saved_l1 == NULL)         // We're done.
    return 0;

  if (get_queued_bytes() > 20 * 1024 * 1024)
    return EHOLDING;

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
            if (int64(*ref_block) < 0) {
              block_bref = int64(*ref_block) * tc_scale / 1000000;
              bref_found = true;
            } else {
              block_fref = int64(*ref_block) * tc_scale / 1000000;
              fref_found = true;
            }

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
            if ((block_track->type == 's') && (block_duration == -1) &&
                (block_track->sub_type == 't')) {
              mxwarn("Text subtitle block does not "
                     "contain a block duration element. This file is "
                     "broken. Assuming a duration of 1000ms.\n");
              block_duration = 1000;
            }

            last_timecode = block->GlobalTimecode() / 1000000;
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
              re_modified = reverse_encodings(block_track, re_buffer, re_size,
                                              1);
              if ((block_track->type == 's') &&
                  (block_track->sub_type == 't')) {
                if ((re_size > 2) ||
                    ((re_size > 0) && (*re_buffer != ' ') &&
                     (*re_buffer != 0) && !iscr(*re_buffer))) {
                  char *lines;

                  lines = (char *)safemalloc(re_size + 1);
                  lines[re_size] = 0;
                  memcpy(lines, re_buffer, re_size);
                  block_track->packetizer->process((unsigned char *)lines, 0,
                                                   (int64_t)last_timecode,
                                                   block_duration,
                                                   block_bref,
                                                   block_fref);
                  safefree(lines);
                }
              } else
                block_track->packetizer->process(re_buffer, re_size,
                                                 (int64_t)last_timecode,
                                                 block_duration,
                                                 block_bref,
                                                 block_fref);
              if (re_modified)
                safefree(re_buffer);
            }

            block_track->previous_timecode = (int64_t)last_timecode;
            block_track->units_processed += block->NumberFrames();
          }

          found_data = true;
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
    return 0;
  }

  debug_leave("kax_reader_c::read");

  if (found_data)
    return EMOREDATA;
  else
    return 0;
}

// }}}

// {{{ FUNCTIONS display_*, set_headers()

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

void kax_reader_c::display_progress(bool final) {
  int i;

  if (segment_duration != 0.0) {
    if (final)
      mxinfo("progress: %.3fs/%.3fs (100%%)\r", segment_duration,
             segment_duration);
    else
      mxinfo("progress: %.3fs/%.3fs (%d%%)\r",
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
  string info;
  char *str;

  if (identify_verbose && (title.length() > 0))
    mxinfo("File '%s': container: Matroska [title:%s]\n", ti->fname,
           escape(title.c_str()).c_str());
  else
    mxinfo("File '%s': container: Matroska\n", ti->fname);
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ok) {
      if (identify_verbose) {
        info = " [";
        if (tracks[i]->language != NULL)
          info += string("language:") + escape(tracks[i]->language) +
            string(" ");
        if (tracks[i]->track_name != NULL)
          info += string("track_name:") + escape(tracks[i]->track_name) +
            string(" ");
        info += "]";
      } else
        info = "";
      mxinfo("Track ID %d: %s (%s%s%s)%s\n", tracks[i]->tnum,
             tracks[i]->type == 'v' ? "video" :
             tracks[i]->type == 'a' ? "audio" :
             tracks[i]->type == 's' ? "subtitles" : "unknown",
             tracks[i]->codec_id,
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
    if (attachments[i].description.length() > 0) {
      str = UTFstring_to_cstr(attachments[i].description.c_str());
      mxinfo("description '%s', ", str);
      safefree(str);
    }
    if (attachments[i].name.length() == 0)
      mxinfo("no file name given\n");
    else {
      str = UTFstring_to_cstr(attachments[i].name.c_str());
      mxinfo("file name '%s'\n", str);
      safefree(str);
    }
  }
}

// }}}

int64_t kax_reader_c::get_queued_bytes() {
  int64_t bytes;
  uint32_t i;

  bytes = 0;
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->packetizer != NULL)
      bytes += tracks[i]->packetizer->get_queued_bytes();

  return bytes;
}

void kax_reader_c::add_attachments(KaxAttachments *a) {
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

bool kax_reader_c::reverse_encodings(kax_track_t *track, unsigned char *&data,
                                     uint32_t &size, uint32_t type) {
  int new_size;
  unsigned char *new_data, *old_data;
  bool modified;
  vector<kax_content_encoding_t>::iterator ce;
  compression_c *compressor;

  if (track->c_encodings->size() == 0)
    return false;

  new_data = data;
  new_size = size;
  modified = false;
  for (ce = track->c_encodings->begin(); ce < track->c_encodings->end();
       ce++) {
    if ((ce->scope & type) == 0)
      continue;

    if (ce->comp_algo == 0)
      compressor = track->zlib_compressor;
    else if (ce->comp_algo == 1)
      compressor = track->bzlib_compressor;
    else
      compressor = track->lzo1x_compressor;

    old_data = new_data;
    new_data = compressor->decompress(old_data, new_size);
    if (modified)
      safefree(old_data);
    modified = true;
  }

  data = new_data;
  size = new_size;

  return modified;
}
