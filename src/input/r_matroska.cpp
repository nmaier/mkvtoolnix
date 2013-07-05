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

#include "common/alac.h"
#include "common/at_scope_exit.h"
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
#include "common/strings/utf8.h"
#include "common/tags/tags.h"
#include "input/r_matroska.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_alac.h"
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
#include "output/p_opus.h"
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
  if ((0 != v_dwidth) && (0 != v_dheight))
    ptzr_ptr->set_video_display_dimensions(v_dwidth, v_dheight, OPTION_SOURCE_CONTAINER);
}

void
kax_track_t::handle_packetizer_pixel_cropping() {
  if ((0 < v_pcleft) || (0 < v_pctop) || (0 < v_pcright) || (0 < v_pcbottom))
    ptzr_ptr->set_video_pixel_cropping(v_pcleft, v_pctop, v_pcright, v_pcbottom, OPTION_SOURCE_CONTAINER);
}

void
kax_track_t::handle_packetizer_stereo_mode() {
  if (stereo_mode_c::unspecified != v_stereo_mode)
    ptzr_ptr->set_video_stereo_mode(v_stereo_mode, OPTION_SOURCE_CONTAINER);
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
  if ((0 != v_display_unit) || ((8 * v_dwidth) > v_width) || ((8 * v_dheight) > v_height))
    return;

  if (boost::math::gcd(v_dwidth, v_dheight) != 1)
    return;

  // max shrinking was applied, ie x264 style
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

/*
   Probes a file by simply comparing the first four bytes to the EBML
   head signature.
*/
int
kax_reader_c::probe_file(mm_io_c *in,
                         uint64_t size) {
  unsigned char data[4];

  if (4 > size)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);
    if (in->read(data, 4) != 4)
      return 0;
    in->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (get_uint32_be(data) != MAGIC_MKV)
    return 0;
  return 1;
}

kax_reader_c::kax_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_segment_duration(0)
  , m_last_timecode(0)
  , m_first_timecode(-1)
  , m_writing_app_ver(-1)
  , m_attachment_id(0)
  , m_file_status(FILE_STATUS_MOREDATA)
  , m_opus_experimental_warning_shown{}
{
  init_l1_position_storage(m_deferred_l1_positions);
  init_l1_position_storage(m_handled_l1_positions);
}

kax_reader_c::~kax_reader_c() {
}

void
kax_reader_c::init_l1_position_storage(deferred_positions_t &storage) {
  storage[dl1t_attachments] = std::vector<int64_t>();
  storage[dl1t_chapters]    = std::vector<int64_t>();
  storage[dl1t_tags]        = std::vector<int64_t>();
  storage[dl1t_tracks]      = std::vector<int64_t>();
  storage[dl1t_seek_head]   = std::vector<int64_t>();
}

bool
kax_reader_c::packets_available() {
  for (auto &track : m_tracks)
    if ((-1 != track->ptzr) && !PTZR(track->ptzr)->packet_available())
      return false;

  return !m_tracks.empty();
}

kax_track_t *
kax_reader_c::find_track_by_num(uint64_t n,
                                kax_track_t *c) {
  auto itr = brng::find_if(m_tracks, [&](kax_track_cptr &track) { return (track->track_number == n) && (track.get() != c); });
  return itr == m_tracks.end() ? nullptr : itr->get();
}

kax_track_t *
kax_reader_c::find_track_by_uid(uint64_t uid,
                                kax_track_t *c) {
  auto itr = brng::find_if(m_tracks, [&](kax_track_cptr &track) { return (track->track_uid == uid) && (track.get() != c); });
  return itr == m_tracks.end() ? nullptr : itr->get();
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
  if (!t->private_data || (sizeof(alWAVEFORMATEX) > t->private_size)) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no WAVEFORMATEX struct present. "
                             "Therefore we don't have a format ID to identify the audio codec used.\n")) % t->tnum % MKV_A_ACM);
    return false;

  }

  alWAVEFORMATEX *wfe = reinterpret_cast<alWAVEFORMATEX *>(t->private_data);
  t->a_formattag      = get_uint16_le(&wfe->w_format_tag);

  if ((0xfffe == t->a_formattag) && (!unlace_vorbis_private_data(t, static_cast<unsigned char *>(t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX)))) {
    // Force the passthrough packetizer to be used if the data behind
    // the WAVEFORMATEX does not contain valid laced Vorbis headers.
    t->a_formattag = 0;
    return true;
  }

  t->ms_compat = 1;
  uint32_t u   = get_uint32_le(&wfe->n_samples_per_sec);

  if (static_cast<uint32_t>(t->a_sfreq) != u) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: (MS compatibility mode for track %1%) Matroska says that there are %2% samples per second, "
                             "but WAVEFORMATEX says that there are %3%.\n")) % t->tnum % static_cast<int>(t->a_sfreq) % u);
    if (0.0 == t->a_sfreq)
      t->a_sfreq = static_cast<double>(u);
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
kax_reader_c::verify_alac_audio_track(kax_track_t *t) {
  t->a_formattag = FOURCC('A', 'L', 'A', 'C');

  if (t->private_data && (sizeof(alac::codec_config_t) <= t->private_size))
    return true;

  if (verbose)
    mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but the private codec data does not contain valid headers.\n")) % t->tnum % MKV_A_VORBIS);

  return false;
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
  if (!t->private_data || !unlace_vorbis_private_data(t, static_cast<unsigned char *>(t->private_data), t->private_size)) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but the private codec data does not contain valid headers.\n")) % t->tnum % MKV_A_VORBIS);
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

bool
kax_reader_c::verify_opus_audio_track(kax_track_t *t) {
  if (!t->private_data || !t->private_size) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but the private codec data does not contain valid headers.\n")) % t->tnum % MKV_A_OPUS);
    return false;
  }

  t->a_formattag = FOURCC('O', 'p', 'u', 's');

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
  else if (balg::starts_with(t->codec_id, MKV_A_AC3) || (t->codec_id == MKV_A_EAC3))
    t->a_formattag = 0x2000;
  else if (t->codec_id == MKV_A_ALAC)
    is_ok = verify_alac_audio_track(t);
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
  else if (balg::starts_with(t->codec_id, "A_REAL/"))
    t->a_formattag = FOURCC('r', 'e', 'a', 'l');
  else if (t->codec_id == MKV_A_FLAC)
    is_ok = verify_flac_audio_track(t);
  else if (t->codec_id == MKV_A_TTA)
    t->a_formattag = FOURCC('T', 'T', 'A', '1');
  else if (t->codec_id == MKV_A_WAVPACK4)
    t->a_formattag = FOURCC('W', 'V', 'P', '4');
  else if (balg::starts_with(t->codec_id, MKV_A_OPUS))
    is_ok = verify_opus_audio_track(t);

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
  if (!t->private_data || (sizeof(alBITMAPINFOHEADER) > t->private_size)) {
    if (verbose)
      mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no BITMAPINFOHEADER struct present. "
                             "Therefore we don't have a FourCC to identify the video codec used.\n"))
             % t->tnum % MKV_V_MSCOMP);
      return false;
  }

  t->ms_compat            = 1;
  alBITMAPINFOHEADER *bih = reinterpret_cast<alBITMAPINFOHEADER *>(t->private_data);
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
  if (t->private_data)
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
  if (t->private_data)
    return true;

  if (verbose)
    mxwarn(boost::format(Y("matroska_reader: The CodecID for track %1% is '%2%', but there was no private data found.\n")) % t->tnum % t->codec_id);

  return false;
}

bool
kax_reader_c::verify_vobsub_subtitle_track(kax_track_t *t) {
  if (t->private_data)
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
    t = m_tracks[tnum].get();

    t->ok = t->content_decoder.is_ok();

    if (!t->ok)
      continue;
    t->ok = 0;

    if (t->private_data) {
      memory_cptr private_data(new memory_c(t->private_data, t->private_size, true));
      t->content_decoder.reverse(private_data, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
      private_data->lock();
      t->private_data = private_data->get_buffer();
      t->private_size = private_data->get_size();
    }

    switch (t->type) {
      case 'v': verify_video_track(t);    break;
      case 'a': verify_audio_track(t);    break;
      case 's': verify_subtitle_track(t); break;
      case 'b': verify_button_track(t);   break;

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
  for (auto test_position : m_handled_l1_positions[type])
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
  at_scope_exit_c restore([&]() { io->restore_pos(); });

  int upper_lvl_el = 0;
  std::shared_ptr<EbmlElement> l1(m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true));
  KaxAttachments *atts = dynamic_cast<KaxAttachments *>(l1.get());

  if (!atts)
    return;

  EbmlElement *element_found = nullptr;
  upper_lvl_el               = 0;

  atts->Read(*m_es, EBML_CLASS_CONTEXT(KaxAttachments), upper_lvl_el, element_found, true);

  for (auto l1_att : *atts) {
    auto att = dynamic_cast<KaxAttached *>(l1_att);
    if (!att)
      continue;

    attachment_t matt;
    matt.name        = to_utf8(FindChildValue<KaxFileName>(att));
    matt.description = to_utf8(FindChildValue<KaxFileDescription>(att));
    matt.mime_type   = FindChildValue<KaxMimeType>(att);
    matt.id          = FindChildValue<KaxFileUID>(att);

    auto fdata       = FindChild<KaxFileData>(att);
    if (fdata)
      matt.data = memory_c::clone(static_cast<unsigned char *>(fdata->GetBuffer()), fdata->GetSize());

    ++m_attachment_id;
    attach_mode_e attach_mode;
    if (   !matt.id
        || !matt.data->get_size()
        || matt.mime_type.empty()
        || matt.name.empty()
        || ((attach_mode = attachment_requested(m_attachment_id)) == ATTACH_MODE_SKIP))
      continue;

    matt.ui_id          = m_attachment_id;
    matt.to_all_files   = ATTACH_MODE_TO_ALL_FILES == attach_mode;

    add_attachment(matt);
  }
}

void
kax_reader_c::handle_chapters(mm_io_c *io,
                              EbmlElement *l0,
                              int64_t pos) {
  if (has_deferred_element_been_processed(dl1t_chapters, pos))
    return;

  io->save_pos(pos);
  at_scope_exit_c restore([&]() { io->restore_pos(); });

  int upper_lvl_el = 0;
  std::shared_ptr<EbmlElement> l1(m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true));
  KaxChapters *tmp_chapters = dynamic_cast<KaxChapters *>(l1.get());

  if (!tmp_chapters)
    return;

  EbmlElement *l2 = nullptr;
  upper_lvl_el    = 0;

  tmp_chapters->Read(*m_es, EBML_CLASS_CONTEXT(KaxChapters), upper_lvl_el, l2, true);

  if (!m_chapters)
    m_chapters = kax_chapters_cptr{new KaxChapters};

  m_chapters->GetElementList().insert(m_chapters->begin(), tmp_chapters->begin(), tmp_chapters->end());
  tmp_chapters->RemoveAll();
}

void
kax_reader_c::handle_tags(mm_io_c *io,
                          EbmlElement *l0,
                          int64_t pos) {
  if (has_deferred_element_been_processed(dl1t_tags, pos))
    return;

  io->save_pos(pos);
  at_scope_exit_c restore([&]() { io->restore_pos(); });

  int upper_lvl_el = 0;
  std::shared_ptr<EbmlElement> l1(m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true));
  KaxTags *tags = dynamic_cast<KaxTags *>(l1.get());

  if (!tags)
    return;

  EbmlElement *l2 = nullptr;
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
    KaxTagTargets *target = FindChild<KaxTagTargets>(tag);

    if (target) {
      KaxTagTrackUID *tuid = FindChild<KaxTagTrackUID>(target);

      if (tuid) {
        is_global          = false;
        kax_track_t *track = find_track_by_uid(tuid->GetValue());

        if (track) {
          bool contains_tag = false;

          size_t i;
          for (i = 0; i < tag->ListSize(); i++)
            if (dynamic_cast<KaxTagSimple *>((*tag)[i])) {
              contains_tag = true;
              break;
            }

          if (contains_tag) {
            if (!track->tags)
              track->tags = new KaxTags;
            track->tags->PushElement(*tag);

            delete_tag = false;
          }
        }
      }
    }

    if (is_global) {
      if (!m_tags)
        m_tags = std::shared_ptr<KaxTags>(new KaxTags);
      m_tags->PushElement(*tag);

    } else if (delete_tag)
      delete tag;

    tags->Remove(0);
  }
}

void
kax_reader_c::read_headers_info(mm_io_c *io,
                                EbmlElement *l0,
                                int64_t pos) {
  // General info about this Matroska file
  if (has_deferred_element_been_processed(dl1t_info, pos))
    return;

  io->save_pos(pos);
  at_scope_exit_c restore([&]() { io->restore_pos(); });

  int upper_lvl_el = 0;
  std::shared_ptr<EbmlElement> l1(m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true));
  KaxInfo *info = dynamic_cast<KaxInfo *>(l1.get());

  if (!info)
    return;

  EbmlElement *l2 = nullptr;
  upper_lvl_el    = 0;

  info->Read(*m_es, EBML_CLASS_CONTEXT(KaxInfo), upper_lvl_el, l2, true);

  m_tc_scale         = FindChildValue<KaxTimecodeScale, uint64_t>(info, 1000000);
  m_segment_duration = irnd(FindChildValue<KaxDuration>(info) * m_tc_scale);
  m_title            = to_utf8(FindChildValue<KaxTitle>(info));

  m_in_file->set_timecode_scale(m_tc_scale);

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
  auto km_writing_app = FindChild<KaxWritingApp>(info);
  if (km_writing_app)
    read_headers_info_writing_app(km_writing_app);

  auto km_muxing_app = FindChild<KaxMuxingApp>(info);
  if (km_muxing_app) {
    m_muxing_app = km_muxing_app->GetValueUTF8();

    // DirectShow Muxer workaround: Gabest's DirectShow muxer
    // writes wrong references (off by 1ms). So let the cluster
    // helper be a bit more imprecise in what it accepts when
    // looking for referenced packets.
    if (m_muxing_app == "DirectShow Matroska Muxer")
      m_reference_timecode_tolerance = 1000000;
  }

  m_segment_uid          = FindChildValue<KaxSegmentUID>(info);
  m_next_segment_uid     = FindChildValue<KaxNextUID>(info);
  m_previous_segment_uid = FindChildValue<KaxPrevUID>(info);
}

void
kax_reader_c::read_headers_info_writing_app(KaxWritingApp *&km_writing_app) {
  size_t idx;

  std::string s = km_writing_app->GetValueUTF8();
  strip(s);

  if (balg::istarts_with(s, "avi-mux gui"))
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

      if (!parse_number(ver_parts[idx], num) || (0 > num) || (255 < num)) {
        failed = true;
        break;
      }
      m_writing_app_ver <<= 8;
      m_writing_app_ver  |= num;
    }
    if (failed)
      m_writing_app_ver = -1;
  }
}

void
kax_reader_c::read_headers_track_audio(kax_track_t *track,
                                       KaxTrackAudio *ktaudio) {
  track->a_sfreq    = FindChildValue<KaxAudioSamplingFreq>(ktaudio, 8000.0);
  track->a_osfreq   = FindChildValue<KaxAudioOutputSamplingFreq>(ktaudio);
  track->a_channels = FindChildValue<KaxAudioChannels>(ktaudio, 1);
  track->a_bps      = FindChildValue<KaxAudioBitDepth>(ktaudio);
}

void
kax_reader_c::read_headers_track_video(kax_track_t *track,
                                       KaxTrackVideo *ktvideo) {
  track->v_width        = FindChildValue<KaxVideoPixelWidth>(ktvideo);
  track->v_height       = FindChildValue<KaxVideoPixelHeight>(ktvideo);
  track->v_dwidth       = FindChildValue<KaxVideoDisplayWidth>(ktvideo,  track->v_width);
  track->v_dheight      = FindChildValue<KaxVideoDisplayHeight>(ktvideo, track->v_height);

  track->v_pcleft       = FindChildValue<KaxVideoPixelCropLeft>(ktvideo);
  track->v_pcright      = FindChildValue<KaxVideoPixelCropRight>(ktvideo);
  track->v_pctop        = FindChildValue<KaxVideoPixelCropTop>(ktvideo);
  track->v_pcbottom     = FindChildValue<KaxVideoPixelCropBottom>(ktvideo);

  track->v_stereo_mode  = FindChildValue<KaxVideoStereoMode, stereo_mode_c::mode>(ktvideo, stereo_mode_c::unspecified);

  // For older files.
  track->v_frate        = FindChildValue<KaxVideoFrameRate>(ktvideo, track->v_frate);

#if MATROSKA_VERSION >= 2
  track->v_display_unit = FindChildValue<KaxVideoDisplayUnit>(ktvideo);
#endif

  if (!track->v_width)
    mxerror(Y("matroska_reader: Pixel width is missing.\n"));
  if (!track->v_height)
    mxerror(Y("matroska_reader: Pixel height is missing.\n"));

  track->fix_display_dimension_parameters();
}

void
kax_reader_c::read_headers_tracks(mm_io_c *io,
                                  EbmlElement *l0,
                                  int64_t position) {
  // Yep, we've found our KaxTracks element. Now find all m_tracks
  // contained in this segment.
  if (has_deferred_element_been_processed(dl1t_tracks, position))
    return;

  int upper_lvl_el = 0;
  io->save_pos(position);
  EbmlElement *l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

  if (!l1 || !is_id(l1, KaxTracks)) {
    delete l1;
    io->restore_pos();

    return;
  }

  EbmlElement *l2 = nullptr;
  upper_lvl_el    = 0;
  l1->Read(*m_es, EBML_CLASS_CONTEXT(KaxTracks), upper_lvl_el, l2, true);

  KaxTrackEntry *ktentry = FindChild<KaxTrackEntry>(l1);
  while (ktentry) {
    // We actually found a track entry :) We're happy now.
    auto track = std::make_shared<kax_track_t>();
    track->tnum = m_tracks.size();

    auto ktnum = FindChild<KaxTrackNumber>(ktentry);
    if (!ktnum)
      mxerror(Y("matroska_reader: A track is missing its track number.\n"));

    track->track_number = ktnum->GetValue();
    if (find_track_by_num(track->track_number, track.get())) {
      ktentry = FindNextChild<KaxTrackEntry>(l1, ktentry);
      continue;
    }

    auto ktuid = FindChild<KaxTrackUID>(ktentry);
    if (!ktuid)
      mxerror(Y("matroska_reader: A track is missing its track UID.\n"));
    track->track_uid = ktuid->GetValue();

    auto kttype = FindChild<KaxTrackType>(ktentry);
    if (!kttype)
      mxerror(Y("matroska_reader: Track type was not found.\n"));
    unsigned char track_type = kttype->GetValue();
    track->type              = track_type == track_audio    ? 'a'
                             : track_type == track_video    ? 'v'
                             : track_type == track_subtitle ? 's'
                             :                                '?';

    auto ktaudio = FindChild<KaxTrackAudio>(ktentry);
    if (ktaudio)
      read_headers_track_audio(track.get(), ktaudio);

    auto ktvideo = FindChild<KaxTrackVideo>(ktentry);
    if (ktvideo)
      read_headers_track_video(track.get(), ktvideo);

    auto kcodecpriv = FindChild<KaxCodecPrivate>(ktentry);
    if (kcodecpriv) {
      track->private_size = kcodecpriv->GetSize();
      if (0 < track->private_size)
        track->private_data = safememdup(kcodecpriv->GetBuffer(), track->private_size);
    }

    track->codec_id         = FindChildValue<KaxCodecID>(ktentry);
    track->track_name       = to_utf8(FindChildValue<KaxTrackName>(ktentry));
    track->language         = FindChildValue<KaxTrackLanguage, std::string>(ktentry, "eng");
    track->default_duration = FindChildValue<KaxTrackDefaultDuration>(ktentry, track->default_duration);
    track->default_track    = FindChildValue<KaxTrackFlagDefault, bool>(ktentry, true);
    track->forced_track     = FindChildValue<KaxTrackFlagForced>(ktentry);
    track->enabled_track    = FindChildValue<KaxTrackFlagEnabled, bool>(ktentry, true);
    track->lacing_flag      = FindChildValue<KaxTrackFlagLacing>(ktentry);
    track->max_blockadd_id  = FindChildValue<KaxMaxBlockAdditionID>(ktentry);
    track->min_cache        = FindChildValue<KaxTrackMinCache>(ktentry);
    track->max_cache        = FindChildValue<KaxTrackMaxCache>(ktentry);
    track->v_bframes        = 1 < track->min_cache;

    auto kax_seek_pre_roll  = FindChild<KaxSeekPreRoll>(ktentry);
    auto kax_codec_delay    = FindChild<KaxCodecDelay>(ktentry);

    if (kax_seek_pre_roll)
      track->seek_pre_roll = timecode_c::ns(kax_seek_pre_roll->GetValue());
    if (kax_codec_delay)
      track->codec_delay   = timecode_c::ns(kax_codec_delay->GetValue());

    if (track->codec_id.empty())
      mxerror(Y("matroska_reader: The CodecID is missing.\n"));

    if (!track->language.empty()) {
      int index = map_to_iso639_2_code(track->language.c_str());
      if (-1 != index)
        track->language = iso639_languages[index].iso639_2_code;
      else
        track->language.clear();
    }

    if (0 != track->default_duration)
      track->v_frate = 1000000000.0 / track->default_duration;

    track->content_decoder.initialize(*ktentry);
    m_tracks.push_back(track);

    ktentry = FindNextChild<KaxTrackEntry>(l1, ktentry);
  } // while (ktentry)

  delete l1;
  io->restore_pos();
}

void
kax_reader_c::handle_seek_head(mm_io_c *io,
                               EbmlElement *l0,
                               int64_t pos) {
  if (has_deferred_element_been_processed(dl1t_seek_head, pos))
    return;

  std::vector<int64_t> next_seek_head_positions;
  at_scope_exit_c restore([&]() { io->restore_pos(); });

  try {
    io->save_pos(pos);

    int upper_lvl_el = 0;
    std::shared_ptr<EbmlElement> l1(m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true));
    auto *seek_head = dynamic_cast<KaxSeekHead *>(l1.get());

    if (!seek_head)
      return;

    EbmlElement *l2 = nullptr;
    upper_lvl_el    = 0;

    seek_head->Read(*m_es, EBML_CLASS_CONTEXT(KaxSeekHead), upper_lvl_el, l2, true);

    for (auto l2 : *seek_head) {
      if (EbmlId(*l2) != EBML_ID(KaxSeek))
        continue;

      KaxSeek &seek = *static_cast<KaxSeek *>(l2);
      int64_t pos   = FindChildValue<KaxSeekPosition, int64_t>(seek, -1);

      if (-1 == pos)
        continue;

      auto *k_id = FindChild<KaxSeekID>(seek);
      if (!k_id)
        continue;

      EbmlId id(k_id->GetBuffer(), k_id->GetSize());

      deferred_l1_type_e type = id == EBML_ID(KaxAttachments) ? dl1t_attachments
        :                       id == EBML_ID(KaxChapters)    ? dl1t_chapters
        :                       id == EBML_ID(KaxTags)        ? dl1t_tags
        :                       id == EBML_ID(KaxTracks)      ? dl1t_tracks
        :                       id == EBML_ID(KaxSeekHead)    ? dl1t_seek_head
        :                       id == EBML_ID(KaxInfo)        ? dl1t_info
        :                                                       dl1t_unknown;

      if (dl1t_unknown == type)
        continue;

      pos = static_cast<KaxSegment *>(l0)->GetGlobalPosition(pos);

      if (dl1t_seek_head == type)
        next_seek_head_positions.push_back(pos);
      else
        m_deferred_l1_positions[type].push_back(pos);
    }

  } catch (...) {
    return;
  }

  for (auto pos : next_seek_head_positions)
    handle_seek_head(io, l0, pos);
}

void
kax_reader_c::read_headers() {
  if (!read_headers_internal())
    throw mtx::input::header_parsing_x();
  show_demuxer_info();
}

bool
kax_reader_c::read_headers_internal() {
  // Elements for different levels

  KaxCluster *cluster = nullptr;
  try {
    m_es        = std::shared_ptr<EbmlStream>(new EbmlStream(*m_in));
    m_in_file   = kax_file_cptr(new kax_file_c(m_in));

    // Find the EbmlHead element. Must be the first one.
    EbmlElement *l0 = m_es->FindNextID(EBML_INFO(EbmlHead), 0xFFFFFFFFFFFFFFFFLL);
    if (!l0) {
      mxwarn(Y("matroska_reader: no EBML head found.\n"));
      return false;
    }

    // Don't verify its data for now.
    l0->SkipData(*m_es, EBML_CONTEXT(l0));
    delete l0;

    // Next element must be a segment
    l0 = m_es->FindNextID(EBML_INFO(KaxSegment), 0xFFFFFFFFFFFFFFFFLL);
    std::shared_ptr<EbmlElement> l0_cptr(l0);
    if (!l0) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return false;
    }
    if (!(EbmlId(*l0) == EBML_ID(KaxSegment))) {
      if (verbose)
        mxwarn(Y("matroska_reader: No segment found.\n"));
      return false;
    }

    // We've got our segment, so let's find the m_tracks
    int upper_lvl_el = 0;
    m_tc_scale         = TIMECODE_SCALE;
    EbmlElement *l1  = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true, 1);

    while (l1 && (0 >= upper_lvl_el)) {
      if (EbmlId(*l1) == EBML_ID(KaxInfo))
        m_deferred_l1_positions[dl1t_info].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxTracks))
        m_deferred_l1_positions[dl1t_tracks].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxAttachments))
        m_deferred_l1_positions[dl1t_attachments].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxChapters))
        m_deferred_l1_positions[dl1t_chapters].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxTags))
        m_deferred_l1_positions[dl1t_tags].push_back(l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxSeekHead))
        handle_seek_head(m_in.get(), l0, l1->GetElementPosition());

      else if (EbmlId(*l1) == EBML_ID(KaxCluster))
        cluster = static_cast<KaxCluster *>(l1);

      else
        l1->SkipData(*m_es, EBML_CONTEXT(l1));

      if (cluster)              // we've found the first cluster, so get out
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
        l1 = nullptr;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*m_es, EBML_CONTEXT(l1));
      delete l1;
      l1 = m_es->FindNextElement(EBML_CONTEXT(l0), upper_lvl_el, 0xFFFFFFFFL, true);

    } // while (l1)

    for (auto position : m_deferred_l1_positions[dl1t_info])
      read_headers_info(m_in.get(), l0, position);

    for (auto position : m_deferred_l1_positions[dl1t_tracks])
      read_headers_tracks(m_in.get(), l0, position);

    if (!m_ti.m_attach_mode_list.none()) {
      for (auto position : m_deferred_l1_positions[dl1t_attachments])
        handle_attachments(m_in.get(), l0, position);
    }

    if (!m_ti.m_no_chapters) {
      for (auto position : m_deferred_l1_positions[dl1t_chapters])
        handle_chapters(m_in.get(), l0, position);
    }

    for (auto position : m_deferred_l1_positions[dl1t_tags])
      handle_tags(m_in.get(), l0, position);

    if (!m_ti.m_no_global_tags)
      process_global_tags();

  } catch (...) {
    mxerror(Y("matroska_reader: caught exception\n"));
  }

  verify_tracks();

  if (cluster) {
    m_in->setFilePointer(cluster->GetElementPosition(), seek_beginning);
    delete cluster;

  } else
    m_in->setFilePointer(0, seek_end);

  return true;
}

void
kax_reader_c::process_global_tags() {
  if (!m_tags || g_identifying)
    return;

  for (auto tag : *m_tags)
    add_tags(static_cast<KaxTag *>(tag));

  m_tags->RemoveAll();
}

void
kax_reader_c::init_passthrough_packetizer(kax_track_t *t) {
  passthrough_packetizer_c *ptzr;
  track_info_c nti(m_ti);

  mxinfo_tid(m_ti.m_fname, t->tnum, boost::format(Y("Using the generic output module for track type '%1%'.\n")) % MAP_TRACK_TYPE_STRING(t->type));

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
  ptzr->set_codec_private(memory_c::clone(t->private_data, t->private_size));

  if (0.0 < t->v_frate)
    ptzr->set_track_default_duration(1000000000.0 / t->v_frate);
  if (0 < t->min_cache)
    ptzr->set_track_min_cache(t->min_cache);
  if (0 < t->max_cache)
    ptzr->set_track_max_cache(t->max_cache);
  if (t->seek_pre_roll.valid())
    ptzr->set_track_seek_pre_roll(t->seek_pre_roll);
  if (t->codec_delay.valid())
    ptzr->set_codec_delay(t->codec_delay);

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

  if (boost::logic::indeterminate(PTZR(t->ptzr)->m_ti.m_enabled_track))
    PTZR(t->ptzr)->set_track_enabled_flag(t->enabled_track);

  if ((0 != t->track_uid) && !PTZR(t->ptzr)->set_uid(t->track_uid))
    mxwarn(boost::format(Y("matroska_reader: Could not keep the track UID %1% because it is already allocated for the new file.\n")) % t->track_uid);
}

void
kax_reader_c::create_video_packetizer(kax_track_t *t,
                                      track_info_c &nti) {
  if ((t->codec_id == MKV_V_MSCOMP) && mpeg4::p10::is_avc_fourcc(t->v_fourcc) && !hack_engaged(ENGAGE_ALLOW_AVC_IN_VFW_MODE))
    create_mpeg4_p10_es_video_packetizer(t, nti);

  else if (   balg::starts_with(t->codec_id, "V_MPEG4")
           || (t->codec_id == MKV_V_MSCOMP)
           || balg::starts_with(t->codec_id, "V_REAL")
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
    if (t->private_data && (2 <= t->private_size)) {
      int channels, sfreq, osfreq;
      bool sbr;

      if (!parse_aac_data(static_cast<const unsigned char *>(t->private_data), t->private_size, profile, channels, sfreq, osfreq, sbr))
        mxerror_tid(m_ti.m_fname, t->tnum, Y("Malformed AAC codec initialization data found.\n"));

      detected_profile = profile;
      id               = AAC_ID_MPEG4;
      if (sbr)
        profile        = AAC_PROFILE_SBR;

    } else if (!parse_aac_codec_id(t->codec_id, id, profile))
      mxerror_tid(m_ti.m_fname, t->tnum, boost::format(Y("Malformed codec id '%1%'.\n")) % t->codec_id);

  } else {
    int channels, sfreq, osfreq;
    bool sbr;

    if (!parse_aac_data(static_cast<const unsigned char *>(t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX), profile, channels, sfreq, osfreq, sbr))
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

  set_track_packetizer(t, new aac_packetizer_c(this, nti, id, profile, t->a_sfreq, t->a_channels, false, true));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_ac3_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  unsigned int bsid = t->codec_id == "A_AC3/BSID9"  ?  9
                    : t->codec_id == "A_AC3/BSID10" ? 10
                    : t->codec_id == MKV_A_EAC3     ? 16
                    :                                  0;

  set_track_packetizer(t, new ac3_packetizer_c(this, nti, t->a_sfreq, t->a_channels, bsid));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_alac_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new alac_packetizer_c(this, nti, memory_c::clone(t->private_data, t->private_size), t->a_sfreq, t->a_channels));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_dts_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  try {
    read_first_frames(t, 5);

    byte_buffer_c buffer;
    for (auto &frame : t->first_frames_data)
      buffer.add(frame);

    dts_header_t dtsheader;
    int position = find_dts_header(buffer.get_buffer(), buffer.get_size(), &dtsheader);

    if (-1 == position)
      throw false;

    set_track_packetizer(t, new dts_packetizer_c(this, nti, dtsheader));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

  } catch (...) {
    mxerror_tid(m_ti.m_fname, t->tnum, Y("Could not find valid DTS headers in this track's first frames.\n"));
  }
}

#if defined(HAVE_FLAC_FORMAT_H)
void
kax_reader_c::create_flac_audio_packetizer(kax_track_t *t,
                                           track_info_c &nti) {
  nti.m_private_data.reset();

  if (FOURCC('f', 'L', 'a', 'C') == t->a_formattag)
    set_track_packetizer(t, new flac_packetizer_c(this, nti, static_cast<unsigned char *>(t->private_data), t->private_size));
  else
    set_track_packetizer(t, new flac_packetizer_c(this, nti, static_cast<unsigned char *>(t->private_data) + sizeof(alWAVEFORMATEX), t->private_size - sizeof(alWAVEFORMATEX)));

  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

#endif  // HAVE_FLAC_FORMAT_H

void
kax_reader_c::create_mp3_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new mp3_packetizer_c(this, nti, t->a_sfreq, t->a_channels, true));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_opus_audio_packetizer(kax_track_t *t,
                                           track_info_c &nti) {
  set_track_packetizer(t, new opus_packetizer_c(this, nti));
  show_packetizer_info(t->tnum, t->ptzr_ptr);

  if (!m_opus_experimental_warning_shown && (t->codec_id == std::string{MKV_A_OPUS} + "/EXPERIMENTAL")) {
    mxwarn(boost::format(Y("'%1%': You're re-muxing an Opus track that was muxed in experimental mode. "
                           "The resulting track will be written in final mode, but one detail cannot be recovered from a track muxed in experimental mode: the end trimming. "
                           "This means that a decoder might output a few samples more than originally intended. "
                           "You should re-mux from the original Opus file if possible.\n"))
           % m_ti.m_fname);
    m_opus_experimental_warning_shown = true;
  }
}

void
kax_reader_c::create_pcm_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  set_track_packetizer(t, new pcm_packetizer_c(this, nti, t->a_sfreq, t->a_channels, t->a_bps, 0x0003 == t->a_formattag ? pcm_packetizer_c::ieee_float : pcm_packetizer_c::little_endian_integer));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_tta_audio_packetizer(kax_track_t *t,
                                          track_info_c &nti) {
  nti.m_private_data.reset();
  set_track_packetizer(t, new tta_packetizer_c(this, nti, t->a_channels, t->a_bps, t->a_sfreq));
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
  nti.m_private_data.reset();

  wavpack_meta_t meta;
  meta.bits_per_sample = t->a_bps;
  meta.channel_count   = t->a_channels;
  meta.sample_rate     = t->a_sfreq;
  meta.has_correction  = t->max_blockadd_id != 0;

  if (0.0 < t->v_frate)
    meta.samples_per_block = t->a_sfreq / t->v_frate;

  set_track_packetizer(t, new wavpack_packetizer_c(this, nti, meta));

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

  else if (FOURCC('A', 'L', 'A', 'C') == t->a_formattag)
    create_alac_audio_packetizer(t, nti);

  else if ((FOURCC('M', 'P', '4', 'A') == t->a_formattag) || (0x00ff == t->a_formattag))
    create_aac_audio_packetizer(t, nti);

#if defined(HAVE_FLAC_FORMAT_H)
  else if ((FOURCC('f', 'L', 'a', 'C') == t->a_formattag) || (0xf1ac == t->a_formattag))
    create_flac_audio_packetizer(t, nti);
#endif

  else if (FOURCC('O', 'p', 'u', 's') == t->a_formattag)
    create_opus_audio_packetizer(t, nti);

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
    set_track_packetizer(t, new vobsub_packetizer_c(this, nti));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

    t->sub_type = 'v';

  } else if (balg::starts_with(t->codec_id, "S_TEXT") || (t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) {
    std::string new_codec_id = ((t->codec_id == "S_SSA") || (t->codec_id == "S_ASS")) ? std::string("S_TEXT/") + std::string(&t->codec_id[2]) : t->codec_id;

    set_track_packetizer(t, new textsubs_packetizer_c(this, nti, new_codec_id.c_str(), false, true));
    show_packetizer_info(t->tnum, t->ptzr_ptr);

    t->sub_type = 't';

  } else if (t->codec_id == MKV_S_KATE) {
    set_track_packetizer(t, new kate_packetizer_c(this, nti));
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
  if (t->codec_id != MKV_B_VOBBTN) {
    init_passthrough_packetizer(t);
    return;
  }

  nti.m_private_data.reset();
  t->sub_type = 'b';

  set_track_packetizer(t, new vobbtn_packetizer_c(this, nti, t->v_width, t->v_height));
  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_packetizer(int64_t tid) {
  kax_track_t *t = m_tracks[tid].get();

  if ((-1 != t->ptzr) || !t->ok || !demuxing_requested(t->type, t->tnum))
    return;

  track_info_c nti(m_ti);
  nti.m_private_data = memory_c::clone(t->private_data, t->private_size);
  nti.m_id           = t->tnum; // ID for this track.

  if (nti.m_language == "")
    nti.m_language   = t->language;
  if (nti.m_track_name == "")
    nti.m_track_name = t->track_name;
  if (t->tags && demuxing_requested('T', t->tnum))
    nti.m_tags       = clone(t->tags);

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
  for (auto &track : m_tracks)
    create_packetizer(track->tnum);

  if (!g_segment_title_set) {
    g_segment_title     = m_title;
    g_segment_title_set = true;
  }
}

void
kax_reader_c::create_mpeg4_p10_es_video_packetizer(kax_track_t *t,
                                                   track_info_c &nti) {
  mpeg4_p10_es_video_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, nti);
  set_track_packetizer(t, ptzr);

  ptzr->set_video_pixel_dimensions(t->v_width, t->v_height);

  show_packetizer_info(t->tnum, t->ptzr_ptr);
}

void
kax_reader_c::create_mpeg4_p10_video_packetizer(kax_track_t *t,
                                                track_info_c &nti) {
  if (!nti.m_private_data || !nti.m_private_data->get_size()) {
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
  read_first_frames(t, 1);
  if (   !t->first_frames_data.empty()
      && (4 <= t->first_frames_data[0]->get_size())
      && !vc1::is_marker(get_uint32_be(t->first_frames_data[0]->get_buffer()))) {
    init_passthrough_packetizer(t);
    return;
  }

  set_track_packetizer(t, new vc1_video_packetizer_c(this, nti));
  show_packetizer_info(t->tnum, t->ptzr_ptr);

  if (t->private_data && (sizeof(alBITMAPINFOHEADER) < t->private_size))
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
      if (!cluster)
        return;

      KaxClusterTimecode *ctc = static_cast<KaxClusterTimecode *> (cluster->FindFirstElt(EBML_INFO(KaxClusterTimecode), false));
      if (ctc)
        cluster->InitTimecode(ctc->GetValue(), m_tc_scale);

      size_t bgidx;
      for (bgidx = 0; bgidx < cluster->ListSize(); bgidx++) {
        if ((EbmlId(*(*cluster)[bgidx]) == EBML_ID(KaxSimpleBlock))) {
          KaxSimpleBlock *block_simple = static_cast<KaxSimpleBlock *>((*cluster)[bgidx]);

          block_simple->SetParent(*cluster);
          kax_track_t *block_track = find_track_by_num(block_simple->TrackNum());

          if (!block_track || (0 == block_simple->NumberFrames()))
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

          if (!block)
            continue;

          block->SetParent(*cluster);
          kax_track_t *block_track = find_track_by_num(block->TrackNum());

          if (!block_track || (0 == block->NumberFrames()))
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

  if (!force) {
    auto num_queued_bytes = get_queued_bytes();
    if (20 * 1024 * 1024 < num_queued_bytes) {
      kax_track_t *requested_ptzr_track = m_ptzr_to_track_map[requested_ptzr];
      if (!requested_ptzr_track || (('a' != requested_ptzr_track->type) && ('v' != requested_ptzr_track->type)) || (512 * 1024 * 1024 < num_queued_bytes))
        return FILE_STATUS_HOLDING;
    }
  }

  try {
    KaxCluster *cluster = m_in_file->read_next_cluster();
    if (!cluster) {
      flush_packetizers();

      m_file_status = FILE_STATUS_DONE;
      return FILE_STATUS_DONE;
    }

    auto cluster_tc = FindChildValue<KaxClusterTimecode>(cluster);
    cluster->InitTimecode(cluster_tc, m_tc_scale);

    if (-1 == m_first_timecode) {
      m_first_timecode = cluster_tc * m_tc_scale;

      // If we're appending this file to another one then the core
      // needs the timecodes shifted to zero.
      if (m_appending && m_chapters && (0 < m_first_timecode))
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

  if (!block_track) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                              "The block will be skipped.\n")) % format_timecode(block_simple->GlobalTimecode()) % block_simple->TrackNum());
    return;
  }

  if (0 != block_track->v_frate)
    block_duration = 1000000000.0 / block_track->v_frate;
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
  if (0 < block_simple->NumberFrames())
    m_in_file->set_last_timecode(m_last_timecode + (block_simple->NumberFrames() - 1) * frame_duration);

  // If we're appending this file to another one then the core
  // needs the timecodes shifted to zero.
  if (m_appending)
    m_last_timecode -= m_first_timecode;

  if ((-1 != block_track->ptzr) && block_track->passthrough) {
    // The handling for passthrough is a bit different. We don't have
    // any special cases, e.g. 0 terminating a string for the subs
    // and stuff. Just pass everything through as it is.
    size_t i;
    for (i = 0; block_simple->NumberFrames() > i; ++i) {
      DataBuffer &data_buffer = block_simple->GetBuffer(i);
      memory_cptr data(new memory_c(data_buffer.Buffer(), data_buffer.Size(), false));
      block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);
      packet_cptr packet(new packet_t(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref));

      static_cast<passthrough_packetizer_c *>(PTZR(block_track->ptzr))->process(packet);
    }

  } else if (-1 != block_track->ptzr) {
    size_t i;
    for (i = 0; i < block_simple->NumberFrames(); i++) {
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

  block_track->previous_timecode  = m_last_timecode;
  block_track->units_processed   += block_simple->NumberFrames();
}

void
kax_reader_c::process_block_group_common(KaxBlockGroup *block_group,
                                         packet_t *packet) {
  auto codec_state     = FindChild<KaxCodecState>(block_group);
  auto discard_padding = FindChild<KaxDiscardPadding>(block_group);

  if (codec_state)
    packet->codec_state = memory_c::clone(codec_state->GetBuffer(), codec_state->GetSize());

  if (discard_padding)
    packet->discard_padding = timecode_c::ns(discard_padding->GetValue());
}

void
kax_reader_c::process_block_group(KaxCluster *cluster,
                                  KaxBlockGroup *block_group) {
  auto block = FindChild<KaxBlock>(block_group);
  if (!block) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block group was found at position %1%, but no block element was found inside it. This might make mkvmerge crash.\n"))
              % block_group->GetElementPosition());
    return;
  }

  block->SetParent(*cluster);
  auto block_track = find_track_by_num(block->TrackNum());

  if (!block_track) {
    mxwarn_fn(m_ti.m_fname,
              boost::format(Y("A block was found at timestamp %1% for track number %2%. However, no headers where found for that track number. "
                              "The block will be skipped.\n")) % format_timecode(block->GlobalTimecode()) % block->TrackNum());
    return;
  }

  auto duration       = FindChild<KaxBlockDuration>(block_group);
  auto block_duration = duration             ? static_cast<int64_t>(duration->GetValue() * m_tc_scale / block->NumberFrames())
                      : block_track->v_frate ? static_cast<int64_t>(1000000000.0 / block_track->v_frate)
                      :                        int64_t{-1};
  auto frame_duration = -1 == block_duration ? int64_t{0} : block_duration;
  m_last_timecode     = block->GlobalTimecode();

  if (0 < block->NumberFrames())
    m_in_file->set_last_timecode(m_last_timecode + (block->NumberFrames() - 1) * frame_duration);

  // If we're appending this file to another one then the core
  // needs the timecodes shifted to zero.
  if (m_appending)
    m_last_timecode -= m_first_timecode;

  if (-1 == block_track->ptzr)
    return;

  auto block_bref = int64_t{VFT_IFRAME};
  auto block_fref = int64_t{VFT_NOBFRAME};
  bool bref_found = false;
  bool fref_found = false;
  auto ref_block  = FindChild<KaxReferenceBlock>(block_group);

  while (ref_block) {
    if (0 >= ref_block->GetValue()) {
      block_bref = ref_block->GetValue() * m_tc_scale;
      bref_found = true;
    } else {
      block_fref = ref_block->GetValue() * m_tc_scale;
      fref_found = true;
    }

    ref_block = FindNextChild<KaxReferenceBlock>(block_group, ref_block);
  }

  if (('s' == block_track->type) && (-1 == block_duration))
    block_duration = 0;

  if (block_track->ignore_duration_hack) {
    frame_duration = 0;
    if (0 < block_duration)
      block_duration = 0;
  }

  if (block_track->passthrough) {
    // The handling for passthrough is a bit different. We don't have
    // any special cases, e.g. 0 terminating a string for the subs
    // and stuff. Just pass everything through as it is.
    if (bref_found)
      block_bref += m_last_timecode;
    if (fref_found)
      block_fref += m_last_timecode;

    size_t i;
    for (i = 0; i < block->NumberFrames(); i++) {
      auto &data_buffer = block->GetBuffer(i);
      auto data         = std::make_shared<memory_c>(data_buffer.Buffer(), data_buffer.Size(), false);
      block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);

      auto packet                = std::make_shared<packet_t>(data, m_last_timecode + i * frame_duration, block_duration, block_bref, block_fref);
      packet->duration_mandatory = duration;

      process_block_group_common(block_group, packet.get());

      static_cast<passthrough_packetizer_c *>(PTZR(block_track->ptzr))->process(packet);
    }

    return;
  }

  if (bref_found) {
    if (FOURCC('r', 'e', 'a', 'l') == block_track->a_formattag)
      block_bref = block_track->previous_timecode;
    else
      block_bref += m_last_timecode;
  }
  if (fref_found)
    block_fref += m_last_timecode;

  for (auto block_idx = 0u, num_frames = block->NumberFrames(); block_idx < num_frames; ++block_idx) {
    auto &data_buffer = block->GetBuffer(block_idx);
    auto data         = std::make_shared<memory_c>(data_buffer.Buffer(), data_buffer.Size(), false);
    block_track->content_decoder.reverse(data, CONTENT_ENCODING_SCOPE_BLOCK);

    if (('s' == block_track->type) && ('t' == block_track->sub_type)) {
      if ((2 < data->get_size()) || ((0 < data->get_size()) && (' ' != *data->get_buffer()) && (0 != *data->get_buffer()) && !iscr(*data->get_buffer()))) {
        auto mem = data->clone();
        mem->resize(mem->get_size() + 1);
        mem->get_buffer()[ mem->get_size() - 1 ] = 0;

        auto packet = std::make_shared<packet_t>(mem, m_last_timecode, block_duration, block_bref, block_fref);

        process_block_group_common(block_group, packet.get());

        PTZR(block_track->ptzr)->process(packet);
      }

    } else {
      auto packet = std::make_shared<packet_t>(data, m_last_timecode + block_idx * frame_duration, block_duration, block_bref, block_fref);

      if ((duration) && !duration->GetValue())
        packet->duration_mandatory = true;

      process_block_group_common(block_group, packet.get());

      auto blockadd = FindChild<KaxBlockAdditions>(block_group);
      if (blockadd) {
        for (auto &child : *blockadd) {
          if (!(is_id(child, KaxBlockMore)))
            continue;

          auto blockmore     = static_cast<KaxBlockMore *>(child);
          auto blockadd_data = &GetChild<KaxBlockAdditional>(*blockmore);
          auto blockadded    = std::make_shared<memory_c>(blockadd_data->GetBuffer(), blockadd_data->GetSize(), false);
          block_track->content_decoder.reverse(blockadded, CONTENT_ENCODING_SCOPE_BLOCK);

          packet->data_adds.push_back(blockadded);
        }
      }

      PTZR(block_track->ptzr)->process(packet);
    }
  }

  block_track->previous_timecode  = m_last_timecode;
  block_track->units_processed   += block->NumberFrames();
}

int
kax_reader_c::get_progress() {
  if (0 != m_segment_duration)
    return (m_last_timecode - std::max(m_first_timecode, static_cast<int64_t>(0))) * 100 / m_segment_duration;

  return 100 * m_in->getFilePointer() / m_size;
}

void
kax_reader_c::set_headers() {
  generic_reader_c::set_headers();

  for (auto &track : m_tracks)
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

  auto add_uid_info = [&](memory_cptr const &uid, std::string const &property) {
    if (uid)
      verbose_info.push_back((boost::format("%1%:%2%") % property % to_hex(uid, true)).str());
  };
  add_uid_info(m_segment_uid,          "segment_uid");
  add_uid_info(m_next_segment_uid,     "next_segment_uid");
  add_uid_info(m_previous_segment_uid, "previous_segment_uid");

  id_result_container(verbose_info);

  for (auto &track : m_tracks) {
    if (!track->ok)
      continue;

    verbose_info.clear();

    verbose_info.push_back((boost::format("number:%1%") % track->track_number).str());
    verbose_info.push_back((boost::format("uid:%1%") % track->track_uid).str());
    verbose_info.push_back((boost::format("codec_id:%1%") % escape(track->codec_id)).str());
    verbose_info.push_back((boost::format("codec_private_length:%1%") % track->private_size).str());

    if ((0 != track->private_size) && track->private_data)
      verbose_info.push_back((boost::format("codec_private_data:%1%") % to_hex(static_cast<const unsigned char *>(track->private_data), track->private_size, true)).str());

    if (track->language != "")
      verbose_info.push_back((boost::format("language:%1%") % escape(track->language)).str());

    if (track->track_name != "")
      verbose_info.push_back((boost::format("track_name:%1%") % escape(track->track_name)).str());

    if ((0 != track->v_width) && (0 != track->v_height))
      verbose_info.push_back((boost::format("pixel_dimensions:%1%x%2%") % track->v_width % track->v_height).str());

    if ((0 != track->v_dwidth) && (0 != track->v_dheight))
      verbose_info.push_back((boost::format("display_dimensions:%1%x%2%") % track->v_dwidth % track->v_dheight).str());

    if (stereo_mode_c::unspecified != track->v_stereo_mode)
      verbose_info.push_back((boost::format("stereo_mode:%1%") % static_cast<int>(track->v_stereo_mode)).str());

    if ((0 != track->v_pcleft) || (0 != track->v_pctop) || (0 != track->v_pcright) || (0 != track->v_pcbottom))
      verbose_info.push_back((boost::format("cropping:%1%,%2%,%3%,%4%") % track->v_pcleft % track->v_pctop % track->v_pcright % track->v_pcbottom).str());

    verbose_info.push_back((boost::format("default_track:%1%") % (track->default_track ? 1 : 0)).str());
    verbose_info.push_back((boost::format("forced_track:%1%")  % (track->forced_track  ? 1 : 0)).str());
    verbose_info.push_back((boost::format("enabled_track:%1%") % (track->enabled_track ? 1 : 0)).str());

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

    if (track->content_decoder.has_encodings())
      verbose_info.push_back((boost::format("content_encoding_algorithms:%1%") % escape(track->content_decoder.descriptive_algorithm_list())).str());

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

  for (auto &attachment : g_attachments)
    id_result_attachment(attachment.ui_id, attachment.mime_type, attachment.data->get_size(), attachment.name, attachment.description);

  if (m_chapters)
    id_result_chapters(count_chapter_atoms(*m_chapters));

  if (m_tags)
    id_result_tags(ID_RESULT_GLOBAL_TAGS_ID, count_simple_tags(*m_tags));

  for (auto &track : m_tracks)
    if (track->ok && track->tags)
      id_result_tags(track->tnum, count_simple_tags(*track->tags));
}

void
kax_reader_c::add_available_track_ids() {
  for (auto &track : m_tracks)
    add_available_track_id(track->tnum);
}

void
kax_reader_c::set_track_packetizer(kax_track_t *t,
                                   generic_packetizer_c *ptzr) {
  t->ptzr     = add_packetizer(ptzr);
  t->ptzr_ptr = PTZR(t->ptzr);
}
