/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/os.h"

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

#include "common/chapters/chapters.h"
#include "common/common.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/tags/tags.h"
#include "input/r_matroska.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_avc.h"
#include "output/p_dirac.h"
#include "output/p_dts.h"
#if defined(HAVE_FLAC_FORMAT_H)
# include "output/p_flac.h"
#endif
#include "output/p_kate.h"
#include "output/p_mp3.h"
#include "output/p_mpeg1_2.h"
#include "output/p_mpeg4_p2.h"
#include "output/p_mpeg4_p10.h"
#include "output/p_passthrough.h"
#include "output/p_pcm.h"
#include "output/p_textsubs.h"
#include "output/p_theora.h"
#include "output/p_tta.h"
#include "output/p_video.h"
#include "output/p_vobbtn.h"
#include "output/p_vobsub.h"
#include "output/p_vorbis.h"
#include "output/p_wavpack.h"

using namespace libmatroska;

#define MAP_TRACK_TYPE(c) (  (c) == 'a' ? track_audio   \
                           : (c) == 'b' ? track_buttons \
                           : (c) == 'v' ? track_video   \
                           :              track_subtitle)
#define MAP_TRACK_TYPE_STRING(c) (  (c) == '?' ? Y("unknown")   \
                                  : (c) == 'a' ? Y("audio")     \
                                  : (c) == 'b' ? Y("buttons")   \
                                  : (c) == 'v' ? Y("video")     \
                                  :              Y("subtitle"))

#define in_parent(p) \
  (!p->IsFiniteSize() || (in->getFilePointer() < (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

#define is_ebmlvoid(e) (EbmlId(*e) == EbmlVoid::ClassInfos.GlobalId)

#define MAGIC_MKV 0x1a45dfa3

/*
   Probes a file by simply comparing the first four bytes to the EBML
   head signature.
*/
int
kax_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  unsigned char data[4];

  if (4 > size)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(data, 4) != 4)
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (get_uint32_be(data) != MAGIC_MKV)
    return 0;
  return 1;
}

kax_reader_c::kax_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
  , segment_duration(0)
  , first_timecode(-1)
  , writing_app_ver(-1)
  , m_attachment_id(0)
  , m_tags(NULL)
{
  if (!read_headers())
    throw error_c(Y("matroska_reader: Failed to read the headers."));
  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the Matroska demultiplexer.\n"));
}

kax_reader_c::~kax_reader_c() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    delete tracks[i];

  delete saved_l1;
  delete in;
  delete es;
  delete segment;
  delete m_tags;
}

int
kax_reader_c::packets_available() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((-1 != tracks[i]->ptzr) && !PTZR(tracks[i]->ptzr)->packet_available())
      return 0;

  if (tracks.empty())
    return 0;

  return 1;
}

kax_track_t *
kax_reader_c::new_kax_track() {
  // Set some default values.
  kax_track_t *t   = new kax_track_t;
  t->default_track = true;
  t->ptzr          = -1;

  tracks.push_back(t);

  return t;
}

kax_track_t *
kax_reader_c::find_track_by_num(uint64_t n,
                                kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((NULL != tracks[i]) && (tracks[i]->tnum == n) && (tracks[i] != c))
      return tracks[i];

  return NULL;
}

kax_track_t *
kax_reader_c::find_track_by_uid(uint64_t uid,
                                kax_track_t *c) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((NULL != tracks[i]) && (tracks[i]->tuid == uid) && (tracks[i] != c))
      return tracks[i];

  return NULL;
}

void
kax_reader_c::verify_tracks() {
  int tnum, u;
  kax_track_t *t;

  for (tnum = 0; tnum < tracks.size(); tnum++) {
    t = tracks[tnum];

    t->ok = t->content_decoder.is_ok();

    if (!t->ok)
      continue;
    t->ok = 0;

    if (NULL != t->private_data) {
      memory_cptr private_data(new memory_c((unsigned char *)t->private_data, t->private_size, true));
      t->content_decoder.reverse(private_data, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
      private_data->lock();
      t->private_data = private_data->get_buffer();
      t->private_size = private_data->get_size();
    }

    switch (t->type) {
      case 'v':                 // video track
        if (t->codec_id == "")
          continue;
        if (t->codec_id == MKV_V_MSCOMP) {
          if ((NULL == t->private_data) || (sizeof(alBITMAPINFOHEADER) > t->private_size)) {
            if (verbose)
              mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no BITMAPINFOHEADER struct present. "
                                     "Therefore we don't have a FourCC to identify the video codec used.\n"))
                     % t->tnum % MKV_V_MSCOMP);
            continue;

          } else {
            t->ms_compat            = 1;
            alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)t->private_data;
            u                       = get_uint32_le(&bih->bi_width);

            if (t->v_width != u) {
              if (verbose)
                mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode, track %1%) Matrosa says video width is %2%, but the BITMAPINFOHEADER says %3%.\n"))
                       % t->tnum % t->v_width % u);
              if (0 == t->v_width)
                t->v_width = u;
            }

            u = get_uint32_le(&bih->bi_height);
            if (t->v_height != u) {
              if (verbose)
                mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode, track %1%) Matrosa video height is %2%, but the BITMAPINFOHEADER says %3%.\n"))
                       % t->tnum % t->v_height % u);
              if (0 == t->v_height)
                t->v_height = u;
            }

            memcpy(t->v_fourcc, &bih->bi_compression, 4);
          }

        } else if (t->codec_id == MKV_V_THEORA) {
          if (NULL == t->private_data) {
            if (verbose)
              mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no codec private headers.\n")) % t->tnum % MKV_V_THEORA);
            continue;
          }
        }

        if (0 == t->v_width) {
          if (verbose)
            mxwarn(boost::format(Y("matroska_reader: The width for track %1% was not set.\n")) % t->tnum);
          continue;
        }

        if (0 == t->v_height) {
          if (verbose)
            mxwarn(boost::format(Y("matroska_reader: The height for track %1% was not set.\n")) % t->tnum);
          continue;
        }

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 'a':                 // audio track
        if (t->codec_id == "")
          continue;

        if (t->codec_id == MKV_A_ACM) {
          if ((NULL == t->private_data) || (sizeof(alWAVEFORMATEX) > t->private_size)) {
            if (verbose)
              mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no WAVEFORMATEX struct present. "
                                     "Therefore we don't have a format ID to identify the audio codec used.\n")) % t->tnum % MKV_A_ACM);
            continue;

          } else {
            t->ms_compat        = 1;
            alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)t->private_data;
            t->a_formattag      = get_uint16_le(&wfe->w_format_tag);
            u                   = get_uint32_le(&wfe->n_samples_per_sec);

            if (((uint32_t)t->a_sfreq) != u) {
              if (verbose)
                mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode for track %1%) Matroska says that there are %2% samples per second, "
                                       "but WAVEFORMATEX says that there are %3%.\n")) % t->tnum % (int)t->a_sfreq % u);
              if (0.0 == t->a_sfreq)
                t->a_sfreq = (float)u;
            }

            u = get_uint16_le(&wfe->n_channels);
            if (t->a_channels != u) {
              if (verbose)
                mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode for track %1%) Matroska says that there are %2% channels, "
                                       "but the WAVEFORMATEX says that there are %3%.\n")) % t->tnum % t->a_channels % u);
              if (0 == t->a_channels)
                t->a_channels = u;
            }

            u = get_uint16_le(&wfe->w_bits_per_sample);
            if (t->a_bps != u) {
              if (verbose && ((0x0001 == t->a_formattag) || (0x0003 == t->a_formattag)))
                mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode for track %1%) Matroska says that there are %2% bits per sample, "
                                       "but the WAVEFORMATEX says that there are %3%.\n")) % t->tnum % t->a_bps % u);
              if (0 == t->a_bps)
                t->a_bps = u;
            }
          }

        } else {
          if (starts_with(t->codec_id, MKV_A_MP3, strlen(MKV_A_MP3) - 1))
            t->a_formattag = 0x0055;
          else if (starts_with(t->codec_id, MKV_A_AC3, strlen(MKV_A_AC3)) || (t->codec_id == MKV_A_EAC3))
            t->a_formattag = 0x2000;
          else if (t->codec_id == MKV_A_DTS)
            t->a_formattag = 0x2001;
          else if (t->codec_id == MKV_A_PCM)
            t->a_formattag = 0x0001;
          else if (t->codec_id == MKV_A_PCM_FLOAT)
            t->a_formattag = 0x0003;
          else if (t->codec_id == MKV_A_VORBIS) {
            if (NULL == t->private_data) {
              if (verbose)
                mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is 'A_VORBIS', but there are no header packets present.\n")) % t->tnum);
              continue;
            }

            try {
              memory_cptr temp(new memory_c((unsigned char *)t->private_data, t->private_size, false));
              std::vector<memory_cptr> blocks = unlace_memory_xiph(temp);
              if (blocks.size() != 3)
                throw false;

              t->headers[0]      = blocks[0]->get_buffer();
              t->headers[1]      = blocks[1]->get_buffer();
              t->headers[2]      = blocks[2]->get_buffer();
              t->header_sizes[0] = blocks[0]->get_size();
              t->header_sizes[1] = blocks[1]->get_size();
              t->header_sizes[2] = blocks[2]->get_size();

            } catch (...) {
              if (verbose)
                mxwarn(Y("matroska_reader: Vorbis track does not contain valid headers.\n"));
              continue;
            }

            t->a_formattag = 0xFFFE;

            // mkvmerge around 0.6.x had a bug writing a default duration
            // for Vorbis tracks but not the correct durations for the
            // individual packets. This comes back to haunt us because
            // when regenerating the timestamps from lacing those durations
            // are used and add up to too large a value. The result is the
            // usual "timecode < last_timecode" message.
            // Workaround: do not use durations for such tracks.
            if ((writing_app == "mkvmerge") && (-1 != writing_app_ver) && (0x07000000 > writing_app_ver))
              t->ignore_duration_hack = true;

          } else if (   (t->codec_id == MKV_A_AAC_2MAIN)
                     || (t->codec_id == MKV_A_AAC_2LC)
                     || (t->codec_id == MKV_A_AAC_2SSR)
                     || (t->codec_id == MKV_A_AAC_4MAIN)
                     || (t->codec_id == MKV_A_AAC_4LC)
                     || (t->codec_id == MKV_A_AAC_4SSR)
                     || (t->codec_id == MKV_A_AAC_4LTP)
                     || (t->codec_id == MKV_A_AAC_2SBR)
                     || (t->codec_id == MKV_A_AAC_4SBR)
                     || (t->codec_id == MKV_A_AAC))
            t->a_formattag = FOURCC('M', 'P', '4', 'A');
          else if (starts_with(t->codec_id, MKV_A_REAL_COOK, strlen("A_REAL/")))
            t->a_formattag = FOURCC('r', 'e', 'a', 'l');
          else if (t->codec_id == MKV_A_FLAC) {
#if defined(HAVE_FLAC_FORMAT_H)
            t->a_formattag = FOURCC('f', 'L', 'a', 'C');
#else
            mxwarn(boost::format(Y("matroska_reader: mkvmerge was not compiled with FLAC support. Ignoring track %1%.\n")) % t->tnum);
            continue;
#endif
          } else if (t->codec_id == MKV_A_TTA)
            t->a_formattag = FOURCC('T', 'T', 'A', '1');
          else if (t->codec_id == MKV_A_WAVPACK4)
            t->a_formattag = FOURCC('W', 'V', 'P', '4');
        }

        if (0.0 == t->a_sfreq)
          t->a_sfreq = 8000.0;

        if (0 == t->a_channels)
          t->a_channels = 1;

        // This track seems to be ok.
        t->ok = 1;

        break;

      case 's':
        if (t->codec_id == MKV_S_VOBSUB) {
          if (NULL == t->private_data) {
            if (verbose)
              mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no private data found.\n")) % t->tnum % t->codec_id);
            continue;
          }

        } else if (t->codec_id == MKV_S_KATE) {
          if (NULL == t->private_data) {
            if (verbose)
              mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no private data found.\n")) % t->tnum % t->codec_id);
            continue;
          }
        }
        t->ok = 1;
        break;

      case 'b':
        if (t->codec_id != MKV_B_VOBBTN) {
          if (verbose)
            mxwarn(boost::format(Y("matroska_reader: The CodecID '%1%' for track %2% is unknown.\n")) % t->codec_id % t->tnum);
          continue;
        }
        t->ok = 1;
        break;

      default:                  // unknown track type!? error in demuxer...
        if (verbose)
          mxwarn(boost::format(Y("matroska_reader: unknown demuxer type for track %1%: '%2%'\n")) % t->tnum % t->type);
        continue;
    }

    if (t->ok && (1 < verbose))
      mxinfo(boost::format(Y("matroska_reader: Track %1% seems to be ok.\n")) % t->tnum);
  }
}

void
kax_reader_c::handle_attachments(mm_io_c *io,
                                 EbmlElement *l0,
                                 int64_t pos) {
  bool found = false;

  int i;
  for (i = 0; i < handled_attachments.size(); i++)
    if (handled_attachments[i] == pos) {
      found = true;
      break;
    }

  if (found)
    return;

  handled_attachments.push_back(pos);

  io->save_pos(pos);
  int upper_lvl_el;
  EbmlElement *l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

  if ((NULL != l1) && (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)) {
    KaxAttachments *atts = (KaxAttachments *)l1;
    EbmlElement *l2      = NULL;
    upper_lvl_el         = 0;

    atts->Read(*es, KaxAttachments::ClassInfos.Context, upper_lvl_el, l2, true);

    for (i = 0; i < atts->ListSize(); i++) {
      KaxAttached *att = (KaxAttached *)(*atts)[i];

      if (EbmlId(*att) == KaxAttached::ClassInfos.GlobalId) {
        UTFstring name        = L"";
        UTFstring description = L"";
        std::string mime_type =  "";
        int64_t size          = -1;
        int64_t id            = -1;
        unsigned char *data   = NULL;
        int k;

        for (k = 0; k < att->ListSize(); k++) {
          l2 = (*att)[k];

          if (EbmlId(*l2) == KaxFileName::ClassInfos.GlobalId) {
            KaxFileName &fname = *static_cast<KaxFileName *>(l2);
            name               = UTFstring(fname);

          } else if (EbmlId(*l2) == KaxFileDescription::ClassInfos.GlobalId) {
            KaxFileDescription &fdesc = *static_cast<KaxFileDescription *>(l2);
            description               = UTFstring(fdesc);

          } else if (EbmlId(*l2) == KaxMimeType::ClassInfos.GlobalId) {
            KaxMimeType &mtype = *static_cast<KaxMimeType *>(l2);
            mime_type          = std::string(mtype);

          } else if (EbmlId(*l2) == KaxFileUID::ClassInfos.GlobalId) {
            KaxFileUID &fuid = *static_cast<KaxFileUID *>(l2);
            id               = uint64(fuid);

          } else if (EbmlId(*l2) == KaxFileData::ClassInfos.GlobalId) {
            KaxFileData &fdata = *static_cast<KaxFileData *>(l2);
            size               = fdata.GetSize();
            data               = (unsigned char *)fdata.GetBuffer();
          }
        }

        ++m_attachment_id;
        attach_mode_e attach_mode;
        if ((-1 != id) && (-1 != size) && !mime_type.empty() && (0 != name.length()) && ((attach_mode = attachment_requested(m_attachment_id)) != ATTACH_MODE_SKIP)) {

          attachment_t matt;
          matt.name           = UTFstring_to_cstrutf8(name);
          matt.mime_type      = mime_type;
          matt.description    = UTFstring_to_cstrutf8(description);
          matt.id             = id;
          matt.ui_id          = m_attachment_id;
          matt.to_all_files   = ATTACH_MODE_TO_ALL_FILES == attach_mode;
          matt.data           = clone_memory(data, size);

          add_attachment(matt);
        }
      }
    }
  }

  delete l1;

  io->restore_pos();
}

void
kax_reader_c::handle_chapters(mm_io_c *io,
                              EbmlElement *l0,
                              int64_t pos) {
  bool found = false;
  int i;
  for (i = 0; i < handled_chapters.size(); i++)
    if (handled_chapters[i] == pos) {
      found = true;
      break;
    }

  if (found)
    return;

  handled_chapters.push_back(pos);

  int upper_lvl_el = 0;
  io->save_pos(pos);
  EbmlElement *l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

  if ((l1 != NULL) && is_id(l1, KaxChapters)) {
    KaxChapters *tmp_chapters = static_cast<KaxChapters *>(l1);
    EbmlElement *l2           = NULL;
    upper_lvl_el              = 0;

    tmp_chapters->Read(*es, KaxChapters::ClassInfos.Context, upper_lvl_el, l2, true);

    if (NULL == chapters)
      chapters = new KaxChapters;

    for (i = 0; i < tmp_chapters->ListSize(); i++)
      chapters->PushElement(*(*tmp_chapters)[i]);
    tmp_chapters->RemoveAll();

  }

  delete l1;

  io->restore_pos();
}

void
kax_reader_c::handle_tags(mm_io_c *io,
                          EbmlElement *l0,
                          int64_t pos) {
  bool found = false;
  int i;
  for (i = 0; i < handled_tags.size(); i++)
    if (handled_tags[i] == pos) {
      found = true;
      break;
    }

  if (found)
    return;

  handled_tags.push_back(pos);

  int upper_lvl_el = 0;
  io->save_pos(pos);
  EbmlElement *l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

  if ((NULL != l1) && (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId)) {
    KaxTags *tags   = (KaxTags *)l1;
    EbmlElement *l2 = NULL;
    upper_lvl_el    = 0;

    tags->Read(*es, KaxTags::ClassInfos.Context, upper_lvl_el, l2, true);

    while (tags->ListSize() > 0) {
      if (!(EbmlId(*(*tags)[0]) == KaxTag::ClassInfos.GlobalId)) {
        delete (*tags)[0];
        tags->Remove(0);
        continue;
      }

      bool delete_tag       = true;
      bool is_global        = true;
      KaxTag *tag           = static_cast<KaxTag *>((*tags)[0]);
      KaxTagTargets *target = FINDFIRST(tag, KaxTagTargets);

      if (NULL != target) {
        KaxTagTrackUID *tuid = FINDFIRST(target, KaxTagTrackUID);

        if (NULL != tuid) {
          is_global          = false;
          kax_track_t *track = find_track_by_uid(uint64(*tuid));

          if (NULL != track) {
            bool contains_tag = false;

            for (i = 0; i < tag->ListSize(); i++)
              if (dynamic_cast<KaxTagSimple *>((*tag)[i]) != NULL) {
                contains_tag = true;
                break;
              }

            if (contains_tag) {
              if (NULL == track->tags)
                track->tags = new KaxTags;
              track->tags->PushElement(*tag);

              delete_tag = false;
            }
          }
        }
      }

      if (is_global) {
        if (NULL == m_tags)
          m_tags = new KaxTags;
        m_tags->PushElement(*tag);

      } else if (delete_tag)
        delete tag;

      tags->Remove(0);
    }

  } else
    delete l1;

  io->restore_pos();
}

void
kax_reader_c::read_headers_info(EbmlElement *&l1,
                                EbmlElement *&l2,
                                int &upper_lvl_el) {
  // General info about this Matroska file
  mxverb(2, "matroska_reader: |+ segment information...\n");

  l1->Read(*es, KaxInfo::ClassInfos.Context, upper_lvl_el, l2, true);

  KaxTimecodeScale *ktc_scale = FINDFIRST(l1, KaxTimecodeScale);
  if (NULL != ktc_scale) {
    tc_scale = uint64(*ktc_scale);
    mxverb(2, boost::format("matroska_reader: | + timecode scale: %1%\n") % tc_scale);

  } else
    tc_scale = 1000000;

  KaxDuration *kduration = FINDFIRST(l1, KaxDuration);
  if (NULL != kduration) {
    segment_duration = irnd(double(*kduration) * tc_scale);
    mxverb(2, boost::format("matroska_reader: | + duration: %|1$.3f|s\n") % (segment_duration / 1000000000.0));
  }

  KaxTitle *ktitle = FINDFIRST(l1, KaxTitle);
  if (NULL != ktitle) {
    title = UTFstring_to_cstrutf8(UTFstring(*ktitle));
    mxverb(2, boost::format("matroska_reader: | + title: %1%\n") % title);
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
  // spaces with one that doesn't. Then split the whole std::string up on
  // spaces into at most three parts. If the result is at least two parts
  // long then try to parse the version number from the second and
  // store a lower case version of the first as the application's name.
  KaxWritingApp *kwriting_app = FINDFIRST(l1, KaxWritingApp);
  if (NULL != kwriting_app)
    read_headers_info_writing_app(kwriting_app);

  KaxMuxingApp *kmuxing_app = FINDFIRST(l1, KaxMuxingApp);
  if (NULL != kmuxing_app) {
    muxing_app = UTFstring_to_cstrutf8(UTFstring(*kmuxing_app));
    mxverb(3, boost::format("matroska_reader: |   (muxing_app '%1%')\n") % muxing_app);

    // DirectShow Muxer workaround: Gabest's DirectShow muxer
    // writes wrong references (off by 1ms). So let the cluster
    // helper be a bit more imprecise in what it accepts when
    // looking for referenced packets.
    if (muxing_app == "DirectShow Matroska Muxer")
      reference_timecode_tolerance = 1000000;
  }
}

void
kax_reader_c::read_headers_info_writing_app(KaxWritingApp *&kwriting_app) {
  int idx;

  std::string s = UTFstring_to_cstrutf8(UTFstring(*kwriting_app));
  strip(s);
  mxverb(2, boost::format("matroska_reader: | + writing app: %1%\n") % s);

  if (starts_with_case(s, "avi-mux gui"))
    s.replace(0, strlen("avi-mux gui"), "avimuxgui");

  std::vector<std::string> parts = split(s.c_str(), " ", 3);
  if (parts.size() < 2) {
    writing_app = "";
    for (idx = 0; idx < s.size(); idx++)
      writing_app += tolower(s[idx]);
    writing_app_ver = -1;

  } else {

    writing_app = "";
    for (idx = 0; idx < parts[0].size(); idx++)
      writing_app += tolower(parts[0][idx]);
    s = "";

    for (idx = 0; idx < parts[1].size(); idx++)
      if (isdigit(parts[1][idx]) || (parts[1][idx] == '.'))
        s += parts[1][idx];

    std::vector<std::string> ver_parts = split(s.c_str(), ".");
    for (idx = ver_parts.size(); idx < 4; idx++)
      ver_parts.push_back("0");

    bool failed     = false;
    writing_app_ver = 0;

    for (idx = 0; idx < 4; idx++) {
      int num;

      if (!parse_int(ver_parts[idx], num) || (0 > num) || (255 < num)) {
        failed = true;
        break;
      }
      writing_app_ver <<= 8;
      writing_app_ver  |= num;
    }
    if (failed)
      writing_app_ver = -1;
  }

  mxverb(3, boost::format("matroska_reader: |   (writing_app '%1%', writing_app_ver 0x%|2$08x|)\n") % writing_app % (unsigned int)writing_app_ver);
}

void
kax_reader_c::read_headers_track_audio(kax_track_t *&track,
                                       KaxTrackAudio *&ktaudio) {
  mxverb(2, "matroska_reader: |  + Audio track\n");

  KaxAudioSamplingFreq *ka_sfreq = FINDFIRST(ktaudio, KaxAudioSamplingFreq);
  if (NULL != ka_sfreq) {
    track->a_sfreq = float(*ka_sfreq);
    mxverb(2, boost::format("matroska_reader: |   + Sampling frequency: %1%\n") % track->a_sfreq);
  } else
    track->a_sfreq = 8000.0;

  KaxAudioOutputSamplingFreq *ka_osfreq = FINDFIRST(ktaudio, KaxAudioOutputSamplingFreq);
  if (NULL != ka_osfreq) {
    track->a_osfreq = float(*ka_osfreq);
    mxverb(2, boost::format("matroska_reader: |   + Output sampling frequency: %1%\n") % track->a_osfreq);
  }

  KaxAudioChannels *ka_channels = FINDFIRST(ktaudio, KaxAudioChannels);
  if (NULL != ka_channels) {
    track->a_channels = uint8(*ka_channels);
    mxverb(2, boost::format("matroska_reader: |   + Channels: %1%\n") % track->a_channels);
  } else
    track->a_channels = 1;

  KaxAudioBitDepth *ka_bitdepth = FINDFIRST(ktaudio, KaxAudioBitDepth);
  if (NULL != ka_bitdepth) {
    track->a_bps = uint8(*ka_bitdepth);
    mxverb(2, boost::format("matroska_reader: |   + Bit depth: %1%\n") % track->a_bps);
  }
}

void
kax_reader_c::read_headers_track_video(kax_track_t *&track,
                                       KaxTrackVideo *&ktvideo) {
  KaxVideoPixelWidth *kv_pwidth = FINDFIRST(ktvideo, KaxVideoPixelWidth);
  if (NULL != kv_pwidth) {
    track->v_width = uint64(*kv_pwidth);
    mxverb(2, boost::format("matroska_reader: |   + Pixel width: %1%\n") % track->v_width);
  } else
    mxerror(Y("matroska_reader: Pixel width is missing.\n"));

  KaxVideoPixelHeight *kv_pheight = FINDFIRST(ktvideo, KaxVideoPixelHeight);
  if (NULL != kv_pheight) {
    track->v_height = uint64(*kv_pheight);
    mxverb(2, boost::format("matroska_reader: |   + Pixel height: %1%\n") % track->v_height);
  } else
    mxerror(Y("matroska_reader: Pixel height is missing.\n"));

  KaxVideoDisplayWidth *kv_dwidth = FINDFIRST(ktvideo, KaxVideoDisplayWidth);
  if (NULL != kv_dwidth) {
    track->v_dwidth = uint64(*kv_dwidth);
    mxverb(2, boost::format("matroska_reader: |   + Display width: %1%\n") % track->v_dwidth);
  } else
    track->v_dwidth = track->v_width;

  KaxVideoDisplayHeight *kv_dheight = FINDFIRST(ktvideo, KaxVideoDisplayHeight);
  if (NULL != kv_dheight) {
    track->v_dheight = uint64(*kv_dheight);
    mxverb(2, boost::format("matroska_reader: |   + Display height: %1%\n") % track->v_dheight);
  } else
    track->v_dheight = track->v_height;

  // For older files.
  KaxVideoFrameRate *kv_frate = FINDFIRST(ktvideo, KaxVideoFrameRate);
  if (NULL != kv_frate) {
    track->v_frate = float(*kv_frate);
    mxverb(2, boost::format("matroska_reader: |   + Frame rate: %1%\n") % track->v_frate);
  }

  KaxVideoPixelCropLeft *kv_pcleft = FINDFIRST(ktvideo, KaxVideoPixelCropLeft);
  if (NULL != kv_pcleft) {
    track->v_pcleft = uint64(*kv_pcleft);
    mxverb(2, boost::format("matroska_reader: |   + Pixel crop left: %1%\n") % track->v_pcleft);
  }

  KaxVideoPixelCropTop *kv_pctop = FINDFIRST(ktvideo, KaxVideoPixelCropTop);
  if (NULL != kv_pctop) {
    track->v_pctop = uint64(*kv_pctop);
    mxverb(2, boost::format("matroska_reader: |   + Pixel crop top: %1%\n") % track->v_pctop);
  }

  KaxVideoPixelCropRight *kv_pcright = FINDFIRST(ktvideo, KaxVideoPixelCropRight);
  if (NULL != kv_pcright) {
    track->v_pcright = uint64(*kv_pcright);
    mxverb(2, boost::format("matroska_reader: |   + Pixel crop right: %1%\n") % track->v_pcright);
  }

  KaxVideoPixelCropBottom *kv_pcbottom = FINDFIRST(ktvideo, KaxVideoPixelCropBottom);
  if (NULL != kv_pcbottom) {
    track->v_pcbottom = uint64(*kv_pcbottom);
    mxverb(2, boost::format("matroska_reader: |   + Pixel crop bottom: %1%\n") % track->v_pcbottom);
  }

  KaxVideoStereoMode *kv_stereo_mode = FINDFIRST(ktvideo, KaxVideoStereoMode);
  if (NULL != kv_stereo_mode) {
    track->v_stereo_mode = (stereo_mode_e)uint64(*kv_stereo_mode);
    mxverb(2, boost::format("matroska_reader: |   + Stereo mode: %1%\n") % (int)track->v_stereo_mode);
  }
}

void
kax_reader_c::read_headers_tracks(EbmlElement *&l1,
                                  EbmlElement *&l2,
                                  int &upper_lvl_el) {
  // Yep, we've found our KaxTracks element. Now find all tracks
  // contained in this segment.
  mxverb(2, "matroska_reader: |+ segment tracks...\n");

  l1->Read(*es, KaxTracks::ClassInfos.Context, upper_lvl_el, l2, true);

  KaxTrackEntry *ktentry = FINDFIRST(l1, KaxTrackEntry);
  while (ktentry != NULL) {
    // We actually found a track entry :) We're happy now.
    mxverb(2, "matroska_reader: | + a track...\n");

    kax_track_t *track = new_kax_track();
    if (NULL == track)
      return;

    KaxTrackNumber *ktnum = FINDFIRST(ktentry, KaxTrackNumber);
    if (NULL == ktnum)
      mxerror(Y("matroska_reader: A track is missing its track number.\n"));
    mxverb(2, boost::format("matroska_reader: |  + Track number: %1%\n") % (int)uint8(*ktnum));

    track->tnum = uint8(*ktnum);
    if (find_track_by_num(track->tnum, track) != NULL)
      mxwarn(boost::format(Y("matroska_reader: |  + There's more than one track with the number %1%.\n")) % track->tnum);

    KaxTrackUID *ktuid = FINDFIRST(ktentry, KaxTrackUID);
    if (NULL == ktuid)
      mxerror(Y("matroska_reader: A track is missing its track UID.\n"));
    mxverb(2, boost::format("matroska_reader: |  + Track UID: %1%\n") % uint64(*ktuid));
    track->tuid = uint64(*ktuid);
    if ((find_track_by_uid(track->tuid, track) != NULL) && (verbose > 1))
      mxwarn(boost::format(Y("matroska_reader: |  + There's more than one track with the UID %1%.\n")) % track->tnum);

    KaxTrackDefaultDuration *kdefdur = FINDFIRST(ktentry, KaxTrackDefaultDuration);
    if (NULL != kdefdur) {
      track->v_frate          = 1000000000.0 / (float)uint64(*kdefdur);
      track->default_duration = uint64(*kdefdur);
      mxverb(2, boost::format("matroska_reader: |  + Default duration: %|1$.3f|ms ( = %|2$.3f| fps)\n") % ((float)uint64(*kdefdur) / 1000000.0) % track->v_frate);
    }

    KaxTrackType *kttype = FINDFIRST(ktentry, KaxTrackType);
    if (NULL == kttype)
      mxerror(Y("matroska_reader: Track type was not found.\n"));
    unsigned char track_type = uint8(*kttype);
    track->type              = track_type == track_audio    ? 'a'
                             : track_type == track_video    ? 'v'
                             : track_type == track_subtitle ? 's'
                             :                                '?';
    mxverb(2, boost::format("matroska_reader: |  + Track type: %1%\n") % MAP_TRACK_TYPE_STRING(track->type));

    KaxTrackAudio *ktaudio = FINDFIRST(ktentry, KaxTrackAudio);
    if (NULL != ktaudio)
      read_headers_track_audio(track, ktaudio);

    KaxTrackVideo *ktvideo = FINDFIRST(ktentry, KaxTrackVideo);
    if (NULL != ktvideo)
      read_headers_track_video(track, ktvideo);

    KaxCodecID *kcodecid = FINDFIRST(ktentry, KaxCodecID);
    if (NULL != kcodecid) {
      mxverb(2, boost::format("matroska_reader: |  + Codec ID: %1%\n") % std::string(*kcodecid));
      track->codec_id = std::string(*kcodecid);
    } else
      mxerror(Y("matroska_reader: The CodecID is missing.\n"));

    KaxCodecPrivate *kcodecpriv = FINDFIRST(ktentry, KaxCodecPrivate);
    if (NULL != kcodecpriv) {
      mxverb(2, boost::format("matroska_reader: |  + CodecPrivate, length %1%\n") % kcodecpriv->GetSize());
      track->private_size = kcodecpriv->GetSize();
      if (0 < track->private_size)
        track->private_data = safememdup(kcodecpriv->GetBuffer(), track->private_size);
    }

    KaxTrackMinCache *ktmincache = FINDFIRST(ktentry, KaxTrackMinCache);
    if (NULL != ktmincache) {
      mxverb(2, boost::format("matroska_reader: |  + MinCache: %1%\n") % uint64(*ktmincache));
      if (1 < uint64(*ktmincache))
        track->v_bframes = true;
      track->min_cache = uint64(*ktmincache);
    }

    KaxTrackMaxCache *ktmaxcache = FINDFIRST(ktentry, KaxTrackMaxCache);
    if (NULL != ktmaxcache) {
      mxverb(2, boost::format("matroska_reader: |  + MaxCache: %1%\n") % uint64(*ktmaxcache));
      track->max_cache = uint64(*ktmaxcache);
    }

    KaxTrackFlagDefault *ktfdefault = FINDFIRST(ktentry, KaxTrackFlagDefault);
    if (NULL != ktfdefault) {
      mxverb(2, boost::format("matroska_reader: |  + Default flag: %1%\n") % uint64(*ktfdefault));
      track->default_track = uint64(*ktfdefault);
    }

    KaxTrackFlagForced *ktfforced = FINDFIRST(ktentry, KaxTrackFlagForced);
    if (NULL != ktfforced) {
      mxverb(2, boost::format("matroska_reader: |  + Forced flag: %1%\n") % uint64(*ktfforced));
      track->forced_track = uint64(*ktfforced);
    }

    KaxTrackFlagLacing *ktflacing = FINDFIRST(ktentry, KaxTrackFlagLacing);
    if (NULL != ktflacing) {
      mxverb(2, boost::format("matroska_reader: |  + Lacing flag: %1%\n") % uint64(*ktflacing));
      track->lacing_flag = uint64(*ktflacing);
    } else
      track->lacing_flag = true;

    KaxMaxBlockAdditionID *ktmax_blockadd_id = FINDFIRST(ktentry, KaxMaxBlockAdditionID);
    if (NULL != ktmax_blockadd_id) {
      mxverb(2, boost::format("matroska_reader: |  + Max Block Addition ID: %1%\n") % uint64(*ktmax_blockadd_id));
      track->max_blockadd_id = uint64(*ktmax_blockadd_id);
    } else
      track->max_blockadd_id = 0;

    KaxTrackLanguage *ktlanguage = FINDFIRST(ktentry, KaxTrackLanguage);
    if (NULL != ktlanguage) {
      mxverb(2, boost::format("matroska_reader: |  + Language: %1%\n") % std::string(*ktlanguage));
      track->language = std::string(*ktlanguage);
    }

    KaxTrackName *ktname = FINDFIRST(ktentry, KaxTrackName);
    if (NULL != ktname) {
      track->track_name = UTFstring_to_cstrutf8(UTFstring(*ktname));
      mxverb(2, boost::format("matroska_reader: |  + Name: %1%\n") % track->track_name);
    }

    track->content_decoder.initialize(*ktentry);
    ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
  } // while (ktentry != NULL)

  l1->SkipData(*es, l1->Generic().Context);
}

void
kax_reader_c::read_headers_seek_head(EbmlElement *&l0,
                                     EbmlElement *&l1,
                                     std::vector<int64_t> &deferred_tags,
                                     std::vector<int64_t> &deferred_chapters,
                                     std::vector<int64_t> &deferred_attachments) {
  EbmlElement *el;

  KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);

  int i = 0;
  seek_head.Read(*es, KaxSeekHead::ClassInfos.Context, i, el, true);

  for (i = 0; i < seek_head.ListSize(); i++) {
    if (EbmlId(*seek_head[i]) != KaxSeek::ClassInfos.GlobalId)
      continue;

    KaxSeek &seek       = *static_cast<KaxSeek *>(seek_head[i]);
    int64_t pos         = -1;
    bool is_attachments = false;
    bool is_chapters    = false;
    bool is_tags        = false;
    int k;

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

      } else if (EbmlId(*seek[k]) == KaxSeekPosition::ClassInfos.GlobalId)
        pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

    if (-1 != pos) {
      pos = ((KaxSegment *)l0)->GetGlobalPosition(pos);
      if (is_attachments)
        deferred_attachments.push_back(pos);
      else if (is_chapters)
        deferred_chapters.push_back(pos);
      else if (is_tags)
        deferred_tags.push_back(pos);
    }
  }
}

int
kax_reader_c::read_headers() {
  // Elements for different levels
  std::vector<int64_t> deferred_tags, deferred_chapters, deferred_attachments;

  bool exit_loop = false;
  try {
    in        = new mm_file_io_c(ti.fname);
    file_size = in->get_size();
    es        = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    EbmlElement *l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (NULL == l0) {
      mxwarn(Y("matroska_reader: no EBML head found.\n"));
      return 0;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;
    mxverb(2, "matroska_reader: Found the head...\n");

    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (NULL == l0) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return 0;
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return 0;
    }
    mxverb(2, "matroska_reader: + a segment...\n");

    segment = l0;

    // We've got our segment, so let's find the tracks
    int upper_lvl_el = 0;
    exit_loop        = false;
    tc_scale         = TIMECODE_SCALE;
    EbmlElement *l1  = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);

    while ((NULL != l1) && (0 >= upper_lvl_el)) {
      EbmlElement *l2;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId)
        read_headers_info(l1, l2, upper_lvl_el);

      else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId)
        read_headers_tracks(l1, l2, upper_lvl_el);

      else if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)
        deferred_attachments.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId)
        deferred_chapters.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId)
        deferred_tags.push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId)
        read_headers_seek_head(l0, l1, deferred_tags, deferred_chapters, deferred_attachments);

      else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        mxverb(2, "matroska_reader: |+ found cluster, headers are parsed completely\n");
        saved_l1  = l1;
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
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    int i;
    if (!ti.no_attachments)
      for (i = 0; i < deferred_attachments.size(); i++)
        handle_attachments(in, l0, deferred_attachments[i]);

    if (!ti.no_chapters)
      for (i = 0; i < deferred_chapters.size(); i++)
        handle_chapters(in, l0, deferred_chapters[i]);

    for (i = 0; i < deferred_tags.size(); i++)
      handle_tags(in, l0, deferred_tags[i]);

    if (!ti.no_global_tags)
      process_global_tags();

  } catch (...) {
    mxerror(Y("matroska_reader: caught exception\n"));
  }

  if (!exit_loop)               // We have NOT found a cluster!
    return 0;

  verify_tracks();

  return 1;
}

void
kax_reader_c::process_global_tags() {
  if ((NULL == m_tags) || g_identifying)
    return;

  int i;
  for (i = 0; m_tags->ListSize() > i; ++i)
    add_tags(static_cast<KaxTag *>((*m_tags)[i]));

  m_tags->RemoveAll();
}

void
kax_reader_c::init_passthrough_packetizer(kax_track_t *t) {
  passthrough_packetizer_c *ptzr;
  track_info_c nti(ti);

  mxinfo_tid(ti.fname, t->tnum, boost::format(Y("Using the passthrough output module for this %1% track.\n")) % MAP_TRACK_TYPE_STRING(t->type));

  nti.id                  = t->tnum;
  nti.language            = t->language;
  nti.track_name          = t->track_name;

  ptzr                    = new passthrough_packetizer_c(this, nti);
  t->ptzr                 = add_packetizer(ptzr);
  t->passthrough          = true;
  ptzr_to_track_map[ptzr] = t;

  ptzr->set_track_type(MAP_TRACK_TYPE(t->type));
  ptzr->set_codec_id(t->codec_id);
  ptzr->set_codec_private((unsigned char *)t->private_data, t->private_size);

  if (0.0 < t->v_frate)
    ptzr->set_track_default_duration((int64_t)(1000000000.0 / t->v_frate));
  if (0 < t->min_cache)
    ptzr->set_track_min_cache(t->min_cache);
  if (0 < t->max_cache)
    ptzr->set_track_max_cache(t->max_cache);

  if ('v' == t->type) {
    ptzr->set_video_pixel_width(t->v_width);
    ptzr->set_video_pixel_height(t->v_height);
    if (!ptzr->ti.aspect_ratio_given) { // The user hasn't set it.
      if (0 != t->v_dwidth)
        ptzr->set_video_display_width(t->v_dwidth);
      if (0 != t->v_dheight)
        ptzr->set_video_display_height(t->v_dheight);
    }
    if (!ptzr->ti.pixel_cropping_specified && ((t->v_pcleft > 0)  || (t->v_pctop > 0) || (t->v_pcright > 0) || (t->v_pcbottom > 0)))
      ptzr->set_video_pixel_cropping(t->v_pcleft, t->v_pctop, t->v_pcright, t->v_pcbottom);
    if (STEREO_MODE_UNSPECIFIED == ptzr->ti.stereo_mode)
      ptzr->set_stereo_mode(t->v_stereo_mode);
    if (CUE_STRATEGY_UNSPECIFIED == ptzr->get_cue_creation())
      ptzr->set_cue_creation(CUE_STRATEGY_IFRAMES);

  } else if ('a' == t->type) {
    ptzr->set_audio_sampling_freq(t->a_sfreq);
    ptzr->set_audio_channels(t->a_channels);
    if (0 != t->a_bps)
      ptzr->set_audio_bit_depth(t->a_bps);
    if (0.0 != t->a_osfreq)
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
    PTZR(t->ptzr)->set_as_default_track(t->type == 'v' ? DEFTRACK_TYPE_VIDEO : t->type == 'a' ? DEFTRACK_TYPE_AUDIO : DEFTRACK_TYPE_SUBS,
                                        DEFAULT_TRACK_PRIORITY_FROM_SOURCE);

  if (t->forced_track && boost::logic::indeterminate(PTZR(t->ptzr)->ti.forced_track))
    PTZR(t->ptzr)->set_track_forced_flag(true);

  if ((0 != t->tuid) && !PTZR(t->ptzr)->set_uid(t->tuid))
    mxwarn(boost::format(Y("matroska_reader: Could not keep the track UID %1% because it is already allocated for the new file.\n")) % t->tuid);
}

void
kax_reader_c::create_video_packetizer(kax_track_t *t,
                                      track_info_c &nti) {
  if ((t->codec_id == MKV_V_MSCOMP) && mpeg4::p10::is_avc_fourcc(t->v_fourcc) && !hack_engaged(ENGAGE_ALLOW_AVC_IN_VFW_MODE))
    create_mpeg4_p10_es_video_packetizer(t, nti);

  else if (   starts_with(t->codec_id, "V_MPEG4", 7)
           || (t->codec_id == MKV_V_MSCOMP)
           || starts_with(t->codec_id, "V_REAL", 6)
           || (t->codec_id == MKV_V_QUICKTIME)
           || (t->codec_id == MKV_V_MPEG1)
           || (t->codec_id == MKV_V_MPEG2)
           || (t->codec_id == MKV_V_THEORA)
           || (t->codec_id == MKV_V_DIRAC)) {
    if ((t->codec_id == MKV_V_MPEG1) || (t->codec_id == MKV_V_MPEG2)) {
      int version = t->codec_id[6] - '0';
      mxinfo_tid(ti.fname, t->tnum, boost::format(Y("Using the MPEG-%1% video output module.\n")) % version);
      t->ptzr = add_packetizer(new mpeg1_2_video_packetizer_c(this, nti, version, t->v_frate, t->v_width, t->v_height, t->v_dwidth, t->v_dheight, true));

    } else if (IS_MPEG4_L2_CODECID(t->codec_id) || IS_MPEG4_L2_FOURCC(t->v_fourcc)) {
      mxinfo_tid(ti.fname, t->tnum, Y("Using the MPEG-4 part 2 video output module.\n"));
      bool is_native = IS_MPEG4_L2_CODECID(t->codec_id);
      t->ptzr = add_packetizer(new mpeg4_p2_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height, is_native));

    } else if (t->codec_id == MKV_V_MPEG4_AVC) {
      mxinfo_tid(ti.fname, t->tnum, Y("Using the MPEG-4 part 10 (AVC) video output module.\n"));
      t->ptzr = add_packetizer(new mpeg4_p10_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height));

    } else if (t->codec_id == MKV_V_THEORA) {
      mxinfo_tid(ti.fname, t->tnum, Y("Using the Theora video output module.\n"));
      t->ptzr = add_packetizer(new theora_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height));

    } else if (t->codec_id == MKV_V_DIRAC) {
      mxinfo_tid(ti.fname, t->tnum, Y("Using the Dirac video output module.\n"));
      t->ptzr = add_packetizer(new dirac_video_packetizer_c(this, nti));

    } else {
      mxinfo_tid(ti.fname, t->tnum, Y("Using the video output module.\n"));
      t->ptzr = add_packetizer(new video_packetizer_c(this, nti, t->codec_id.c_str(), t->v_frate, t->v_width, t->v_height));
    }

    if (!PTZR(t->ptzr)->ti.aspect_ratio_given) {
      // The user hasn't set it.
      if (0 != t->v_dwidth)
        PTZR(t->ptzr)->set_video_display_width(t->v_dwidth);
      if (0 != t->v_dheight)
        PTZR(t->ptzr)->set_video_display_height(t->v_dheight);
    }

    if (!PTZR(t->ptzr)->ti.pixel_cropping_specified && ((t->v_pcleft > 0) || (t->v_pctop > 0) || (t->v_pcright > 0) || (t->v_pcbottom > 0)))
      PTZR(t->ptzr)->set_video_pixel_cropping(t->v_pcleft, t->v_pctop, t->v_pcright, t->v_pcbottom);
    if (STEREO_MODE_UNSPECIFIED == PTZR(t->ptzr)->ti.stereo_mode)
      PTZR(t->ptzr)->set_stereo_mode(t->v_stereo_mode);

  } else
    init_passthrough_packetizer(t);
}

void
kax_reader_c::create_audio_packetizer(kax_track_t *t,
                                      track_info_c &nti) {
  if ((0x0001 == t->a_formattag) || (0x0003 == t->a_formattag)) {
    t->ptzr = add_packetizer(new pcm_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, t->a_bps, false, t->a_formattag==0x0003));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the PCM output module.\n"));

  } else if (0x0055 == t->a_formattag) {
    t->ptzr = add_packetizer(new mp3_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, true));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the MPEG audio output module.\n"));

  } else if (0x2000 == t->a_formattag) {
    int bsid = t->codec_id == "A_AC3/BSID9"  ?  9
             : t->codec_id == "A_AC3/BSID10" ? 10
             : t->codec_id == MKV_A_EAC3     ? 16
             :                                  0;

    t->ptzr = add_packetizer(new ac3_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, bsid));
    mxinfo_tid(ti.fname, t->tnum, boost::format(Y("Using the %1%AC3 output module.\n")) % (16 == bsid ? "E" : ""));

  } else if (0x2001 == t->a_formattag) {
    dts_header_t dtsheader;

    dtsheader.core_sampling_frequency = (unsigned int)t->a_sfreq;
    dtsheader.audio_channels          = t->a_channels;

    t->ptzr = add_packetizer(new dts_packetizer_c(this, nti, dtsheader, true));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the DTS output module.\n"));

  } else if (0xFFFE == t->a_formattag) {
    t->ptzr = add_packetizer(new vorbis_packetizer_c(this, nti, t->headers[0], t->header_sizes[0], t->headers[1], t->header_sizes[1], t->headers[2], t->header_sizes[2]));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the Vorbis output module.\n"));

  } else if ((FOURCC('M', 'P', '4', 'A') == t->a_formattag) || (0x00ff == t->a_formattag)) {
    // A_AAC/MPEG2/MAIN
    // 0123456789012345
    int id               = 0;
    int profile          = 0;
    int detected_profile = AAC_PROFILE_MAIN;

    if (FOURCC('M', 'P', '4', 'A') == t->a_formattag) {
      if ((NULL != t->private_data) && (2 <= t->private_size)) {
        int channels, sfreq, osfreq;
        bool sbr;

        if (!parse_aac_data((unsigned char *)t->private_data, t->private_size, profile, channels, sfreq, osfreq, sbr))
          mxerror_tid(ti.fname, t->tnum, Y("Malformed AAC codec initialization data found.\n"));

        detected_profile = profile;
        id               = AAC_ID_MPEG4;
        if (sbr)
          profile        = AAC_PROFILE_SBR;

      } else if (!parse_aac_codec_id(std::string(t->codec_id), id, profile))
        mxerror_tid(ti.fname, t->tnum, boost::format(Y("Malformed codec id '%1%'.\n")) % t->codec_id);

    } else {
      int channels, sfreq, osfreq;
      bool sbr;

      if (!parse_aac_data(((unsigned char *)t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX), profile, channels, sfreq, osfreq, sbr))
        mxerror_tid(ti.fname, t->tnum, Y("Malformed AAC codec initialization data found.\n"));

      detected_profile = profile;
      id               = AAC_ID_MPEG4;
      if (sbr)
        profile        = AAC_PROFILE_SBR;
    }

    if ((map_has_key(ti.all_aac_is_sbr, t->tnum) &&  ti.all_aac_is_sbr[t->tnum]) || (map_has_key(ti.all_aac_is_sbr, -1) &&  ti.all_aac_is_sbr[-1]))
      profile = AAC_PROFILE_SBR;

    if ((map_has_key(ti.all_aac_is_sbr, t->tnum) && !ti.all_aac_is_sbr[t->tnum]) || (map_has_key(ti.all_aac_is_sbr, -1) && !ti.all_aac_is_sbr[-1]))
      profile = detected_profile;

    t->ptzr = add_packetizer(new aac_packetizer_c(this, nti, id, profile, (int32_t)t->a_sfreq, t->a_channels, false, true));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the AAC output module.\n"));

 #if defined(HAVE_FLAC_FORMAT_H)
  } else if ((FOURCC('f', 'L', 'a', 'C') == t->a_formattag) || (0xf1ac == t->a_formattag)) {
    safefree(nti.private_data);
    nti.private_data = NULL;
    nti.private_size = 0;

    if (FOURCC('f', 'L', 'a', 'C') == t->a_formattag)
      t->ptzr = add_packetizer(new flac_packetizer_c(this, nti, (unsigned char *) t->private_data, t->private_size));
    else {
      flac_packetizer_c * p= new flac_packetizer_c(this, nti, ((unsigned char *)t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX));
      t->ptzr              = add_packetizer(p);
    }

    mxinfo_tid(ti.fname, t->tnum, Y("Using the FLAC output module.\n"));
 #endif

  } else if (FOURCC('T', 'T', 'A', '1') == t->a_formattag) {
    safefree(nti.private_data);
    nti.private_data = NULL;
    nti.private_size = 0;

    t->ptzr          = add_packetizer(new tta_packetizer_c(this, nti, t->a_channels, t->a_bps, (int32_t)t->a_sfreq));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the TTA output module.\n"));

  } else if (FOURCC('W', 'V', 'P', '4') == t->a_formattag) {
    wavpack_meta_t meta;

    nti.private_data     = (unsigned char *)t->private_data;
    nti.private_size     = t->private_size;

    meta.bits_per_sample = t->a_bps;
    meta.channel_count   = t->a_channels;
    meta.sample_rate     = (uint32_t)t->a_sfreq;
    meta.has_correction  = t->max_blockadd_id != 0;

    if (0.0 < t->v_frate)
      meta.samples_per_block = (uint32_t)(t->a_sfreq / t->v_frate);

    t->ptzr          = add_packetizer(new wavpack_packetizer_c(this, nti, meta));
    nti.private_data = NULL;
    nti.private_size = 0;

    mxinfo_tid(ti.fname, t->tnum, Y("Using the WAVPACK output module.\n"));

  } else
    init_passthrough_packetizer(t);

  if (0.0 != t->a_osfreq)
    PTZR(t->ptzr)->set_audio_output_sampling_freq(t->a_osfreq);
}

void
kax_reader_c::create_subtitle_packetizer(kax_track_t *t,
                                         track_info_c &nti) {
  if (t->codec_id == MKV_S_VOBSUB) {
    t->ptzr = add_packetizer(new vobsub_packetizer_c(this, t->private_data, t->private_size, nti));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the VobSub output module.\n"));

    t->sub_type = 'v';

  } else if (starts_with(t->codec_id, "S_TEXT", 6) || (t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) {
    std::string new_codec_id = ((t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) ? std::string("S_TEXT/") + std::string(&t->codec_id[2]) : t->codec_id;

    t->ptzr = add_packetizer(new textsubs_packetizer_c(this, nti, new_codec_id.c_str(), t->private_data, t->private_size, false, true));
    mxinfo_tid(ti.fname, t->tnum, Y("Using the text subtitle output module.\n"));

    t->sub_type = 't';

  } else if (t->codec_id == MKV_S_KATE) {
    t->ptzr     = add_packetizer(new kate_packetizer_c(this, nti, t->private_data, t->private_size));
    t->sub_type = 'k';

    mxinfo_tid(ti.fname, t->tnum, Y("Using the Kate output module.\n"));

  } else
    init_passthrough_packetizer(t);

}

void
kax_reader_c::create_button_packetizer(kax_track_t *t,
                                       track_info_c &nti) {
  if (t->codec_id == MKV_B_VOBBTN) {
    mxinfo_tid(ti.fname, t->tnum, Y("Using the VobBtn output module.\n"));

    safefree(nti.private_data);
    nti.private_data = NULL;
    nti.private_size = 0;

    t->ptzr          = add_packetizer(new vobbtn_packetizer_c(this, nti, t->v_width, t->v_height));
    t->sub_type      = 'b';

  } else
    init_passthrough_packetizer(t);
}

void
kax_reader_c::create_packetizer(int64_t tid) {
  uint32_t i;
  kax_track_t *t = NULL;
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->tnum == tid) {
      t = tracks[i];
      break;
    }

  if ((NULL == t) || (-1 != t->ptzr) || !t->ok || !demuxing_requested(t->type, t->tnum))
    return;

  track_info_c nti(ti);
  nti.private_data = (unsigned char *)safememdup(t->private_data, t->private_size);
  nti.private_size = t->private_size;
  nti.id           = t->tnum; // ID for this track.

  if (nti.language == "")
    nti.language   = t->language;
  if (nti.track_name == "")
    nti.track_name = t->track_name;
  if ((NULL != t->tags) && demuxing_requested('T', t->tnum))
    nti.tags       = dynamic_cast<KaxTags *>(t->tags->Clone());

  if (hack_engaged(ENGAGE_FORCE_PASSTHROUGH_PACKETIZER)) {
    init_passthrough_packetizer(t);
    set_packetizer_headers(t);

    return;
  }

  switch (t->type) {
    case 'v':
      create_video_packetizer(t, nti);
      break;

    case 'a':
      create_audio_packetizer(t, nti);
      break;

    case 's':
      create_subtitle_packetizer(t, nti);
      break;

    case 'b':
      create_button_packetizer(t, nti);
      break;

    default:
      mxerror_tid(ti.fname, t->tnum, Y("Unsupported track type for this track.\n"));
      break;
  }

  set_packetizer_headers(t);
  ptzr_to_track_map[reader_packetizers[t->ptzr]] = t;
}

void
kax_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    create_packetizer(tracks[i]->tnum);

  if (!g_segment_title_set) {
    g_segment_title     = title;
    g_segment_title_set = true;
  }
}

void
kax_reader_c::create_mpeg4_p10_es_video_packetizer(kax_track_t *t,
                                                   track_info_c &nti) {
  try {
    read_first_frame(t);
    if (!t->first_frame_data.is_set())
      throw false;

    avc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    if (map_has_key(ti.nalu_size_lengths, t->tnum))
      parser.set_nalu_size_length(ti.nalu_size_lengths[t->tnum]);
    else if (map_has_key(ti.nalu_size_lengths, -1))
      parser.set_nalu_size_length(ti.nalu_size_lengths[-1]);

    if (sizeof(alBITMAPINFOHEADER) < t->private_size)
      parser.add_bytes((unsigned char *)t->private_data + sizeof(alBITMAPINFOHEADER), t->private_size - sizeof(alBITMAPINFOHEADER));
    parser.add_bytes(t->first_frame_data->get_buffer(), t->first_frame_data->get_size());
    parser.flush();

    if (!parser.headers_parsed())
      throw false;

    if (verbose)
      mxinfo_tid(ti.fname, t->tnum, Y("Using the MPEG-4 part 10 ES video output module.\n"));

    memory_cptr avcc                      = parser.get_avcc();
    mpeg4_p10_es_video_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, nti, avcc, t->v_width, t->v_height);
    t->ptzr                               = add_packetizer(ptzr);

    ptzr->enable_timecode_generation(false);
    if (t->default_duration)
      ptzr->set_track_default_duration(t->default_duration);

  } catch (...) {
    mxerror_tid(ti.fname, t->tnum, Y("Could not extract the decoder specific config data (AVCC) from this AVC/h.264 track.\n"));
  }
}

void
kax_reader_c::read_first_frame(kax_track_t *t) {
  if (t->first_frame_data.is_set() || (NULL == saved_l1))
    return;

  in->save_pos(saved_l1->GetElementPosition());

  try {
    int upper_lvl_el = 0;
    EbmlElement *l1  = es->FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
    EbmlElement *l2  = NULL;

    while ((l1 != NULL) && (upper_lvl_el <= 0)) {
      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        cluster = (KaxCluster *)l1;
        cluster->Read(*es, KaxCluster::ClassInfos.Context, upper_lvl_el, l2, true);

        KaxClusterTimecode *ctc = static_cast<KaxClusterTimecode *> (cluster->FindFirstElt(KaxClusterTimecode::ClassInfos, false));
        if (NULL != ctc) {
          cluster_tc = uint64(*ctc);
          cluster->InitTimecode(cluster_tc, tc_scale);
        }

        int bgidx;
        for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
          if ((EbmlId(*(*cluster)[bgidx]) == KaxSimpleBlock::ClassInfos.GlobalId)) {
            KaxSimpleBlock *block_simple = static_cast<KaxSimpleBlock *>((*cluster)[bgidx]);

            block_simple->SetParent(*cluster);
            kax_track_t *block_track = find_track_by_num(block_simple->TrackNum());

            if ((NULL == block_track) || (0 == block_simple->NumberFrames()))
              continue;

            if (!block_track->first_frame_data.is_set()) {
              DataBuffer &data_buffer = block_simple->GetBuffer(0);
              block_track->first_frame_data = memory_cptr(new memory_c(data_buffer.Buffer(), data_buffer.Size()));
              block_track->content_decoder.reverse(block_track->first_frame_data, CONTENT_ENCODING_SCOPE_BLOCK);
              block_track->first_frame_data->grab();

              if (t == block_track) {
                in->restore_pos();
                delete l1;
                return;
              }
            }

          } else if ((EbmlId(*(*cluster)[bgidx]) == KaxBlockGroup::ClassInfos.GlobalId)) {
            KaxBlockGroup *block_group = static_cast<KaxBlockGroup *>((*cluster)[bgidx]);
            KaxBlock *block            = static_cast<KaxBlock *>(block_group->FindFirstElt(KaxBlock::ClassInfos, false));

            if (NULL == block)
              continue;

            block->SetParent(*cluster);
            kax_track_t *block_track = find_track_by_num(block->TrackNum());

            if ((NULL == block_track) || (0 == block->NumberFrames()))
              continue;

            if (!block_track->first_frame_data.is_set()) {
              DataBuffer &data_buffer       = block->GetBuffer(0);
              block_track->first_frame_data = memory_cptr(new memory_c(data_buffer.Buffer(), data_buffer.Size()));
              block_track->content_decoder.reverse(block_track->first_frame_data, CONTENT_ENCODING_SCOPE_BLOCK);
              block_track->first_frame_data->grab();

              if (t == block_track) {
                in->restore_pos();
                delete l1;
                return;
              }
            }
          }
        }
      }

      l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(segment)) {
        delete l1;
        break;
      }

      if (0 < upper_lvl_el) {
        upper_lvl_el--;
        if (0 < upper_lvl_el)
          break;
        delete l1;
        l1 = l2;
        break;

      } else if (0 > upper_lvl_el)
        upper_lvl_el++;

      delete l1;
      l1 = es->FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
    } // while (l1 != NULL)

  } catch (...) {
  }

  in->restore_pos();
}

file_status_e
kax_reader_c::read(generic_packetizer_c *requested_ptzr,
                   bool force) {
  if (tracks.size() == 0)
    return FILE_STATUS_DONE;

  if (NULL == saved_l1) {       // We're done.
    flush_packetizers();
    return FILE_STATUS_DONE;
  }

  int64_t num_queued_bytes = get_queued_bytes();
  if (!force && (20 * 1024 * 1024 < num_queued_bytes)) {
    kax_track_t *requested_ptzr_track = ptzr_to_track_map[requested_ptzr];
    if (!requested_ptzr_track || (('a' != requested_ptzr_track->type) && ('v' != requested_ptzr_track->type)) || (512 * 1024 * 1024 < num_queued_bytes))
      return FILE_STATUS_HOLDING;
  }

  bool found_cluster = false;
  int upper_lvl_el   = 0;
  EbmlElement *l0    = segment;
  EbmlElement *l1    = saved_l1;
  EbmlElement *l2    = NULL;
  saved_l1           = NULL;

  try {
    while ((NULL != l1) && (0 >= upper_lvl_el)) {
      if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        found_cluster = true;
        cluster       = (KaxCluster *)l1;
        cluster->Read(*es, KaxCluster::ClassInfos.Context, upper_lvl_el, l2, true);

        KaxClusterTimecode *ctc = static_cast<KaxClusterTimecode *>(cluster->FindFirstElt(KaxClusterTimecode::ClassInfos, false));
        if (NULL == ctc)
          mxerror(Y("r_matroska: Cluster does not contain a cluster timecode. File is broken. Aborting.\n"));

        cluster_tc = uint64(*ctc);
        cluster->InitTimecode(cluster_tc, tc_scale);
        if (-1 == first_timecode) {
          first_timecode = cluster_tc * tc_scale;

          // If we're appending this file to another one then the core
          // needs the timecodes shifted to zero.
          if (appending && (NULL != chapters) && (0 < first_timecode))
            adjust_chapter_timecodes(*chapters, -first_timecode);
        }

        int bgidx;
        for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
          int64_t block_duration = -1;
          int64_t block_bref     = VFT_IFRAME;
          int64_t block_fref     = VFT_NOBFRAME;
          bool bref_found        = false;
          bool fref_found        = false;

          if ((EbmlId(*(*cluster)[bgidx]) == KaxSimpleBlock::ClassInfos.GlobalId)) {
            KaxSimpleBlock *block_simple = static_cast<KaxSimpleBlock *>((*cluster)[bgidx]);

            block_simple->SetParent(*cluster);
            kax_track_t *block_track = find_track_by_num(block_simple->TrackNum());

            if (NULL == block_track) {
              mxwarn_fn(ti.fname,
                        boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                                        "The block will be skipped.\n")) % format_timecode(block_simple->GlobalTimecode()) % block_simple->TrackNum());
              continue;
            }

            if (0 != block_track->v_frate)
              block_duration = (int64_t)(1000000000.0 / block_track->v_frate);
            int64_t frame_duration = (block_duration == -1) ? 0 : block_duration;

            if (('s' == block_track->type) && (-1 == block_duration))
              block_duration = 0;

            if (block_track->ignore_duration_hack) {
              frame_duration = 0;
              if (0 < block_duration)
                block_duration = 0;
            }

            if (!block_simple->IsKeyframe()) {
              if (block_simple->IsDiscardable()) {
                block_fref = block_track->previous_timecode;
                fref_found = true;
              } else {
                block_bref = block_track->previous_timecode;
                bref_found = true;
              }
            }

            last_timecode = block_simple->GlobalTimecode();

            // If we're appending this file to another one then the core
            // needs the timecodes shifted to zero.
            if (appending)
              last_timecode -= first_timecode;

            if ((-1 != block_track->ptzr) && block_track->passthrough) {
              // The handling for passthrough is a bit different. We don't have
              // any special cases, e.g. 0 terminating a string for the subs
              // and stuff. Just pass everything through as it is.
              int i;
              for (i = 0; i < (int)block_simple->NumberFrames(); i++) {
                DataBuffer &data = block_simple->GetBuffer(i);
                packet_t *packet = new packet_t(new memory_c((unsigned char *)data.Buffer(), data.Size(), false),
                                                last_timecode + i * frame_duration, block_duration, block_bref, block_fref);

                ((passthrough_packetizer_c *)PTZR(block_track->ptzr))->process(packet_cptr(packet));
              }

            } else if (-1 != block_track->ptzr) {
              int i;
              for (i = 0; i < (int)block_simple->NumberFrames(); i++) {
                DataBuffer &data_buffer = block_simple->GetBuffer(i);
                memory_cptr data(new memory_c(data_buffer.Buffer(), data_buffer.Size(), false));
                block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);

                if (('s' == block_track->type) && ('t' == block_track->sub_type)) {
                  if ((2 < data->get_size()) || ((0 < data->get_size()) && (' ' != *data->get_buffer()) && (0 != *data->get_buffer()) && !iscr(*data->get_buffer()))) {
                    char *lines             = (char *)safemalloc(data->get_size() + 1);
                    lines[data->get_size()] = 0;
                    memcpy(lines, data->get_buffer(), data->get_size());

                    PTZR(block_track->ptzr)-> process(new packet_t(new memory_c((unsigned char *)lines, 0, true), last_timecode, block_duration, block_bref, block_fref));
                  }

                } else {
                  packet_cptr packet(new packet_t(data, last_timecode + i * frame_duration, block_duration, block_bref, block_fref));
                  PTZR(block_track->ptzr)->process(packet);
                }
              }
            }

            block_track->previous_timecode  = (int64_t)last_timecode;
            block_track->units_processed   += block_simple->NumberFrames();

          } else if ((EbmlId(*(*cluster)[bgidx]) == KaxBlockGroup::ClassInfos.GlobalId)) {
            KaxBlockGroup *block_group   = static_cast<KaxBlockGroup *>((*cluster)[bgidx]);
            KaxReferenceBlock *ref_block = static_cast<KaxReferenceBlock *>(block_group->FindFirstElt(KaxReferenceBlock::ClassInfos, false));

            while (NULL != ref_block) {
              if (0 >= int64(*ref_block)) {
                block_bref = int64(*ref_block) * tc_scale;
                bref_found = true;
              } else {
                block_fref = int64(*ref_block) * tc_scale;
                fref_found = true;
              }

              ref_block = static_cast<KaxReferenceBlock *>(block_group->FindNextElt(*ref_block, false));
            }

            KaxBlock *block = static_cast<KaxBlock *>(block_group->FindFirstElt(KaxBlock::ClassInfos, false));
            if (NULL == block) {
              mxwarn_fn(ti.fname,
                        boost::format(Y("A block group was found at position %1%, but no block element was found inside it. This might make mkvmerge crash.\n"))
                        % block_group->GetElementPosition());
              continue;
            }

            block->SetParent(*cluster);
            kax_track_t *block_track = find_track_by_num(block->TrackNum());

            if (NULL == block_track) {
              mxwarn_fn(ti.fname,
                        boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                                        "The block will be skipped.\n")) % format_timecode(block->GlobalTimecode()) % block->TrackNum());
              continue;
            }

            KaxBlockAdditions *blockadd = static_cast<KaxBlockAdditions *>(block_group->FindFirstElt(KaxBlockAdditions::ClassInfos, false));
            KaxBlockDuration *duration  = static_cast<KaxBlockDuration *>(block_group->FindFirstElt(KaxBlockDuration::ClassInfos, false));

            if (NULL != duration)
              block_duration = (int64_t)uint64(*duration) * tc_scale / block->NumberFrames();
            else if (0 != block_track->v_frate)
              block_duration = (int64_t)(1000000000.0 / block_track->v_frate);
            int64_t frame_duration = (block_duration == -1) ? 0 : block_duration;

            if (('s' == block_track->type) && (-1 == block_duration))
              block_duration = 0;

            if (block_track->ignore_duration_hack) {
              frame_duration = 0;
              if (0 < block_duration)
                block_duration = 0;
            }

            last_timecode = block->GlobalTimecode();
            // If we're appending this file to another one then the core
            // needs the timecodes shifted to zero.
            if (appending)
              last_timecode -= first_timecode;

            KaxCodecState *codec_state = static_cast<KaxCodecState *>(block_group->FindFirstElt(KaxCodecState::ClassInfos));
            if ((NULL != codec_state) && !hack_engaged(ENGAGE_USE_CODEC_STATE))
              mxerror_tid(ti.fname, block_track->tnum,
                          Y("This track uses a Matroska feature called 'Codec state elements'. mkvmerge supports these but "
                            "this feature has not been turned on with the option '--engage use_codec_state'.\n"));

            if ((-1 != block_track->ptzr) && block_track->passthrough) {
              // The handling for passthrough is a bit different. We don't have
              // any special cases, e.g. 0 terminating a string for the subs
              // and stuff. Just pass everything through as it is.
              if (bref_found)
                block_bref += last_timecode;
              if (fref_found)
                block_fref += last_timecode;

              int i;
              for (i = 0; i < (int)block->NumberFrames(); i++) {
                DataBuffer &data           = block->GetBuffer(i);
                packet_t *packet           = new packet_t(new memory_c((unsigned char *)data.Buffer(), data.Size(), false),
                                                          last_timecode + i * frame_duration, block_duration, block_bref, block_fref);
                packet->duration_mandatory = duration != NULL;

                if (NULL != codec_state)
                  packet->codec_state = clone_memory(codec_state->GetBuffer(), codec_state->GetSize());

                ((passthrough_packetizer_c *)PTZR(block_track->ptzr))->process(packet_cptr(packet));
              }

            } else if (-1 != block_track->ptzr) {
              if (bref_found) {
                if (FOURCC('r', 'e', 'a', 'l') == block_track->a_formattag)
                  block_bref = block_track->previous_timecode;
                else
                  block_bref += (int64_t)last_timecode;
              }
              if (fref_found)
                block_fref += (int64_t)last_timecode;

              int i;
              for (i = 0; i < (int)block->NumberFrames(); i++) {
                DataBuffer &data_buffer = block->GetBuffer(i);
                memory_cptr data(new memory_c(data_buffer.Buffer(), data_buffer.Size(), false));
                block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);

                if (('s' == block_track->type) && ('t' == block_track->sub_type)) {
                  if ((2 < data->get_size()) || ((0 < data->get_size()) && (' ' != *data->get_buffer()) && (0 != *data->get_buffer()) && !iscr(*data->get_buffer()))) {
                    char *lines             = (char *)safemalloc(data->get_size() + 1);
                    lines[data->get_size()] = 0;
                    memcpy(lines, data->get_buffer(), data->get_size());

                    packet_t *packet = new packet_t(new memory_c((unsigned char *)lines, 0, true), last_timecode, block_duration, block_bref, block_fref);
                    if (NULL != codec_state)
                      packet->codec_state = clone_memory(codec_state->GetBuffer(), codec_state->GetSize());

                    PTZR(block_track->ptzr)->process(packet);
                  }

                } else {
                  packet_cptr packet(new packet_t(data, last_timecode + i * frame_duration, block_duration, block_bref, block_fref));

                  if (NULL != codec_state)
                    packet->codec_state = clone_memory(codec_state->GetBuffer(), codec_state->GetSize());

                  if (blockadd) {
                    int k;
                    for (k = 0; k < blockadd->ListSize(); k++) {
                      if (!(is_id((*blockadd)[k], KaxBlockMore)))
                        continue;

                      KaxBlockMore *blockmore           = static_cast<KaxBlockMore *>((*blockadd)[k]);
                      KaxBlockAdditional *blockadd_data = &GetChild<KaxBlockAdditional>(*blockmore);

                      memory_cptr blockadded(new memory_c(blockadd_data->GetBuffer(), blockadd_data->GetSize(), false));
                      block_track->content_decoder.reverse(blockadded, CONTENT_ENCODING_SCOPE_BLOCK);

                      packet->data_adds.push_back(blockadded);
                    }
                  }

                  PTZR(block_track->ptzr)->process(packet);
                }
              }

              block_track->previous_timecode  = (int64_t)last_timecode;
              block_track->units_processed   += block->NumberFrames();
            }
          }
        }
      }

      l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (0 < upper_lvl_el) {
        upper_lvl_el--;
        if (0 < upper_lvl_el)
          break;
        delete l1;
        saved_l1 = l2;
        break;

      } else if (0 > upper_lvl_el)
        upper_lvl_el++;

      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
      if (found_cluster) {
        saved_l1 = l1;
        break;
      }

    } // while (l1 != NULL)

  } catch (...) {
    mxwarn(Y("matroska_reader: caught exception\n"));
    flush_packetizers();
    return FILE_STATUS_DONE;
  }

  if (found_cluster)
    return FILE_STATUS_MOREDATA;

  flush_packetizers();
  return FILE_STATUS_DONE;
}

int
kax_reader_c::get_progress() {
  if (0 != segment_duration)
    return (last_timecode - first_timecode) * 100 / segment_duration;

  return 100 * in->getFilePointer() / file_size;
}

void
kax_reader_c::set_headers() {
  generic_reader_c::set_headers();

  int i;
  for (i = 0; i < tracks.size(); i++)
    if ((-1 != tracks[i]->ptzr) && tracks[i]->passthrough)
      PTZR(tracks[i]->ptzr)->get_track_entry()->EnableLacing(tracks[i]->lacing_flag);
}

void
kax_reader_c::identify() {
  std::vector<std::string> verbose_info;

  if (!title.empty())
    verbose_info.push_back((boost::format("title:%1%") % escape(title)).str());
  if (0 != segment_duration)
    verbose_info.push_back((boost::format("duration:%1%") % segment_duration).str());

  id_result_container("Matroska", verbose_info);

  int i;
  for (i = 0; i < tracks.size(); i++) {
    if (!tracks[i]->ok)
      continue;

    verbose_info.clear();

    if (tracks[i]->language != "")
      verbose_info.push_back((boost::format("language:%1%") % escape(tracks[i]->language)).str());

    if (tracks[i]->track_name != "")
      verbose_info.push_back((boost::format("track_name:%1%") % escape(tracks[i]->track_name)).str());

    if ((0 != tracks[i]->v_dwidth) && (0 != tracks[i]->v_dheight))
      verbose_info.push_back((boost::format("display_dimensions:%1%x%2%") % tracks[i]->v_dwidth % tracks[i]->v_dheight).str());

    if (STEREO_MODE_UNSPECIFIED != tracks[i]->v_stereo_mode)
      verbose_info.push_back((boost::format("stereo_mode:%1%") % (int)tracks[i]->v_stereo_mode).str());

    verbose_info.push_back((boost::format("default_track:%1%") % (tracks[i]->default_track ? 1 : 0)).str());
    verbose_info.push_back((boost::format("forced_track:%1%")  % (tracks[i]->forced_track  ? 1 : 0)).str());

    if ((tracks[i]->codec_id == MKV_V_MSCOMP) && mpeg4::p10::is_avc_fourcc(tracks[i]->v_fourcc))
      verbose_info.push_back("packetizer:mpeg4_p10_es_video");
    else if (tracks[i]->codec_id == MKV_V_MPEG4_AVC)
      verbose_info.push_back("packetizer:mpeg4_p10_video");

    std::string info = tracks[i]->codec_id;

    if (tracks[i]->ms_compat)
      info += std::string(", ") +
        (  tracks[i]->type        == 'v'    ? tracks[i]->v_fourcc
         : tracks[i]->a_formattag == 0x0001 ? "PCM"
         : tracks[i]->a_formattag == 0x0003 ? "PCM"
         : tracks[i]->a_formattag == 0x0055 ? "MP3"
         : tracks[i]->a_formattag == 0x2000 ? "AC3"
         :                                   Y("unknown"));

    id_result_track(tracks[i]->tnum,
                      tracks[i]->type == 'v' ? ID_RESULT_TRACK_VIDEO
                    : tracks[i]->type == 'a' ? ID_RESULT_TRACK_AUDIO
                    : tracks[i]->type == 'b' ? ID_RESULT_TRACK_BUTTONS
                    : tracks[i]->type == 's' ? ID_RESULT_TRACK_SUBTITLES
                    :                          Y("unknown"),
                    info, verbose_info);
  }

  for (i = 0; i < g_attachments.size(); i++)
    id_result_attachment(g_attachments[i].ui_id, g_attachments[i].mime_type, g_attachments[i].data->get_size(), g_attachments[i].name, g_attachments[i].description);

  if (NULL != chapters)
    id_result_chapters(count_chapter_atoms(*chapters));

  if (NULL != m_tags)
    id_result_tags(ID_RESULT_GLOBAL_TAGS_ID, count_simple_tags(*m_tags));

  for (i = 0; tracks.size() > i; i++)
    if (tracks[i]->ok && (NULL != tracks[i]->tags))
      id_result_tags(tracks[i]->tnum, count_simple_tags(*tracks[i]->tags));
}

void
kax_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    available_track_ids.push_back(tracks[i]->tnum);
}

