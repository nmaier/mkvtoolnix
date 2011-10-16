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

#include "common/common_pch.h"

#include <avilib.h>   // for BITMAPINFOHEADER
#include <boost/math/common_factor.hpp>

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
#include "common/ebml.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/iso639.h"
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
#include "output/p_pgs.h"
#include "output/p_textsubs.h"
#include "output/p_theora.h"
#include "output/p_tta.h"
#include "output/p_video.h"
#include "output/p_vobbtn.h"
#include "output/p_vobsub.h"
#include "output/p_vorbis.h"
#include "output/p_vc1.h"
#include "output/p_vp8.h"
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
  (!p->IsFiniteSize() || (m_in->getFilePointer() < (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

#define is_ebmlvoid(e) (EbmlId(*e) == EBML_ID(EbmlVoid))

#define MAGIC_MKV 0x1a45dfa3

void
kax_track_t::handle_packetizer_display_dimensions() {
  // If user hasn't set an aspect ratio via the command line and the
  // source file provides display width/height paramaters then use
  // these and signal the packetizer not to extract the dimensions
  // from the bitstream.
  if ((0 == v_dwidth) || (0 == v_dheight))
    return;

  ptzr_ptr->set_video_display_dimensions(v_dwidth, v_dheight, PARAMETER_SOURCE_CONTAINER);
}

void
kax_track_t::handle_packetizer_pixel_cropping() {
  if ((0 < v_pcleft) || (0 < v_pctop) || (0 < v_pcright) || (0 < v_pcbottom))
    ptzr_ptr->set_video_pixel_cropping(v_pcleft, v_pctop, v_pcright, v_pcbottom, PARAMETER_SOURCE_CONTAINER);
}

void
kax_track_t::handle_packetizer_stereo_mode() {
  if (stereo_mode_c::unspecified != v_stereo_mode)
    ptzr_ptr->set_video_stereo_mode(v_stereo_mode, PARAMETER_SOURCE_CONTAINER);
}

void
kax_track_t::handle_packetizer_pixel_dimensions() {
  if ((0 == v_width) || (0 == v_height))
    return;

  ptzr_ptr->set_video_pixel_width(v_width);
  ptzr_ptr->set_video_pixel_height(v_height);
}

void
kax_track_t::handle_packetizer_default_duration() {
  if (0 != default_duration)
    ptzr_ptr->set_track_default_duration(default_duration);
}

/* Fix display dimension parameters

   Certain Matroska muxers abuse the DisplayWidth/DisplayHeight
   parameters for only storing an aspect ratio. These values are
   usually very small, e.g. 16/9. Fix them so that the quotient is
   kept but the values are in the range of the PixelWidth/PixelHeight
   elements.
 */
void
kax_track_t::fix_display_dimension_parameters() {
  if (0 != v_display_unit)
    return;

  if (((8 * v_dwidth) > v_width) || ((8 * v_dheight) > v_height))
    return;

  if (boost::math::gcd(v_dwidth, v_dheight) == 1) { // max shrinking was applied, ie x264 style
    if (v_dwidth > v_dheight) {
      if (((v_height * v_dwidth) % v_dheight) == 0) { // only if we get get an exact count of pixels
        v_dwidth  = v_height * v_dwidth / v_dheight;
        v_dheight = v_height;
      }
    } else if (((v_width * v_dheight) % v_dwidth) == 0) {
      v_dwidth  = v_width;
      v_dheight = v_width * v_dheight / v_dwidth;
    }
  }
}

/*
   Probes a file by simply comparing the first four bytes to the EBML
   head signature.
*/
int
kax_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
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
  , m_segment_duration(0)
  , m_last_timecode(0)
  , m_first_timecode(-1)
  , m_writing_app_ver(-1)
  , m_attachment_id(0)
  , m_file_status(FILE_STATUS_MOREDATA)
{
  init_l1_position_storage(m_deferred_l1_positions);
  init_l1_position_storage(m_handled_l1_positions);

  if (!read_headers())
    throw error_c(Y("matroska_reader: Failed to read the headers."));
  show_demuxer_info();
}

kax_reader_c::~kax_reader_c() {
}

void
kax_reader_c::init_l1_position_storage(deferred_positions_t &storage) {
  storage[dl1t_attachments] = std::vector<int64_t>();
  storage[dl1t_chapters]    = std::vector<int64_t>();
  storage[dl1t_tags]        = std::vector<int64_t>();
  storage[dl1t_tracks]      = std::vector<int64_t>();
}

int
kax_reader_c::packets_available() {
  if (m_tracks.empty())
    return 0;

  foreach(kax_track_cptr &track, m_tracks)
    if ((-1 != track->ptzr) && !PTZR(track->ptzr)->packet_available())
      return 0;

  return 1;
}

kax_track_t *
kax_reader_c::find_track_by_num(uint64_t n,
                                kax_track_t *c) {
  foreach(kax_track_cptr &track, m_tracks)
    if ((track->tnum == n) && (track.get_object() != c))
      return track.get_object();

  return NULL;
}

kax_track_t *
kax_reader_c::find_track_by_uid(uint64_t uid,
                                kax_track_t *c) {
  foreach(kax_track_cptr &track, m_tracks)
    if ((track->tuid == uid) && (track.get_object() != c))
      return track.get_object();

  return NULL;
}

bool
kax_reader_c::unlace_vorbis_private_data(kax_track_t *t,
                                         unsigned char *buffer,
                                         int size) {
  try {
    memory_cptr temp(new memory_c(buffer, size, false));
    std::vector<memory_cptr> blocks = unlace_memory_xiph(temp);
    if (blocks.size() != 3)
      return false;

    for (unsigned int i = 0; 3 > i; ++i) {
      t->headers[i]      = blocks[i]->get_buffer();
      t->header_sizes[i] = blocks[i]->get_size();
    }

  } catch (...) {
    return false;
  }

  return true;
}

bool
kax_reader_c::verify_acm_audio_track(kax_track_t *t) {
  if ((NULL == t->private_data) || (sizeof(alWAVEFORMATEX) > t->private_size)) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no WAVEFORMATEX struct present. "
                             "Therefore we don't have a format ID to identify the audio codec used.\n")) % t->tnum % MKV_A_ACM);
    return false;

  }

  alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)t->private_data;
  t->a_formattag      = get_uint16_le(&wfe->w_format_tag);

  if ((0xfffe == t->a_formattag) && (!unlace_vorbis_private_data(t, static_cast<unsigned char *>(t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX)))) {
    // Force the passthrough packetizer to be used if the data behind
    // the WAVEFORMATEX does not contain valid laced Vorbis headers.
    t->a_formattag = 0;
    return true;
  }

  t->ms_compat = 1;
  uint32_t u   = get_uint32_le(&wfe->n_samples_per_sec);

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

  return true;
}

bool
kax_reader_c::verify_flac_audio_track(kax_track_t *t) {
#if defined(HAVE_FLAC_FORMAT_H)
  t->a_formattag = FOURCC('f', 'L', 'a', 'C');
  return true;

#else
  mxwarn(boost::format(Y("matroska_reader: mkvmerge was not compiled with FLAC support. Ignoring track %1%.\n")) % t->tnum);
  return false;
#endif
}

bool
kax_reader_c::verify_vorbis_audio_track(kax_track_t *t) {
  if (NULL == t->private_data) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is 'A_VORBIS', but there are no header packets present.\n")) % t->tnum);
    return false;
  }

  if (!unlace_vorbis_private_data(t, static_cast<unsigned char *>(t->private_data), t->private_size)) {
    if (verbose)
      mxwarn(Y("matroska_reader: Vorbis track does not contain valid headers.\n"));

    return false;
  }

  t->a_formattag = 0xFFFE;

  // mkvmerge around 0.6.x had a bug writing a default duration
  // for Vorbis m_tracks but not the correct durations for the
  // individual packets. This comes back to haunt us because
  // when regenerating the timestamps from lacing those durations
  // are used and add up to too large a value. The result is the
  // usual "timecode < m_last_timecode" message.
  // Workaround: do not use durations for such m_tracks.
  if ((m_writing_app == "mkvmerge") && (-1 != m_writing_app_ver) && (0x07000000 > m_writing_app_ver))
    t->ignore_duration_hack = true;

  return true;
}

void
kax_reader_c::verify_audio_track(kax_track_t *t) {
  if (t->codec_id.empty())
    return;

  bool is_ok = true;
  if (t->codec_id == MKV_A_ACM)
    is_ok = verify_acm_audio_track(t);
  else if ((t->codec_id == MKV_A_MP3) || (t->codec_id == MKV_A_MP2))
    t->a_formattag = 0x0055;
  else if (ba::starts_with(t->codec_id, MKV_A_AC3) || (t->codec_id == MKV_A_EAC3))
    t->a_formattag = 0x2000;
  else if (t->codec_id == MKV_A_DTS)
    t->a_formattag = 0x2001;
  else if (t->codec_id == MKV_A_PCM)
    t->a_formattag = 0x0001;
  else if (t->codec_id == MKV_A_PCM_FLOAT)
    t->a_formattag = 0x0003;
  else if (t->codec_id == MKV_A_VORBIS)
    is_ok = verify_vorbis_audio_track(t);
  else if (   (t->codec_id == MKV_A_AAC_2MAIN)
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
  else if (ba::starts_with(t->codec_id, "A_REAL/"))
    t->a_formattag = FOURCC('r', 'e', 'a', 'l');
  else if (t->codec_id == MKV_A_FLAC)
    is_ok = verify_flac_audio_track(t);
  else if (t->codec_id == MKV_A_TTA)
    t->a_formattag = FOURCC('T', 'T', 'A', '1');
  else if (t->codec_id == MKV_A_WAVPACK4)
    t->a_formattag = FOURCC('W', 'V', 'P', '4');

  if (!is_ok)
    return;

  if (0.0 == t->a_sfreq)
    t->a_sfreq = 8000.0;

  if (0 == t->a_channels)
    t->a_channels = 1;

  // This track seems to be ok.
  t->ok = 1;
}

bool
kax_reader_c::verify_mscomp_video_track(kax_track_t *t) {
  if ((NULL == t->private_data) || (sizeof(alBITMAPINFOHEADER) > t->private_size)) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no BITMAPINFOHEADER struct present. "
                             "Therefore we don't have a FourCC to identify the video codec used.\n"))
             % t->tnum % MKV_V_MSCOMP);
      return false;
  }

  t->ms_compat            = 1;
  alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)t->private_data;
  uint32_t u              = get_uint32_le(&bih->bi_width);

  if (t->v_width != u) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode, track %1%) Matroska says video width is %2%, but the BITMAPINFOHEADER says %3%.\n"))
             % t->tnum % t->v_width % u);
    if (0 == t->v_width)
      t->v_width = u;
  }

  u = get_uint32_le(&bih->bi_height);
  if (t->v_height != u) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode, track %1%) Matroska says video height is %2%, but the BITMAPINFOHEADER says %3%.\n"))
             % t->tnum % t->v_height % u);
    if (0 == t->v_height)
      t->v_height = u;
  }

  memcpy(t->v_fourcc, &bih->bi_compression, 4);

  return true;
}

bool
kax_reader_c::verify_theora_video_track(kax_track_t *t) {
  if (NULL != t->private_data)
    return true;

  if (verbose)
    mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no codec private headers.\n")) % t->tnum % MKV_V_THEORA);

  return false;
}

void
kax_reader_c::verify_video_track(kax_track_t *t) {
  if (t->codec_id == "")
    return;

  bool is_ok = true;
  if (t->codec_id == MKV_V_MSCOMP)
    is_ok = verify_mscomp_video_track(t);

  else if (t->codec_id == MKV_V_THEORA)
    is_ok = verify_theora_video_track(t);

  if (!is_ok)
    return;

  if (0 == t->v_width) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The width for track %1% was not set.\n")) % t->tnum);
    return;
  }

  if (0 == t->v_height) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The height for track %1% was not set.\n")) % t->tnum);
    return;
  }

  // This track seems to be ok.
  t->ok = 1;
}

bool
kax_reader_c::verify_kate_subtitle_track(kax_track_t *t) {
  if (NULL != t->private_data)
    return true;

  if (verbose)
    mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no private data found.\n")) % t->tnum % t->codec_id);

  return false;
}

bool
kax_reader_c::verify_vobsub_subtitle_track(kax_track_t *t) {
  if (NULL != t->private_data)
    return true;

  if (verbose)
    mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no private data found.\n")) % t->tnum % t->codec_id);

  return false;
}

void
kax_reader_c::verify_subtitle_track(kax_track_t *t) {
  bool is_ok = true;

  if (t->codec_id == MKV_S_VOBSUB)
    is_ok = verify_vobsub_subtitle_track(t);

  else if (t->codec_id == MKV_S_KATE)
    is_ok = verify_kate_subtitle_track(t);

  t->ok = is_ok ? 1 : 0;
}

void
kax_reader_c::verify_button_track(kax_track_t *t) {
  if (t->codec_id != MKV_B_VOBBTN) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID '%1%' for track %2% is unknown.\n")) % t->codec_id % t->tnum);
    return;
  }

  t->ok = 1;
}

void
kax_reader_c::verify_tracks() {
  size_t tnum;
  kax_track_t *t;

  for (tnum = 0; tnum < m_tracks.size(); tnum++) {
    t = m_tracks[tnum].get_object();

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
        verify_video_track(t);
        break;

      case 'a':                 // audio track
        verify_audio_track(t);
        break;

      case 's':
        verify_subtitle_track(t);
        break;

      case 'b':
        verify_button_track(t);
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

bool
kax_reader_c::has_deferred_element_been_processed(kax_reader_c::deferred_l1_type_e type,
                                                  int64_t position) {
  foreach(int64_t test_position, m_handled_l1_positions[type])
    if (position == test_position)
      return true;

  m_handled_l1_positions[type].push_back(position);

  return false;
}

void
kax_reader_c::handle_attachments(mm_io_c *io,
                                 EbmlElement *l0,
                                 int64_t pos) {
  if (has_deferred_element_been_processed(dl1t_attachments, pos))
    return;

  io->save_pos(pos);
  int upper_lvl_el;
  EbmlElement *l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

  if ((NULL != l1) && (EbmlId(*l1) == EBML_ID(KaxAttachments))) {
    KaxAttachments *atts = (KaxAttachments *)l1;
    EbmlElement *l2      = NULL;
    upper_lvl_el         = 0;

    atts->Read(*m_es, EBML_CLASS_CONTEXT(KaxAttachments), upper_lvl_el, l2, true);

    size_t i;
    for (i = 0; i < atts->ListSize(); i++) {
      KaxAttached *att = (KaxAttached *)(*atts)[i];

      if (EbmlId(*att) == EBML_ID(KaxAttached)) {
        UTFstring name        = L"";
        UTFstring description = L"";
        std::string mime_type =  "";
        int64_t size          = -1;
        int64_t id            = -1;
        unsigned char *data   = NULL;
        size_t k;

        for (k = 0; k < att->ListSize(); k++) {
          l2 = (*att)[k];

          if (EbmlId(*l2) == EBML_ID(KaxFileName)) {
            KaxFileName &fname = *static_cast<KaxFileName *>(l2);
            name               = UTFstring(fname);

          } else if (EbmlId(*l2) == EBML_ID(KaxFileDescription)) {
            KaxFileDescription &fdesc = *static_cast<KaxFileDescription *>(l2);
            description               = UTFstring(fdesc);

          } else if (EbmlId(*l2) == EBML_ID(KaxMimeType)) {
            KaxMimeType &mtype = *static_cast<KaxMimeType *>(l2);
            mime_type          = std::string(mtype);

          } else if (EbmlId(*l2) == EBML_ID(KaxFileUID)) {
            KaxFileUID &fuid = *static_cast<KaxFileUID *>(l2);
            id               = uint64(fuid);

          } else if (EbmlId(*l2) == EBML_ID(KaxFileData)) {
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
  if (has_deferred_element_been_processed(dl1t_chapters, pos))
    return;

  int upper_lvl_el = 0;
  io->save_pos(pos);
  EbmlElement *l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

  if ((l1 != NULL) && is_id(l1, KaxChapters)) {
    KaxChapters *tmp_chapters = static_cast<KaxChapters *>(l1);
    EbmlElement *l2           = NULL;
    upper_lvl_el              = 0;

    tmp_chapters->Read(*m_es, EBML_CLASS_CONTEXT(KaxChapters), upper_lvl_el, l2, true);

    if (NULL == m_chapters)
      m_chapters = new KaxChapters;

    size_t i;
    for (i = 0; i < tmp_chapters->ListSize(); i++)
      m_chapters->PushElement(*(*tmp_chapters)[i]);
    tmp_chapters->RemoveAll();

  }

  delete l1;

  io->restore_pos();
}

void
kax_reader_c::handle_tags(mm_io_c *io,
                          EbmlElement *l0,
                          int64_t pos) {
  if (has_deferred_element_been_processed(dl1t_tags, pos))
    return;

  int upper_lvl_el = 0;
  io->save_pos(pos);
  EbmlElement *l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

  if ((NULL != l1) && (EbmlId(*l1) == EBML_ID(KaxTags))) {
    KaxTags *tags   = (KaxTags *)l1;
    EbmlElement *l2 = NULL;
    upper_lvl_el    = 0;

    tags->Read(*m_es, EBML_CLASS_CONTEXT(KaxTags), upper_lvl_el, l2, true);

    while (tags->ListSize() > 0) {
      if (!(EbmlId(*(*tags)[0]) == EBML_ID(KaxTag))) {
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

            size_t i;
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
        if (!m_tags.is_set())
          m_tags = counted_ptr<KaxTags>(new KaxTags);
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

  l1->Read(*m_es, EBML_CLASS_CONTEXT(KaxInfo), upper_lvl_el, l2, true);

  KaxTimecodeScale *ktc_scale = FINDFIRST(l1, KaxTimecodeScale);
  if (NULL != ktc_scale) {
    m_tc_scale = uint64(*ktc_scale);
    mxverb(2, boost::format("matroska_reader: | + timecode scale: %1%\n") % m_tc_scale);

  } else
    m_tc_scale = 1000000;

  KaxDuration *kduration = FINDFIRST(l1, KaxDuration);
  if (NULL != kduration) {
    m_segment_duration = irnd(double(*kduration) * m_tc_scale);
    mxverb(2, boost::format("matroska_reader: | + duration: %|1$.3f|s\n") % (m_segment_duration / 1000000000.0));
  }

  KaxTitle *ktitle = FINDFIRST(l1, KaxTitle);
  if (NULL != ktitle) {
    m_title = UTFstring_to_cstrutf8(UTFstring(*ktitle));
    mxverb(2, boost::format("matroska_reader: | + title: %1%\n") % m_title);
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
  KaxWritingApp *km_writing_app = FINDFIRST(l1, KaxWritingApp);
  if (NULL != km_writing_app)
    read_headers_info_writing_app(km_writing_app);

  KaxMuxingApp *km_muxing_app = FINDFIRST(l1, KaxMuxingApp);
  if (NULL != km_muxing_app) {
    m_muxing_app = UTFstring_to_cstrutf8(UTFstring(*km_muxing_app));
    mxverb(3, boost::format("matroska_reader: |   (m_muxing_app '%1%')\n") % m_muxing_app);

    // DirectShow Muxer workaround: Gabest's DirectShow muxer
    // writes wrong references (off by 1ms). So let the cluster
    // helper be a bit more imprecise in what it accepts when
    // looking for referenced packets.
    if (m_muxing_app == "DirectShow Matroska Muxer")
      m_reference_timecode_tolerance = 1000000;
  }
}

void
kax_reader_c::read_headers_info_writing_app(KaxWritingApp *&km_writing_app) {
  size_t idx;

  std::string s = UTFstring_to_cstrutf8(UTFstring(*km_writing_app));
  strip(s);
  mxverb(2, boost::format("matroska_reader: | + writing app: %1%\n") % s);

  if (ba::istarts_with(s, "avi-mux gui"))
    s.replace(0, strlen("avi-mux gui"), "avimuxgui");

  std::vector<std::string> parts = split(s.c_str(), " ", 3);
  if (parts.size() < 2) {
    m_writing_app = "";
    for (idx = 0; idx < s.size(); idx++)
      m_writing_app += tolower(s[idx]);
    m_writing_app_ver = -1;

  } else {

    m_writing_app = "";
    for (idx = 0; idx < parts[0].size(); idx++)
      m_writing_app += tolower(parts[0][idx]);
    s = "";

    for (idx = 0; idx < parts[1].size(); idx++)
      if (isdigit(parts[1][idx]) || (parts[1][idx] == '.'))
        s += parts[1][idx];

    std::vector<std::string> ver_parts = split(s.c_str(), ".");
    for (idx = ver_parts.size(); idx < 4; idx++)
      ver_parts.push_back("0");

    bool failed     = false;
    m_writing_app_ver = 0;

    for (idx = 0; idx < 4; idx++) {
      int num;

      if (!parse_int(ver_parts[idx], num) || (0 > num) || (255 < num)) {
        failed = true;
        break;
      }
      m_writing_app_ver <<= 8;
      m_writing_app_ver  |= num;
    }
    if (failed)
      m_writing_app_ver = -1;
  }

  mxverb(3, boost::format("matroska_reader: |   (m_writing_app '%1%', m_writing_app_ver 0x%|2$08x|)\n") % m_writing_app % (unsigned int)m_writing_app_ver);
}

void
kax_reader_c::read_headers_track_audio(kax_track_t *track,
                                       KaxTrackAudio *ktaudio) {
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
kax_reader_c::read_headers_track_video(kax_track_t *track,
                                       KaxTrackVideo *ktvideo) {
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

#if MATROSKA_VERSION >= 2
  KaxVideoDisplayUnit *kv_dunit = FINDFIRST(ktvideo, KaxVideoDisplayUnit);
  if (NULL != kv_dunit) {
    track->v_display_unit = uint64(*kv_dunit);
    mxverb(2, boost::format("matroska_reader: |   + Display unit: %1%\n") % track->v_display_unit);
  }
#endif

  track->fix_display_dimension_parameters();

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
    track->v_stereo_mode = static_cast<stereo_mode_c::mode>(uint64(*kv_stereo_mode));
    mxverb(2, boost::format("matroska_reader: |   + Stereo mode: %1%\n") % static_cast<int>(track->v_stereo_mode));
  }
}

void
kax_reader_c::read_headers_tracks(mm_io_c *io,
                                  EbmlElement *l0,
                                  int64_t position) {
  // Yep, we've found our KaxTracks element. Now find all m_tracks
  // contained in this segment.
  if (has_deferred_element_been_processed(dl1t_tracks, position))
    return;

  mxverb(2, "matroska_reader: |+ segment m_tracks...\n");

  int upper_lvl_el = 0;
  io->save_pos(position);
  EbmlElement *l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

  if ((l1 == NULL) || !is_id(l1, KaxTracks)) {
    delete l1;
    io->restore_pos();

    return;
  }

  EbmlElement *l2 = NULL;
  upper_lvl_el    = 0;
  l1->Read(*m_es, EBML_CLASS_CONTEXT(KaxTracks), upper_lvl_el, l2, true);

  KaxTrackEntry *ktentry = FINDFIRST(l1, KaxTrackEntry);
  while (ktentry != NULL) {
    // We actually found a track entry :) We're happy now.
    mxverb(2, "matroska_reader: | + a track...\n");

    kax_track_cptr track(new kax_track_t);

    KaxTrackNumber *ktnum = FINDFIRST(ktentry, KaxTrackNumber);
    if (NULL == ktnum)
      mxerror(Y("matroska_reader: A track is missing its track number.\n"));
    mxverb(2, boost::format("matroska_reader: |  + Track number: %1%\n") % (int)uint8(*ktnum));

    track->tnum = uint8(*ktnum);
    if (find_track_by_num(track->tnum, track.get_object()) != NULL) {
      mxverb(2, boost::format(Y("matroska_reader: |  + There's more than one track with the number %1%.\n")) % track->tnum);
      ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
      continue;
    }

    KaxTrackUID *ktuid = FINDFIRST(ktentry, KaxTrackUID);
    if (NULL == ktuid)
      mxerror(Y("matroska_reader: A track is missing its track UID.\n"));
    mxverb(2, boost::format("matroska_reader: |  + Track UID: %1%\n") % uint64(*ktuid));
    track->tuid = uint64(*ktuid);
    if ((find_track_by_uid(track->tuid, track.get_object()) != NULL) && (verbose > 1))
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
      read_headers_track_audio(track.get_object(), ktaudio);

    KaxTrackVideo *ktvideo = FINDFIRST(ktentry, KaxTrackVideo);
    if (NULL != ktvideo)
      read_headers_track_video(track.get_object(), ktvideo);

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
    }

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
      int index       = map_to_iso639_2_code(track->language.c_str());
      if (-1 != index)
        track->language = iso639_languages[index].iso639_2_code;
    }

    KaxTrackName *ktname = FINDFIRST(ktentry, KaxTrackName);
    if (NULL != ktname) {
      track->track_name = UTFstring_to_cstrutf8(UTFstring(*ktname));
      mxverb(2, boost::format("matroska_reader: |  + Name: %1%\n") % track->track_name);
    }

    track->content_decoder.initialize(*ktentry);
    m_tracks.push_back(track);

    ktentry = FINDNEXT(l1, KaxTrackEntry, ktentry);
  } // while (ktentry != NULL)

  delete l1;
  io->restore_pos();
}

void
kax_reader_c::read_headers_seek_head(EbmlElement *l0,
                                     EbmlElement *l1) {
  EbmlElement *el;

  KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);

  int i = 0;
  seek_head.Read(*m_es, EBML_CLASS_CONTEXT(KaxSeekHead), i, el, true);

  for (i = 0; i < static_cast<int>(seek_head.ListSize()); i++) {
    if (EbmlId(*seek_head[i]) != EBML_ID(KaxSeek))
      continue;

    KaxSeek &seek           = *static_cast<KaxSeek *>(seek_head[i]);
    int64_t pos             = -1;
    deferred_l1_type_e type = dl1t_unknown;
    size_t k;

    for (k = 0; k < seek.ListSize(); k++)
      if (EbmlId(*seek[k]) == EBML_ID(KaxSeekID)) {
        KaxSeekID &sid = *static_cast<KaxSeekID *>(seek[k]);
        EbmlId id(sid.GetBuffer(), sid.GetSize());

        type = id == EBML_ID(KaxAttachments) ? dl1t_attachments
          :    id == EBML_ID(KaxChapters)    ? dl1t_chapters
          :    id == EBML_ID(KaxTags)        ? dl1t_tags
          :    id == EBML_ID(KaxTracks)      ? dl1t_tracks
          :                                    dl1t_unknown;

      } else if (EbmlId(*seek[k]) == EBML_ID(KaxSeekPosition))
        pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

    if ((-1 != pos) && (dl1t_unknown != type)) {
      pos = ((KaxSegment *)l0)->GetGlobalPosition(pos);
      m_deferred_l1_positions[type].push_back(pos);
    }
  }
}

bool
kax_reader_c::read_headers() {
  // Elements for different levels

  KaxCluster *cluster = NULL;
  try {
    m_in        = mm_io_cptr(new mm_file_io_c(m_ti.m_fname));
    m_file_size = m_in->get_size();
    m_es        = counted_ptr<EbmlStream>(new EbmlStream(*m_in));
    m_in_file   = kax_file_cptr(new kax_file_c(m_in));

    // Find the EbmlHead element. Must be the first one.
    EbmlElement *l0 = m_es->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFFFFFFFFFLL);
    if (NULL == l0) {
      mxwarn(Y("matroska_reader: no EBML head found.\n"));
      return false;
    }

    // Don't verify its data for now.
    l0->SkipData(*m_es, EBML_CONTEXT(l0));
    delete l0;
    mxverb(2, "matroska_reader: Found the head...\n");

    // Next element must be a segment
    l0 = m_es->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFFFFFFFFFLL);
    counted_ptr<EbmlElement> l0_cptr(l0);
    if (NULL == l0) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return false;
    }
    if (!(EbmlId(*l0) == EBML_ID(KaxSegment))) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return false;
    }
    mxverb(2, "matroska_reader: + a segment...\n");

    // We've got our segment, so let's find the m_tracks
    int upper_lvl_el = 0;
    m_tc_scale         = TIMECODE_SCALE;
    EbmlElement *l1  = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true, 1);

    while ((NULL != l1) && (0 >= upper_lvl_el)) {
      EbmlElement *l2;

      if (EbmlId(*l1) == EBML_ID(KaxInfo))
        read_headers_info(l1, l2, upper_lvl_el);

      else if (EbmlId(*l1) == EBML_ID(KaxTracks))
        m_deferred_l1_positions[dl1t_tracks].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxAttachments))
        m_deferred_l1_positions[dl1t_attachments].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxChapters))
        m_deferred_l1_positions[dl1t_chapters].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxTags))
        m_deferred_l1_positions[dl1t_tags].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxSeekHead))
        read_headers_seek_head(l0, l1);

      else if (EbmlId(*l1) == EBML_ID(KaxCluster)) {
        mxverb(2, "matroska_reader: |+ found cluster, headers are parsed completely\n");
        cluster = static_cast<KaxCluster *>(l1);

      } else
        l1->SkipData(*m_es, EBML_CONTEXT(l1));

      if (NULL != cluster)      // we've found the first cluster, so get out
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

      l1->SkipData(*m_es, EBML_CONTEXT(l1));
      delete l1;
      l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    foreach(int64_t position, m_deferred_l1_positions[dl1t_tracks])
      read_headers_tracks(m_in.get_object(), l0, position);

    if (!m_ti.m_attach_mode_list.none()) {
      foreach(int64_t position, m_deferred_l1_positions[dl1t_attachments])
        handle_attachments(m_in.get_object(), l0, position);
    }

    if (!m_ti.m_no_chapters) {
      foreach(int64_t position, m_deferred_l1_positions[dl1t_chapters])
        handle_chapters(m_in.get_object(), l0, position);
    }

    foreach(int64_t position, m_deferred_l1_positions[dl1t_tags])
      handle_tags(m_in.get_object(), l0, position);

    if (!m_ti.m_no_global_tags)
      process_global_tags();

  } catch (...) {
    mxerror(Y("matroska_reader: caught exception\n"));
  }

  verify_tracks();

  if (NULL != cluster) {
    m_in->setFilePointer(cluster->GetElementPosition(), seek_beginning);
    delete cluster;

  } else
    m_in->setFilePointer(0, seek_end);

  return true;
}

void
kax_reader_c::process_global_tags() {
  if (!m_tags.is_set() || g_identifying)
    return;

  size_t i;
  for (i = 0; m_tags->ListSize() > i; ++i)
    add_tags(static_cast<KaxTag *>((*m_tags)[i]));

  m_tags->RemoveAll();
}

void
kax_reader_c::init_passthrough_packetizer(kax_track_t *t) {
  passthrough_packetizer_c *ptzr;
  track_info_c nti(m_ti);

  mxinfo_tid(m_ti.m_fname, t->tnum, boost::format(Y("Using the output module for track type '%1%'.\n")) % MAP_TRACK_TYPE_STRING(t->type));

  nti.m_id                  = t->tnum;
  nti.m_language            = t->language;
  nti.m_track_name          = t->track_name;

  ptzr                      = new passthrough_packetizer_c(this, nti);
  t->ptzr                   = add_packetizer(ptzr);
  t->ptzr_ptr               = ptzr;
  t->passthrough            = true;
  m_ptzr_to_track_map[ptzr] = t;

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

    t->handle_packetizer_display_dimensions();
    t->handle_packetizer_pixel_cropping();
    t->handle_packetizer_stereo_mode();

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
  if (m_appending)
    return;

  if (t->default_track)
    PTZR(t->ptzr)->set_as_default_track(t->type == 'v' ? DEFTRACK_TYPE_VIDEO : t->type == 'a' ? DEFTRACK_TYPE_AUDIO : DEFTRACK_TYPE_SUBS, DEFAULT_TRACK_PRIORITY_FROM_SOURCE);

  else if (!boost::logic::indeterminate(t->default_track) && boost::logic::indeterminate(PTZR(t->ptzr)->m_ti.m_default_track))
    PTZR(t->ptzr)->m_ti.m_default_track = false;

  if (t->forced_track && boost::logic::indeterminate(PTZR(t->ptzr)->m_ti.m_forced_track))
    PTZR(t->ptzr)->set_track_forced_flag(true);

  if ((0 != t->tuid) && !PTZR(t->ptzr)->set_uid(t->tuid))
    mxwarn(boost::format(Y("matroska_reader: Could not keep the track UID %1% because it is already allocated for the new file.\n")) % t->tuid);
}

void
kax_reader_c::create_video_packetizer(kax_track_t *t,
                                      track_info_c &nti) {
  if ((t->codec_id == MKV_V_MSCOMP) && mpeg4::p10::is_avc_fourcc(t->v_fourcc) && !hack_engaged(ENGAGE_ALLOW_AVC_IN_VFW_MODE))
    create_mpeg4_p10_es_video_packetizer(t, nti);

  else if (   ba::starts_with(t->codec_id, "V_MPEG4")
           || (t->codec_id == MKV_V_MSCOMP)
           || ba::starts_with(t->codec_id, "V_REAL")
           || (t->codec_id == MKV_V_QUICKTIME)
           || (t->codec_id == MKV_V_MPEG1)
           || (t->codec_id == MKV_V_MPEG2)
           || (t->codec_id == MKV_V_THEORA)
           || (t->codec_id == MKV_V_DIRAC)
           || (t->codec_id == MKV_V_VP8)) {
    if ((t->codec_id == MKV_V_MPEG1) || (t->codec_id == MKV_V_MPEG2)) {
      int version = t->codec_id[6] - '0';
      set_track_packetizer(t, new mpeg1_2_video_packetizer_c(this, nti, version, t->v_frate, t->v_width, t->v_height, t->v_dwidth, t->v_dheight, true));
      show_packetizer_info(t->tnum, t->ptzr_ptr);

    } else if (IS_MPEG4_L2_CODECID(t->codec_id) || IS_MPEG4_L2_FOURCC(t->v_fourcc)) {
      bool is_native = IS_MPEG4_L2_CODECID(t->codec_id);
      set_track_packetizer(t, new mpeg4_p2_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height, is_native));
      show_packetizer_info(t->tnum, t->ptzr_ptr);

    } else if (t->codec_id == MKV_V_MPEG4_AVC)
      create_mpeg4_p10_video_packetizer(t, nti);

    else if (t->codec_id == MKV_V_THEORA) {
      set_track_packetizer(t, new theora_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height));
      show_packetizer_info(t->tnum, t->ptzr_ptr);

    } else if (t->codec_id == MKV_V_DIRAC) {
      set_track_packetizer(t, new dirac_video_packetizer_c(this, nti));
      show_packetizer_info(t->tnum, t->ptzr_ptr);

    } else if (t->codec_id == MKV_V_VP8) {
      set_track_packetizer(t, new vp8_video_packetizer_c(this, nti));
      show_packetizer_info(t->tnum, t->ptzr_ptr);
      t->handle_packetizer_pixel_dimensions();
      t->handle_packetizer_default_duration();

    } else if ((t->codec_id == MKV_V_MSCOMP) && vc1::is_fourcc(t->v_fourcc))
      create_vc1_video_packetizer(t, nti);

    else {
      set_track_packetizer(t, new video_packetizer_c(this, nti, t->codec_id.c_str(), t->v_frate, t->v_width, t->v_height));
      show_packetizer_info(t->tnum, t->ptzr_ptr);
    }

    t->handle_packetizer_display_dimensions();
    t->handle_packetizer_pixel_cropping();
    t->handle_packetizer_stereo_mode();

  } else
    init_passthrough_packetizer(t);
}

void
kax_reader_c::create_aac_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
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
        mxerror_tid(m_ti.m_fname, t->tnum, Y("Malformed AAC codec initialization data found.\n"));

      detected_profile = profile;
      id               = AAC_ID_MPEG4;
      if (sbr)
        profile        = AAC_PROFILE_SBR;

    } else if (!parse_aac_codec_id(std::string(t->codec_id), id, profile))
      mxerror_tid(m_ti.m_fname, t->tnum, boost::format(Y("Malformed codec id '%1%'.\n")) % t->codec_id);

  } else {
    int channels, sfreq, osfreq;
    bool sbr;

    if (!parse_aac_data(((unsigned char *)t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX), profile, channels, sfreq, osfreq, sbr))
      mxerror_tid(m_ti.m_fname, t->tnum, Y("Malformed AAC codec initialization data found.\n"));

    detected_profile = profile;
    id               = AAC_ID_MPEG4;
    if (sbr)
      profile        = AAC_PROFILE_SBR;
  }

  if ((map_has_key(m_ti.m_all_aac_is_sbr, t->tnum) &&  m_ti.m_all_aac_is_sbr[t->tnum]) || (map_has_key(m_ti.m_all_aac_is_sbr, -1) &&  m_ti.m_all_aac_is_sbr[-1]))
    profile = AAC_PROFILE_SBR;

  if ((map_has_key(m_ti.m_all_aac_is_sbr, t->tnum) && !m_ti.m_all_aac_is_sbr[t->tnum]) || (map_has_key(m_ti.m_all_aac_is_sbr, -1) && !m_ti.m_all_aac_is_sbr[-1]))
    profile = detected_profile;

  set_track_packetizer(t, new aac_packetizer_c(this, nti, id, profile, (int32_t)t->a_sfreq, t->a_channels, false, true));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_ac3_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  unsigned int bsid = t->codec_id == "A_AC3/BSID9"  ?  9
                    : t->codec_id == "A_AC3/BSID10" ? 10
                    : t->codec_id == MKV_A_EAC3     ? 16
                    :                                  0;

  set_track_packetizer(t, new ac3_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, bsid));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_dts_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  try {
    read_first_frames(t, 5);

    byte_buffer_c buffer;
    foreach(memory_cptr &frame, t->first_frames_data)
      buffer.add(frame);

    dts_header_t dtsheader;
    int position = find_dts_header(buffer.get_buffer(), buffer.get_size(), &dtsheader);

    if (-1 == position)
      throw false;

    set_track_packetizer(t, new dts_packetizer_c(this, nti, dtsheader, true));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

  } catch (...) {
    mxerror_tid(m_ti.m_fname, t->tnum, Y("Could not find valid DTS headers in this track's first frames.\n"));
  }
}

#if defined(HAVE_FLAC_FORMAT_H)
void
kax_reader_c::create_flac_audio_packetizer(kax_track_t *t,
                                           track_info_c &nti) {
  safefree(nti.m_private_data);
  nti.m_private_data = NULL;
  nti.m_private_size = 0;

  if (FOURCC('f', 'L', 'a', 'C') == t->a_formattag)
    set_track_packetizer(t, new flac_packetizer_c(this, nti, (unsigned char *) t->private_data, t->private_size));
  else
    set_track_packetizer(t, new flac_packetizer_c(this, nti, ((unsigned char *)t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX)));

  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

#endif  // HAVE_FLAC_FORMAT_H

void
kax_reader_c::create_mp3_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new mp3_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, true));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_pcm_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new pcm_packetizer_c(this, nti, (int32_t)t->a_sfreq, t->a_channels, t->a_bps, false, 0x0003 == t->a_formattag));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_tta_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  safefree(nti.m_private_data);
  nti.m_private_data = NULL;
  nti.m_private_size = 0;

  set_track_packetizer(t, new tta_packetizer_c(this, nti, t->a_channels, t->a_bps, (int32_t)t->a_sfreq));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_vorbis_audio_packetizer(kax_track_t *t,
                                             track_info_c &nti) {
  set_track_packetizer(t, new vorbis_packetizer_c(this, nti, t->headers[0], t->header_sizes[0], t->headers[1], t->header_sizes[1], t->headers[2], t->header_sizes[2]));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_wavpack_audio_packetizer(kax_track_t *t,
                                              track_info_c &nti) {
  wavpack_meta_t meta;

  nti.m_private_data   = (unsigned char *)t->private_data;
  nti.m_private_size   = t->private_size;

  meta.bits_per_sample = t->a_bps;
  meta.channel_count   = t->a_channels;
  meta.sample_rate     = (uint32_t)t->a_sfreq;
  meta.has_correction  = t->max_blockadd_id != 0;

  if (0.0 < t->v_frate)
    meta.samples_per_block = (uint32_t)(t->a_sfreq / t->v_frate);

  set_track_packetizer(t, new wavpack_packetizer_c(this, nti, meta));
  nti.m_private_data = NULL;
  nti.m_private_size = 0;

  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_audio_packetizer(kax_track_t *t,
                                      track_info_c &nti) {
  if ((0x0001 == t->a_formattag) || (0x0003 == t->a_formattag))
    create_pcm_audio_packetizer(t, nti);

  else if (0x0055 == t->a_formattag)
    create_mp3_audio_packetizer(t, nti);

  else if (0x2000 == t->a_formattag)
    create_ac3_audio_packetizer(t, nti);

  else if (0x2001 == t->a_formattag)
    create_dts_audio_packetizer(t, nti);

  else if (0xFFFE == t->a_formattag)
    create_vorbis_audio_packetizer(t, nti);

  else if ((FOURCC('M', 'P', '4', 'A') == t->a_formattag) || (0x00ff == t->a_formattag))
    create_aac_audio_packetizer(t, nti);

 #if defined(HAVE_FLAC_FORMAT_H)
  else if ((FOURCC('f', 'L', 'a', 'C') == t->a_formattag) || (0xf1ac == t->a_formattag))
    create_flac_audio_packetizer(t, nti);
 #endif

  else if (FOURCC('T', 'T', 'A', '1') == t->a_formattag)
    create_tta_audio_packetizer(t, nti);

  else if (FOURCC('W', 'V', 'P', '4') == t->a_formattag)
    create_wavpack_audio_packetizer(t, nti);

  else
    init_passthrough_packetizer(t);

  if (0.0 != t->a_osfreq)
    PTZR(t->ptzr)->set_audio_output_sampling_freq(t->a_osfreq);
}

void
kax_reader_c::create_subtitle_packetizer(kax_track_t *t,
                                         track_info_c &nti) {
  if (t->codec_id == MKV_S_VOBSUB) {
    set_track_packetizer(t, new vobsub_packetizer_c(this, t->private_data, t->private_size, nti));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

    t->sub_type = 'v';

  } else if (ba::starts_with(t->codec_id, "S_TEXT") || (t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) {
    std::string new_codec_id = ((t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) ? std::string("S_TEXT/") + std::string(&t->codec_id[2]) : t->codec_id;

    set_track_packetizer(t, new textsubs_packetizer_c(this, nti, new_codec_id.c_str(), t->private_data, t->private_size, false, true));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

    t->sub_type = 't';

  } else if (t->codec_id == MKV_S_KATE) {
    set_track_packetizer(t, new kate_packetizer_c(this, nti, t->private_data, t->private_size));
    show_packetizer_info(t->tnum, t->ptzr_ptr);
    t->sub_type = 'k';

  } else if (t->codec_id == MKV_S_HDMV_PGS) {
    set_track_packetizer(t, new pgs_packetizer_c(this, nti));
    show_packetizer_info(t->tnum, t->ptzr_ptr);
    t->sub_type = 'p';

  } else
    init_passthrough_packetizer(t);

}

void
kax_reader_c::create_button_packetizer(kax_track_t *t,
                                       track_info_c &nti) {
  if (t->codec_id == MKV_B_VOBBTN) {
    safefree(nti.m_private_data);
    nti.m_private_data = NULL;
    nti.m_private_size = 0;
    t->sub_type        = 'b';

    set_track_packetizer(t, new vobbtn_packetizer_c(this, nti, t->v_width, t->v_height));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

  } else
    init_passthrough_packetizer(t);
}

void
kax_reader_c::create_packetizer(int64_t tid) {
  kax_track_t *t = find_track_by_num(tid);

  if ((NULL == t) || (-1 != t->ptzr) || !t->ok || !demuxing_requested(t->type, t->tnum))
    return;

  track_info_c nti(m_ti);
  nti.m_private_data = (unsigned char *)safememdup(t->private_data, t->private_size);
  nti.m_private_size = t->private_size;
  nti.m_id           = t->tnum; // ID for this track.

  if (nti.m_language == "")
    nti.m_language   = t->language;
  if (nti.m_track_name == "")
    nti.m_track_name = t->track_name;
  if ((NULL != t->tags) && demuxing_requested('T', t->tnum))
    nti.m_tags       = dynamic_cast<KaxTags *>(t->tags->Clone());

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
      mxerror_tid(m_ti.m_fname, t->tnum, Y("Unsupported track type for this track.\n"));
      break;
  }

  set_packetizer_headers(t);
  m_ptzr_to_track_map[ m_reader_packetizers[t->ptzr] ] = t;
}

void
kax_reader_c::create_packetizers() {
  foreach(kax_track_cptr &track, m_tracks)
    create_packetizer(track->tnum);

  if (!g_segment_title_set) {
    g_segment_title     = m_title;
    g_segment_title_set = true;
  }
}

mpeg4::p10::avc_es_parser_cptr
kax_reader_c::parse_first_mpeg4_p10_frame(kax_track_t *t,
                                          track_info_c &nti) {
  try {
    read_first_frames(t, 5);

    avc_es_parser_cptr parser(new avc_es_parser_c);

    parser->ignore_nalu_size_length_errors();
    parser->discard_actual_frames();
    if (map_has_key(m_ti.m_nalu_size_lengths, t->tnum))
      parser->set_nalu_size_length(m_ti.m_nalu_size_lengths[t->tnum]);
    else if (map_has_key(m_ti.m_nalu_size_lengths, -1))
      parser->set_nalu_size_length(m_ti.m_nalu_size_lengths[-1]);

    if (sizeof(alBITMAPINFOHEADER) < t->private_size)
      parser->add_bytes((unsigned char *)t->private_data + sizeof(alBITMAPINFOHEADER), t->private_size - sizeof(alBITMAPINFOHEADER));
    foreach(memory_cptr &frame, t->first_frames_data)
      parser->add_bytes(frame->get_buffer(), frame->get_size());
    parser->flush();

    if (!parser->headers_parsed())
      throw false;

    return parser;
  } catch (...) {
    mxerror_tid(m_ti.m_fname, t->tnum, Y("Could not extract the decoder specific config data (AVCC) from this AVC/h.264 track.\n"));
  }

  return avc_es_parser_cptr(NULL);
}

void
kax_reader_c::create_mpeg4_p10_es_video_packetizer(kax_track_t *t,
                                                   track_info_c &nti) {
  avc_es_parser_cptr parser = parse_first_mpeg4_p10_frame(t, nti);

  mpeg4_p10_es_video_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, nti, parser->get_avcc(), t->v_width, t->v_height);
  set_track_packetizer(t, ptzr);

  ptzr->enable_timecode_generation(false);
  if (t->default_duration)
    ptzr->set_track_default_duration(t->default_duration);

  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_mpeg4_p10_video_packetizer(kax_track_t *t,
                                                track_info_c &nti) {
  if ((0 == nti.m_private_size) || (NULL == nti.m_private_data)) {
    // avc_es_parser_cptr parser = parse_first_mpeg4_p10_frame(t, nti);
    create_mpeg4_p10_es_video_packetizer(t, nti);
    return;
  }

  set_track_packetizer(t, new mpeg4_p10_video_packetizer_c(this, nti, t->v_frate, t->v_width, t->v_height));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_vc1_video_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new vc1_video_packetizer_c(this, nti));
  show_packetizer_info(t->tnum, t->ptzr_ptr);

  if ((NULL != t->private_data) && (sizeof(alBITMAPINFOHEADER) < t->private_size))
    t->ptzr_ptr->process(new packet_t(new memory_c(reinterpret_cast<unsigned char *>(t->private_data) + sizeof(alBITMAPINFOHEADER), t->private_size - sizeof(alBITMAPINFOHEADER), false)));
}

void
kax_reader_c::read_first_frames(kax_track_t *t,
                                unsigned num_wanted) {
  if (t->first_frames_data.size() >= num_wanted)
    return;

  std::map<int64_t, unsigned int> frames_by_track_id;

  m_in->save_pos();

  try {
    while (true) {
      KaxCluster *cluster = m_in_file->read_next_cluster();
      if (NULL == cluster)
        return;

      KaxClusterTimecode *ctc = static_cast<KaxClusterTimecode *> (cluster->FindFirstElt(EBML_INFO(KaxClusterTimecode), false));
      if (NULL != ctc)
        cluster->InitTimecode(uint64(*ctc), m_tc_scale);

      size_t bgidx;
      for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
        if ((EbmlId(*(*cluster)[bgidx]) == EBML_ID(KaxSimpleBlock))) {
          KaxSimpleBlock *block_simple = static_cast<KaxSimpleBlock *>((*cluster)[bgidx]);

          block_simple->SetParent(*cluster);
          kax_track_t *block_track = find_track_by_num(block_simple->TrackNum());

          if ((NULL == block_track) || (0 == block_simple->NumberFrames()))
            continue;

          for (size_t frame_idx = 0; block_simple->NumberFrames() > frame_idx; ++frame_idx) {
            frames_by_track_id[ block_simple->TrackNum() ]++;

            if (frames_by_track_id[ block_simple->TrackNum() ] <= block_track->first_frames_data.size())
              continue;

            DataBuffer &data_buffer = block_simple->GetBuffer(frame_idx);
            block_track->first_frames_data.push_back(memory_cptr(new memory_c(data_buffer.Buffer(), data_buffer.Size())));
            block_track->content_decoder.reverse(block_track->first_frames_data.back(), CONTENT_ENCODING_SCOPE_BLOCK);
            block_track->first_frames_data.back()->grab();
          }

        } else if ((EbmlId(*(*cluster)[bgidx]) == EBML_ID(KaxBlockGroup))) {
          KaxBlockGroup *block_group = static_cast<KaxBlockGroup *>((*cluster)[bgidx]);
          KaxBlock *block            = static_cast<KaxBlock *>(block_group->FindFirstElt(EBML_INFO(KaxBlock), false));

          if (NULL == block)
            continue;

          block->SetParent(*cluster);
          kax_track_t *block_track = find_track_by_num(block->TrackNum());

          if ((NULL == block_track) || (0 == block->NumberFrames()))
            continue;

          for (size_t frame_idx = 0; block->NumberFrames() > frame_idx; ++frame_idx) {
            frames_by_track_id[ block->TrackNum() ]++;

            if (frames_by_track_id[ block->TrackNum() ] <= block_track->first_frames_data.size())
              continue;

            DataBuffer &data_buffer = block->GetBuffer(frame_idx);
            block_track->first_frames_data.push_back(memory_cptr(new memory_c(data_buffer.Buffer(), data_buffer.Size())));
            block_track->content_decoder.reverse(block_track->first_frames_data.back(), CONTENT_ENCODING_SCOPE_BLOCK);
            block_track->first_frames_data.back()->grab();
          }
        }
      }

      delete cluster;

      if (t->first_frames_data.size() >= num_wanted)
        break;
    }
  } catch (...) {
  }

  m_in->restore_pos();
}

file_status_e
kax_reader_c::read(generic_packetizer_c *requested_ptzr,
                   bool force) {
  if (m_tracks.empty() || (FILE_STATUS_DONE == m_file_status))
    return FILE_STATUS_DONE;

  int64_t num_queued_bytes = get_queued_bytes();
  if (!force && (20 * 1024 * 1024 < num_queued_bytes)) {
    kax_track_t *requested_ptzr_track = m_ptzr_to_track_map[requested_ptzr];
    if (!requested_ptzr_track || (('a' != requested_ptzr_track->type) && ('v' != requested_ptzr_track->type)) || (512 * 1024 * 1024 < num_queued_bytes))
      return FILE_STATUS_HOLDING;
  }

  try {
    KaxCluster *cluster = m_in_file->read_next_cluster();
    if (NULL == cluster) {
      flush_packetizers();

      m_file_status = FILE_STATUS_DONE;
      return FILE_STATUS_DONE;
    }

    KaxClusterTimecode *ctc = static_cast<KaxClusterTimecode *>(cluster->FindFirstElt(EBML_INFO(KaxClusterTimecode), false));
    uint64_t cluster_tc = NULL != ctc ? uint64(*ctc) : 0;
    cluster->InitTimecode(cluster_tc, m_tc_scale);

    if (-1 == m_first_timecode) {
      m_first_timecode = cluster_tc * m_tc_scale;

      // If we're appending this file to another one then the core
      // needs the timecodes shifted to zero.
      if (m_appending && (NULL != m_chapters) && (0 < m_first_timecode))
        adjust_chapter_timecodes(*m_chapters, -m_first_timecode);
    }

    size_t bgidx;
    for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
      EbmlElement *element = (*cluster)[bgidx];

      if (EbmlId(*element) == EBML_ID(KaxSimpleBlock))
        process_simple_block(cluster, static_cast<KaxSimpleBlock *>(element));

      else if (EbmlId(*element) == EBML_ID(KaxBlockGroup))
        process_block_group(cluster, static_cast<KaxBlockGroup *>(element));
    }

    delete cluster;

  } catch (...) {
    mxwarn(Y("matroska_reader: caught exception\n"));
    flush_packetizers();
    return FILE_STATUS_DONE;
  }

  return FILE_STATUS_MOREDATA;
}

void
kax_reader_c::process_simple_block(KaxCluster *cluster,
                                   KaxSimpleBlock *block_simple) {
  int64_t block_duration = -1;
  int64_t block_bref     = VFT_IFRAME;
  int64_t block_fref     = VFT_NOBFRAME;

  block_simple->SetParent(*cluster);
  kax_track_t *block_track = find_track_by_num(block_simple->TrackNum());

  if (NULL == block_track) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                              "The block will be skipped.\n")) % format_timecode(block_simple->GlobalTimecode()) % block_simple->TrackNum());
    return;
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
    if (block_simple->IsDiscardable())
      block_fref = block_track->previous_timecode;
    else
      block_bref = block_track->previous_timecode;
  }

  m_last_timecode = block_simple->GlobalTimecode();

  // If we're appending this file to another one then the core
  // needs the timecodes shifted to zero.
  if (m_appending)
    m_last_timecode -= m_first_timecode;

  if ((-1 != block_track->ptzr) && block_track->passthrough) {
    // The handling for passthrough is a bit different. We don't have
    // any special cases, e.g. 0 terminating a string for the subs
    // and stuff. Just pass everything through as it is.
    int i;
    for (i = 0; i < (int)block_simple->NumberFrames(); i++) {
      DataBuffer &data_buffer = block_simple->GetBuffer(i);
      memory_cptr data(new memory_c(data_buffer.Buffer(), data_buffer.Size(), false));
      block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);
      packet_t *packet = new packet_t(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref);

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
          memory_cptr lines = memory_c::alloc(data->get_size() + 1);
          lines->get_buffer()[ data->get_size() ] = 0;
          memcpy(lines->get_buffer(), data->get_buffer(), data->get_size());

          PTZR(block_track->ptzr)->process(new packet_t(lines, m_last_timecode, block_duration, block_bref, block_fref));
        }

      } else {
        packet_cptr packet(new packet_t(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref));
        PTZR(block_track->ptzr)->process(packet);
      }
    }
  }

  block_track->previous_timecode  = (int64_t)m_last_timecode;
  block_track->units_processed   += block_simple->NumberFrames();
}

void
kax_reader_c::process_block_group(KaxCluster *cluster,
                                  KaxBlockGroup *block_group) {
  int64_t block_duration       = -1;
  int64_t block_bref           = VFT_IFRAME;
  int64_t block_fref           = VFT_NOBFRAME;
  bool bref_found              = false;
  bool fref_found              = false;
  KaxReferenceBlock *ref_block = static_cast<KaxReferenceBlock *>(block_group->FindFirstElt(EBML_INFO(KaxReferenceBlock), false));

  while (NULL != ref_block) {
    if (0 >= int64(*ref_block)) {
      block_bref = int64(*ref_block) * m_tc_scale;
      bref_found = true;
    } else {
      block_fref = int64(*ref_block) * m_tc_scale;
      fref_found = true;
    }

    ref_block = static_cast<KaxReferenceBlock *>(block_group->FindNextElt(*ref_block, false));
  }

  KaxBlock *block = static_cast<KaxBlock *>(block_group->FindFirstElt(EBML_INFO(KaxBlock), false));
  if (NULL == block) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block group was found at position %1%, but no block element was found inside it. This might make mkvmerge crash.\n"))
              % block_group->GetElementPosition());
    return;
  }

  block->SetParent(*cluster);
  kax_track_t *block_track = find_track_by_num(block->TrackNum());

  if (NULL == block_track) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                              "The block will be skipped.\n")) % format_timecode(block->GlobalTimecode()) % block->TrackNum());
    return;
  }

  KaxBlockAdditions *blockadd = static_cast<KaxBlockAdditions *>(block_group->FindFirstElt(EBML_INFO(KaxBlockAdditions), false));
  KaxBlockDuration *duration  = static_cast<KaxBlockDuration *>(block_group->FindFirstElt(EBML_INFO(KaxBlockDuration), false));

  if (NULL != duration)
    block_duration = (int64_t)uint64(*duration) * m_tc_scale / block->NumberFrames();
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

  m_last_timecode = block->GlobalTimecode();
  // If we're appending this file to another one then the core
  // needs the timecodes shifted to zero.
  if (m_appending)
    m_last_timecode -= m_first_timecode;

  KaxCodecState *codec_state = static_cast<KaxCodecState *>(block_group->FindFirstElt(EBML_INFO(KaxCodecState)));

  if ((-1 != block_track->ptzr) && block_track->passthrough) {
    // The handling for passthrough is a bit different. We don't have
    // any special cases, e.g. 0 terminating a string for the subs
    // and stuff. Just pass everything through as it is.
    if (bref_found)
      block_bref += m_last_timecode;
    if (fref_found)
      block_fref += m_last_timecode;

    int i;
    for (i = 0; i < (int)block->NumberFrames(); i++) {
      DataBuffer &data_buffer = block->GetBuffer(i);
      memory_cptr data(new memory_c(data_buffer.Buffer(), data_buffer.Size(), false));
      block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);
      packet_t *packet           = new packet_t(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref);
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
        block_bref += (int64_t)m_last_timecode;
    }
    if (fref_found)
      block_fref += (int64_t)m_last_timecode;

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

          packet_t *packet = new packet_t(new memory_c((unsigned char *)lines, 0, true), m_last_timecode, block_duration, block_bref, block_fref);
          if (NULL != codec_state)
            packet->codec_state = clone_memory(codec_state->GetBuffer(), codec_state->GetSize());

          PTZR(block_track->ptzr)->process(packet);
        }

      } else {
        packet_cptr packet(new packet_t(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref));

        if ((NULL != duration) && (0 == uint64(*duration)))
          packet->duration_mandatory = true;

        if (NULL != codec_state)
          packet->codec_state = clone_memory(codec_state->GetBuffer(), codec_state->GetSize());

        if (blockadd) {
          size_t k;
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

    block_track->previous_timecode  = (int64_t)m_last_timecode;
    block_track->units_processed   += block->NumberFrames();
  }
}

int
kax_reader_c::get_progress() {
  if (0 != m_segment_duration)
    return (m_last_timecode - std::max(m_first_timecode, static_cast<int64_t>(0))) * 100 / m_segment_duration;

  return 100 * m_in->getFilePointer() / m_file_size;
}

void
kax_reader_c::set_headers() {
  generic_reader_c::set_headers();

  foreach(kax_track_cptr &track, m_tracks)
    if ((-1 != track->ptzr) && track->passthrough)
      PTZR(track->ptzr)->get_track_entry()->EnableLacing(track->lacing_flag);
}

void
kax_reader_c::identify() {
  std::vector<std::string> verbose_info;

  if (!m_title.empty())
    verbose_info.push_back((boost::format("title:%1%") % escape(m_title)).str());
  if (0 != m_segment_duration)
    verbose_info.push_back((boost::format("duration:%1%") % m_segment_duration).str());

  id_result_container(verbose_info);

  foreach(kax_track_cptr &track, m_tracks) {
    if (!track->ok)
      continue;

    verbose_info.clear();

    if (track->language != "")
      verbose_info.push_back((boost::format("language:%1%") % escape(track->language)).str());

    if (track->track_name != "")
      verbose_info.push_back((boost::format("track_name:%1%") % escape(track->track_name)).str());

    if ((0 != track->v_dwidth) && (0 != track->v_dheight))
      verbose_info.push_back((boost::format("display_dimensions:%1%x%2%") % track->v_dwidth % track->v_dheight).str());

    if (stereo_mode_c::unspecified != track->v_stereo_mode)
      verbose_info.push_back((boost::format("stereo_mode:%1%") % static_cast<int>(track->v_stereo_mode)).str());

    if ((0 != track->v_pcleft) || (0 != track->v_pctop) || (0 != track->v_pcright) || (0 != track->v_pcbottom))
      verbose_info.push_back((boost::format("cropping:%1%,%2%,%3%,%4%") % track->v_pcleft % track->v_pctop % track->v_pcright % track->v_pcbottom).str());

    verbose_info.push_back((boost::format("default_track:%1%") % (track->default_track ? 1 : 0)).str());
    verbose_info.push_back((boost::format("forced_track:%1%")  % (track->forced_track  ? 1 : 0)).str());

    if ((track->codec_id == MKV_V_MSCOMP) && mpeg4::p10::is_avc_fourcc(track->v_fourcc))
      verbose_info.push_back("packetizer:mpeg4_p10_es_video");
    else if (track->codec_id == MKV_V_MPEG4_AVC)
      verbose_info.push_back("packetizer:mpeg4_p10_video");

    if (0 != track->default_duration)
      verbose_info.push_back((boost::format("default_duration:%1%") % track->default_duration).str());

    if ('a' == track->type) {
      if (0.0 != track->a_sfreq)
        verbose_info.push_back((boost::format("audio_sampling_frequency:%1%") % track->a_sfreq).str());
      if (0 != track->a_channels)
        verbose_info.push_back((boost::format("audio_channels:%1%") % track->a_channels).str());
    }

    std::string info = track->codec_id;

    if (track->ms_compat)
      info += std::string(", ") +
        (  track->type        == 'v'    ? std::string(track->v_fourcc)
         : track->a_formattag == 0x0001 ? std::string("PCM")
         : track->a_formattag == 0x0003 ? std::string("PCM")
         : track->a_formattag == 0x0055 ? std::string("MP3")
         : track->a_formattag == 0x2000 ? std::string("AC3")
         :                                (boost::format(Y("unknown, format tag 0x%|1$04x|")) % track->a_formattag).str());

    id_result_track(track->tnum,
                      track->type == 'v' ? ID_RESULT_TRACK_VIDEO
                    : track->type == 'a' ? ID_RESULT_TRACK_AUDIO
                    : track->type == 'b' ? ID_RESULT_TRACK_BUTTONS
                    : track->type == 's' ? ID_RESULT_TRACK_SUBTITLES
                    :                      Y("unknown"),
                    info, verbose_info);
  }

  foreach(attachment_t &attachment, g_attachments)
    id_result_attachment(attachment.ui_id, attachment.mime_type, attachment.data->get_size(), attachment.name, attachment.description);

  if (NULL != m_chapters)
    id_result_chapters(count_chapter_atoms(*m_chapters));

  if (m_tags.is_set())
    id_result_tags(ID_RESULT_GLOBAL_TAGS_ID, count_simple_tags(*m_tags));

  foreach(kax_track_cptr &track, m_tracks)
    if (track->ok && (NULL != track->tags))
      id_result_tags(track->tnum, count_simple_tags(*track->tags));
}

void
kax_reader_c::add_available_track_ids() {
  foreach(kax_track_cptr &track, m_tracks)
    add_available_track_id(track->tnum);
}

void
kax_reader_c::set_track_packetizer(kax_track_t *t,
                                   generic_packetizer_c *ptzr) {
  t->ptzr     = add_packetizer(ptzr);
  t->ptzr_ptr = PTZR(t->ptzr);
}

