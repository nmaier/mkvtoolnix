/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AVI demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
// This one goes out to haali ;)
#include <sys/types.h>

#include <avilib.h>

#include "common/aac.h"
#include "common/endian.h"
#include "common/error.h"
#include "common/hacks.h"
#include "common/ivf.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "common/mpeg4_p10.h"
#include "common/strings/formatting.h"
#include "input/r_avi.h"
#include "input/subtitles.h"
#include "merge/output_control.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_dts.h"
#include "output/p_mp3.h"
#include "output/p_mpeg1_2.h"
#include "output/p_mpeg4_p2.h"
#include "output/p_mpeg4_p10.h"
#include "output/p_pcm.h"
#include "output/p_video.h"
#include "output/p_vorbis.h"
#include "output/p_vpx.h"

#define GAB2_TAG                 FOURCC('G', 'A', 'B', '2')
#define GAB2_ID_LANGUAGE         0x0000
#define GAB2_ID_LANGUAGE_UNICODE 0x0002
#define GAB2_ID_SUBTITLES        0x0004

avi_subs_demuxer_t::avi_subs_demuxer_t()
  : m_ptzr(-1)
{
}

bool
avi_reader_c::probe_file(mm_io_c *in,
                         uint64_t size) {
  if (12 > size)
    return false;

  std::string data;
  try {
    in->setFilePointer(0, seek_beginning);
    if (in->read(data, 12) != 12)
      return false;
    in->setFilePointer(0, seek_beginning);
  } catch (...) {
    return false;
  }

  balg::to_lower(data);
  return (data.substr(0, 4) == "riff") && (data.substr(8, 4) == "avi ");
}

avi_reader_c::avi_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_divx_type(DIVX_TYPE_NONE)
  , m_avi(nullptr)
  , m_vptzr(-1)
  , m_fps(1.0)
  , m_video_frames_read(0)
  , m_max_video_frames(0)
  , m_dropped_video_frames(0)
  , m_avc_nal_size_size(-1)
  , m_bytes_to_process(0)
  , m_bytes_processed(0)
  , m_video_track_ok(false)
{
}

void
avi_reader_c::read_headers() {
  try {
    if (!avi_reader_c::probe_file(m_in.get(), m_size))
      throw mtx::input::invalid_format_x();

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }

  show_demuxer_info();

  if (!(m_avi = AVI_open_input_file(m_in.get(), 1)))
    throw mtx::input::invalid_format_x();

  m_fps              = AVI_frame_rate(m_avi);
  m_max_video_frames = AVI_video_frames(m_avi);

  verify_video_track();
  parse_subtitle_chunks();

  if (debugging_requested("avi_dump_video_index"))
    debug_dump_video_index();
}

avi_reader_c::~avi_reader_c() {
  if (m_avi)
    AVI_close(m_avi);

  mxverb(2, boost::format("avi_reader_c: Dropped video frames: %1%\n") % m_dropped_video_frames);
}

void
avi_reader_c::verify_video_track() {
  alBITMAPINFOHEADER *bih = m_avi->bitmap_info_header;
  size_t size             = get_uint32_le(&bih->bi_size);

  if (sizeof(alBITMAPINFOHEADER) > size)
    return;

  const char *codec = AVI_video_compressor(m_avi);
  if ((0 == codec[0]) || (0 == AVI_video_width(m_avi)) || (0 == AVI_video_height(m_avi)))
    return;

  m_video_track_ok = true;
}

void
avi_reader_c::parse_subtitle_chunks() {
  int i;
  for (i = 0; AVI_text_tracks(m_avi) > i; ++i) {
    AVI_set_text_track(m_avi, i);

    if (AVI_text_chunks(m_avi) == 0)
      continue;

    int chunk_size = AVI_read_text_chunk(m_avi, nullptr);
    if (0 >= chunk_size)
      continue;

    memory_cptr chunk(memory_c::alloc(chunk_size));
    chunk_size = AVI_read_text_chunk(m_avi, reinterpret_cast<char *>(chunk->get_buffer()));

    if (0 >= chunk_size)
      continue;

    avi_subs_demuxer_t demuxer;

    try {
      mm_mem_io_c io(chunk->get_buffer(), chunk_size);
      uint32_t tag = io.read_uint32_be();

      if (GAB2_TAG != tag)
        continue;

      io.skip(1);

      while (!io.eof() && (io.getFilePointer() < static_cast<size_t>(chunk_size))) {
        uint16_t id = io.read_uint16_le();
        int len     = io.read_uint32_le();

        if (GAB2_ID_SUBTITLES == id) {
          demuxer.m_subtitles = memory_c::alloc(len);
          len                 = io.read(demuxer.m_subtitles->get_buffer(), len);
          demuxer.m_subtitles->set_size(len);

        } else
          io.skip(len);
      }

      if (0 != demuxer.m_subtitles->get_size()) {
        mm_text_io_c text_io(new mm_mem_io_c(demuxer.m_subtitles->get_buffer(), demuxer.m_subtitles->get_size()));
        demuxer.m_type
          = srt_parser_c::probe(&text_io) ? avi_subs_demuxer_t::TYPE_SRT
          : ssa_parser_c::probe(&text_io) ? avi_subs_demuxer_t::TYPE_SSA
          :                                 avi_subs_demuxer_t::TYPE_UNKNOWN;

        if (avi_subs_demuxer_t::TYPE_UNKNOWN != demuxer.m_type)
          m_subtitle_demuxers.push_back(demuxer);
      }

    } catch (...) {
    }
  }
}

void
avi_reader_c::create_packetizer(int64_t tid) {
  m_ti.m_private_data.reset();

  if ((0 == tid) && demuxing_requested('v', 0) && (-1 == m_vptzr) && m_video_track_ok)
    create_video_packetizer();

  else if ((0 != tid) && (tid <= AVI_audio_tracks(m_avi)) && demuxing_requested('a', tid))
    add_audio_demuxer(tid - 1);
}

void
avi_reader_c::create_video_packetizer() {
  size_t i;

  mxverb_tid(4, m_ti.m_fname, 0, "frame sizes:\n");

  for (i = 0; i < m_max_video_frames; i++) {
    m_bytes_to_process += AVI_frame_size(m_avi, i);
    mxverb(4, boost::format("  %1%: %2%\n") % i % AVI_frame_size(m_avi, i));
  }

  if (m_avi->bitmap_info_header) {
    m_ti.m_private_data = memory_c::clone(m_avi->bitmap_info_header, get_uint32_le(&m_avi->bitmap_info_header->bi_size));

    mxverb(4, boost::format("track extra data size: %1%\n") % (m_ti.m_private_data->get_size() - sizeof(alBITMAPINFOHEADER)));

    if (sizeof(alBITMAPINFOHEADER) < m_ti.m_private_data->get_size())
      mxverb(4, boost::format("  %1%\n") % to_hex(m_ti.m_private_data->get_buffer() + sizeof(alBITMAPINFOHEADER), m_ti.m_private_data->get_size() - sizeof(alBITMAPINFOHEADER)));
  }

  const char *codec = AVI_video_compressor(m_avi);
  if (mpeg4::p2::is_v3_fourcc(codec))
    m_divx_type = DIVX_TYPE_V3;
  else if (mpeg4::p2::is_fourcc(codec))
    m_divx_type = DIVX_TYPE_MPEG4;

  if (map_has_key(m_ti.m_default_durations, 0))
    m_fps = 1000000000.0 / m_ti.m_default_durations[0];

  else if (map_has_key(m_ti.m_default_durations, -1))
    m_fps = 1000000000.0 / m_ti.m_default_durations[-1];

  m_ti.m_id = 0;                 // ID for the video track.
  if (DIVX_TYPE_MPEG4 == m_divx_type)
    create_mpeg4_p2_packetizer();

  else if (mpeg4::p10::is_avc_fourcc(codec) && !hack_engaged(ENGAGE_ALLOW_AVC_IN_VFW_MODE))
    create_mpeg4_p10_packetizer();

  else if (mpeg1_2::is_fourcc(get_uint32_le(codec)))
    create_mpeg1_2_packetizer();

  else if (FOURCC('V', 'P', '8', '0') == get_uint32_be(codec))
    create_vp8_packetizer();

  else
    create_standard_video_packetizer();
}

void
avi_reader_c::create_mpeg1_2_packetizer() {
  std::shared_ptr<M2VParser> m2v_parser(new M2VParser);

  m2v_parser->SetProbeMode();
  if (m_ti.m_private_data && (m_ti.m_private_data->get_size() < sizeof(alBITMAPINFOHEADER)))
    m2v_parser->WriteData(m_ti.m_private_data->get_buffer() + sizeof(alBITMAPINFOHEADER), m_ti.m_private_data->get_size() - sizeof(alBITMAPINFOHEADER));

  unsigned int frame_number = 0;
  unsigned int state        = m2v_parser->GetState();
  while ((frame_number < std::min(m_max_video_frames, 100u)) && (MPV_PARSER_STATE_FRAME != state)) {
    ++frame_number;

    int size = AVI_frame_size(m_avi, frame_number - 1);
    if (0 == size)
      continue;

    AVI_set_video_position(m_avi, frame_number - 1);

    memory_cptr buffer = memory_c::alloc(size);
    int key      = 0;
    int num_read = AVI_read_frame(m_avi, reinterpret_cast<char *>(buffer->get_buffer()), &key);

    if (0 >= num_read)
      continue;

    m2v_parser->WriteData(buffer->get_buffer(), num_read);

    state = m2v_parser->GetState();
  }

  AVI_set_video_position(m_avi, 0);

  if (MPV_PARSER_STATE_FRAME != state)
    mxerror_tid(m_ti.m_fname, 0, Y("Could not extract the sequence header from this MPEG-1/2 track.\n"));

  MPEG2SequenceHeader seq_hdr = m2v_parser->GetSequenceHeader();
  std::shared_ptr<MPEGFrame> frame(m2v_parser->ReadFrame());
  if (!frame)
    mxerror_tid(m_ti.m_fname, 0, Y("Could not extract the sequence header from this MPEG-1/2 track.\n"));

  int display_width      = ((0 >= seq_hdr.aspectRatio) || (1 == seq_hdr.aspectRatio)) ? seq_hdr.width : static_cast<int>(seq_hdr.height * seq_hdr.aspectRatio);

  MPEGChunk *raw_seq_hdr = m2v_parser->GetRealSequenceHeader();
  if (raw_seq_hdr)
    m_ti.m_private_data  = memory_c::clone(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
  else
    m_ti.m_private_data.reset();

  m_vptzr                = add_packetizer(new mpeg1_2_video_packetizer_c(this, m_ti, m2v_parser->GetMPEGVersion(), seq_hdr.frameOrFieldRate,
                                                                         seq_hdr.width, seq_hdr.height, display_width, seq_hdr.height, false));

  show_packetizer_info(0, PTZR(m_vptzr));
}

void
avi_reader_c::create_mpeg4_p2_packetizer() {
  m_vptzr = add_packetizer(new mpeg4_p2_video_packetizer_c(this, m_ti, m_fps, AVI_video_width(m_avi), AVI_video_height(m_avi), false));

  show_packetizer_info(0, PTZR(m_vptzr));
}

void
avi_reader_c::create_mpeg4_p10_packetizer() {
  try {
    mpeg4_p10_es_video_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, m_ti);
    m_vptzr                               = add_packetizer(ptzr);

    ptzr->set_video_pixel_dimensions(AVI_video_width(m_avi), AVI_video_height(m_avi));

    if (0 != m_fps)
      ptzr->set_container_default_field_duration(1000000000ll / m_fps / 2);

    uint32_t extra_data_size = get_uint32_le(&m_avi->bitmap_info_header->bi_size) - sizeof(alBITMAPINFOHEADER);
    if (0 < extra_data_size) {
      memory_cptr avc_extra_nalus = mpeg4::p10::avcc_to_nalus(reinterpret_cast<unsigned char *>(m_avi->bitmap_info_header + 1), extra_data_size);
      if (avc_extra_nalus)
        ptzr->add_extra_data(avc_extra_nalus);
    }

    set_avc_nal_size_size(ptzr);

    show_packetizer_info(0, ptzr);

  } catch (...) {
    mxerror_tid(m_ti.m_fname, 0, Y("Could not extract the decoder specific config data (AVCC) from this AVC/h.264 track.\n"));
  }
}

void
avi_reader_c::create_vp8_packetizer() {
  m_ti.m_private_data.reset();
  m_vptzr = add_packetizer(new vpx_video_packetizer_c(this, m_ti, ivf::VP8));

  PTZR(m_vptzr)->set_track_default_duration(1000000000ll / m_fps);
  PTZR(m_vptzr)->set_video_pixel_width(AVI_video_width(m_avi));
  PTZR(m_vptzr)->set_video_pixel_height(AVI_video_height(m_avi));

  show_packetizer_info(0, PTZR(m_vptzr));
}

void
avi_reader_c::create_standard_video_packetizer() {
  m_vptzr = add_packetizer(new video_packetizer_c(this, m_ti, nullptr, m_fps, AVI_video_width(m_avi), AVI_video_height(m_avi)));

  show_packetizer_info(0, PTZR(m_vptzr));
}

void
avi_reader_c::create_packetizers() {
  int i;

  create_packetizer(0);

  for (i = 0; i < AVI_audio_tracks(m_avi); i++)
    create_packetizer(i + 1);

  for (i = 0; static_cast<int>(m_subtitle_demuxers.size()) > i; ++i)
    create_subs_packetizer(i);
}

void
avi_reader_c::create_subs_packetizer(int idx) {
  if (!demuxing_requested('s', 1 + AVI_audio_tracks(m_avi) + idx))
    return;

  m_ti.m_private_data.reset();
  avi_subs_demuxer_t &demuxer = m_subtitle_demuxers[idx];

  demuxer.m_text_io = mm_text_io_cptr(new mm_text_io_c(new mm_mem_io_c(demuxer.m_subtitles->get_buffer(), demuxer.m_subtitles->get_size())));

  if (avi_subs_demuxer_t::TYPE_SRT == demuxer.m_type)
    create_srt_packetizer(idx);

  else if (avi_subs_demuxer_t::TYPE_SSA == demuxer.m_type)
    create_ssa_packetizer(idx);
}

void
avi_reader_c::create_srt_packetizer(int idx) {
  avi_subs_demuxer_t &demuxer = m_subtitle_demuxers[idx];
  int id                      = idx + 1 + AVI_audio_tracks(m_avi);

  srt_parser_c *parser        = new srt_parser_c(demuxer.m_text_io.get(), m_ti.m_fname, id);
  demuxer.m_subs              = subtitles_cptr(parser);

  parser->parse();

  bool is_utf8   = demuxer.m_text_io->get_byte_order() != BO_NONE;
  demuxer.m_ptzr = add_packetizer(new textsubs_packetizer_c(this, m_ti, MKV_S_TEXTUTF8, true, is_utf8));

  show_packetizer_info(id, PTZR(demuxer.m_ptzr));
}

void
avi_reader_c::create_ssa_packetizer(int idx) {
  avi_subs_demuxer_t &demuxer    = m_subtitle_demuxers[idx];
  int id                         = idx + 1 + AVI_audio_tracks(m_avi);

  ssa_parser_c *parser           = new ssa_parser_c(this, demuxer.m_text_io.get(), m_ti.m_fname, id);
  demuxer.m_subs                 = subtitles_cptr(parser);

  charset_converter_cptr cc_utf8 = map_has_key(m_ti.m_sub_charsets, id)           ? charset_converter_c::init(m_ti.m_sub_charsets[id])
                                 : map_has_key(m_ti.m_sub_charsets, -1)           ? charset_converter_c::init(m_ti.m_sub_charsets[-1])
                                 : demuxer.m_text_io->get_byte_order() != BO_NONE ? charset_converter_c::init("UTF-8")
                                 :                                                  g_cc_local_utf8;

  parser->set_charset_converter(cc_utf8);
  parser->set_attachment_id_base(g_attachments.size());
  parser->parse();

  m_ti.m_private_data = memory_c::clone(parser->get_global());
  demuxer.m_ptzr      = add_packetizer(new textsubs_packetizer_c(this, m_ti, parser->is_ass() ?  MKV_S_TEXTASS : MKV_S_TEXTSSA, false, false));

  show_packetizer_info(id, PTZR(demuxer.m_ptzr));
}

void
avi_reader_c::add_audio_demuxer(int aid) {
  for (auto &demuxer : m_audio_demuxers)
    if (demuxer.m_aid == aid) // Demuxer already added?
      return;

  AVI_set_audio_track(m_avi, aid);
  if (AVI_read_audio_chunk(m_avi, nullptr) < 0) {
    mxwarn(boost::format(Y("Could not find an index for audio track %1% (avilib error message: %2%). Skipping track.\n")) % (aid + 1) % AVI_strerror());
    return;
  }

  avi_demuxer_t demuxer;
  generic_packetizer_c *packetizer = nullptr;
  alWAVEFORMATEX *wfe              = m_avi->wave_format_ex[aid];
  uint32_t audio_format            = AVI_audio_format(m_avi);

  demuxer.m_aid                    = aid;
  demuxer.m_ptzr                   = -1;
  demuxer.m_samples_per_second     = AVI_audio_rate(m_avi);
  demuxer.m_channels               = AVI_audio_channels(m_avi);
  demuxer.m_bits_per_sample        = AVI_audio_bits(m_avi);

  m_ti.m_id                        = aid + 1;       // ID for this audio track.
  m_ti.m_avi_block_align           = get_uint16_le(&wfe->n_block_align);
  m_ti.m_avi_avg_bytes_per_sec     = get_uint32_le(&wfe->n_avg_bytes_per_sec);
  m_ti.m_avi_samples_per_chunk     = get_uint32_le(&m_avi->stream_headers[aid].dw_scale);
  m_ti.m_avi_sample_scale          = get_uint32_le(&m_avi->stream_headers[aid].dw_rate);
  m_ti.m_avi_samples_per_sec       = demuxer.m_samples_per_second;

  if ((0xfffe == audio_format) && (get_uint16_le(&wfe->cb_size) >= (sizeof(alWAVEFORMATEXTENSION)))) {
    alWAVEFORMATEXTENSIBLE *ext = reinterpret_cast<alWAVEFORMATEXTENSIBLE *>(wfe);
    audio_format                = get_uint32_le(&ext->extension.guid.data1);

  } else if (get_uint16_le(&wfe->cb_size) > 0)
    m_ti.m_private_data = memory_c::clone(wfe + 1, get_uint16_le(&wfe->cb_size));

  else
    m_ti.m_private_data.reset();

  switch(audio_format) {
    case 0x0001:                // raw PCM audio
    case 0x0003:                // raw PCM audio (float)
      packetizer = new pcm_packetizer_c(this, m_ti, demuxer.m_samples_per_second, demuxer.m_channels, demuxer.m_bits_per_sample, 0x0003 == audio_format ? pcm_packetizer_c::ieee_float : pcm_packetizer_c::little_endian_integer);
      break;

    case 0x0050:                // MP2
    case 0x0055:                // MP3
      packetizer = new mp3_packetizer_c(this, m_ti, demuxer.m_samples_per_second, demuxer.m_channels, false);
      break;

    case 0x2000:                // AC3
      packetizer = new ac3_packetizer_c(this, m_ti, demuxer.m_samples_per_second, demuxer.m_channels, 0);
      break;

    case 0x2001:                // DTS
      packetizer = create_dts_packetizer(aid);
      break;

    case 0x00ff:
    case 0x706d:                // AAC
      packetizer = create_aac_packetizer(aid, demuxer);
      break;

    case 0x566f:                // Vorbis
      packetizer = create_vorbis_packetizer(aid);
      break;

    default:
      mxerror_tid(m_ti.m_fname, aid + 1, boost::format(Y("Unknown/unsupported audio format 0x%|1$04x| for this audio track.\n")) % audio_format);
  }

  show_packetizer_info(aid + 1, packetizer);

  demuxer.m_ptzr = add_packetizer(packetizer);

  m_audio_demuxers.push_back(demuxer);

  int i, maxchunks = AVI_audio_chunks(m_avi);
  for (i = 0; i < maxchunks; i++)
    m_bytes_to_process += AVI_audio_size(m_avi, i);
}

generic_packetizer_c *
avi_reader_c::create_aac_packetizer(int aid,
                                    avi_demuxer_t &demuxer) {
  int profile, channels, sample_rate, output_sample_rate;
  bool is_sbr;

  bool headerless        = (AVI_audio_format(m_avi) != 0x706d);

  if (!m_ti.m_private_data
      || (   (0x706d                       == AVI_audio_format(m_avi))
             && ((sizeof(alWAVEFORMATEX) + 7)  < m_ti.m_private_data->get_size()))) {
    channels             = AVI_audio_channels(m_avi);
    sample_rate          = AVI_audio_rate(m_avi);
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
    auto size           = create_aac_data(created_aac_data, profile, channels, sample_rate, output_sample_rate, is_sbr);
    m_ti.m_private_data = memory_c::clone(created_aac_data, size);

  } else {
    if (!parse_aac_data(m_ti.m_private_data->get_buffer(), m_ti.m_private_data->get_size(), profile, channels, sample_rate, output_sample_rate, is_sbr))
      mxerror_tid(m_ti.m_fname, aid + 1, Y("This AAC track does not contain valid headers. Could not parse the AAC information.\n"));

    if (is_sbr)
      profile = AAC_PROFILE_SBR;
  }

  demuxer.m_samples_per_second     = sample_rate;
  demuxer.m_channels               = channels;

  generic_packetizer_c *packetizer = new aac_packetizer_c(this, m_ti, AAC_ID_MPEG4, profile, demuxer.m_samples_per_second, demuxer.m_channels, false, headerless);

  if (is_sbr)
    packetizer->set_audio_output_sampling_freq(output_sample_rate);

  return packetizer;
}

generic_packetizer_c *
avi_reader_c::create_dts_packetizer(int aid) {
  try {
    AVI_set_audio_track(m_avi, aid);

    long audio_position   = AVI_get_audio_position_index(m_avi);
    unsigned int num_read = 0;
    int dts_position      = -1;
    byte_buffer_c buffer;
    dts_header_t dtsheader;

    while ((-1 == dts_position) && (10 > num_read)) {
      int chunk_size = AVI_read_audio_chunk(m_avi, nullptr);

      if (0 < chunk_size) {
        memory_cptr chunk = memory_c::alloc(chunk_size);
        AVI_read_audio_chunk(m_avi, reinterpret_cast<char *>(chunk->get_buffer()));

        buffer.add(chunk);
        dts_position = find_dts_header(buffer.get_buffer(), buffer.get_size(), &dtsheader);

      } else {
        dts_position = find_dts_header(buffer.get_buffer(), buffer.get_size(), &dtsheader, true);
        break;
      }
    }

    if (-1 == dts_position)
      throw false;

    AVI_set_audio_position_index(m_avi, audio_position);

    return new dts_packetizer_c(this, m_ti, dtsheader);

  } catch (...) {
    mxerror_tid(m_ti.m_fname, aid + 1, Y("Could not find valid DTS headers in this track's first frames.\n"));
    return nullptr;
  }
}

generic_packetizer_c *
avi_reader_c::create_vorbis_packetizer(int aid) {
  try {
    if (m_ti.m_private_data)
      throw mtx::input::extended_x(Y("Invalid Vorbis headers in AVI audio track."));

    auto c = m_ti.m_private_data->get_buffer();

    if (2 != c[0])
      throw mtx::input::extended_x(Y("Invalid Vorbis headers in AVI audio track."));

    int offset           = 1;
    const int laced_size = m_ti.m_private_data->get_size();
    int i;

    int header_sizes[3];
    unsigned char *headers[3];

    for (i = 0; 2 > i; ++i) {
      int size = 0;

      while ((offset < laced_size) && (255u == c[offset])) {
        size += 255;
        ++offset;
      }
      if ((laced_size - 1) <= offset)
        throw mtx::input::extended_x(Y("Invalid Vorbis headers in AVI audio track."));

      size            += c[offset];
      header_sizes[i]  = size;
      ++offset;
    }

    headers[0]        = &c[offset];
    headers[1]        = &c[offset + header_sizes[0]];
    headers[2]        = &c[offset + header_sizes[0] + header_sizes[1]];
    header_sizes[2]   = laced_size - offset - header_sizes[0] - header_sizes[1];

    m_ti.m_private_data.reset();

    return new vorbis_packetizer_c(this, m_ti, headers[0], header_sizes[0], headers[1], header_sizes[1], headers[2], header_sizes[2]);

  } catch (mtx::exception &e) {
    mxerror_tid(m_ti.m_fname, aid + 1, boost::format("%1%\n") % e.error());

    // Never reached, but make the compiler happy:
    return nullptr;
  }
}

void
avi_reader_c::set_avc_nal_size_size(mpeg4_p10_es_video_packetizer_c *ptzr) {
  m_avc_nal_size_size = ptzr->get_nalu_size_length();

  for (size_t i = 0; i < m_max_video_frames; ++i) {
    int size = AVI_frame_size(m_avi, i);
    if (0 == size)
      continue;

    memory_cptr buffer = memory_c::alloc(size);

    AVI_set_video_position(m_avi, i);
    int key = 0;
    size    = AVI_read_frame(m_avi, reinterpret_cast<char *>(buffer->get_buffer()), &key);

    if (   (4 <= size)
        && (   (get_uint32_be(buffer->get_buffer()) == NALU_START_CODE)
            || (get_uint24_be(buffer->get_buffer()) == NALU_START_CODE)))
      m_avc_nal_size_size = -1;

    break;
  }

  AVI_set_video_position(m_avi, 0);
}

file_status_e
avi_reader_c::read_video() {
  if (m_video_frames_read >= m_max_video_frames)
    return flush_packetizer(m_vptzr);

  memory_cptr chunk;
  int key                   = 0;
  int old_video_frames_read = m_video_frames_read;

  int size, num_read;

  int dropped_frames_here   = 0;

  do {
    size  = AVI_frame_size(m_avi, m_video_frames_read);
    chunk = memory_c::alloc(size);
    num_read = AVI_read_frame(m_avi, reinterpret_cast<char *>(chunk->get_buffer()), &key);

    ++m_video_frames_read;

    if (0 > num_read) {
      // Error reading the frame: abort
      m_video_frames_read = m_max_video_frames;
      return flush_packetizer(m_vptzr);

    } else if (0 == num_read)
      ++dropped_frames_here;

  } while ((0 == num_read) && (m_video_frames_read < m_max_video_frames));

  if (0 == num_read)
    // This is only the case if the AVI contains dropped frames only.
    return flush_packetizer(m_vptzr);

  size_t i;
  for (i = m_video_frames_read; i < m_max_video_frames; ++i) {
    if (0 != AVI_frame_size(m_avi, i))
      break;

    int dummy_key;
    AVI_read_frame(m_avi, nullptr, &dummy_key);
    ++dropped_frames_here;
    ++m_video_frames_read;
  }

  int64_t timestamp       = static_cast<int64_t>(static_cast<int64_t>(old_video_frames_read)   * 1000000000ll / m_fps);
  int64_t duration        = static_cast<int64_t>(static_cast<int64_t>(dropped_frames_here + 1) * 1000000000ll / m_fps);

  m_dropped_video_frames += dropped_frames_here;

  // AVC with framed packets (without NALU start codes but with length fields)
  // or non-AVC video track?
  if (0 >= m_avc_nal_size_size)
    PTZR(m_vptzr)->process(new packet_t(chunk, timestamp, duration, key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME));

  else {
    // AVC video track without NALU start codes. Re-frame with NALU start codes.
    int offset = 0;

    while ((offset + m_avc_nal_size_size) < num_read) {
      int nalu_size  = get_uint_be(chunk->get_buffer() + offset, m_avc_nal_size_size);
      offset        += m_avc_nal_size_size;

      if ((offset + nalu_size) > num_read)
        break;

      memory_cptr nalu = memory_c::alloc(4 + nalu_size);
      put_uint32_be(nalu->get_buffer(), NALU_START_CODE);
      memcpy(nalu->get_buffer() + 4, chunk->get_buffer() + offset, nalu_size);
      offset += nalu_size;

      PTZR(m_vptzr)->process(new packet_t(nalu, timestamp, duration, key ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME));
    }
  }

  m_bytes_processed += num_read;

  return m_video_frames_read >= m_max_video_frames ? flush_packetizer(m_vptzr) :  FILE_STATUS_MOREDATA;
}

file_status_e
avi_reader_c::read_audio(avi_demuxer_t &demuxer) {
  AVI_set_audio_track(m_avi, demuxer.m_aid);

  while (true) {
    int size = AVI_audio_size(m_avi, AVI_get_audio_position_index(m_avi));

    if (-1 == size)
      return flush_packetizer(demuxer.m_ptzr);

    if (!size) {
      AVI_set_audio_position_index(m_avi, AVI_get_audio_position_index(m_avi) + 1);
      continue;
    }

    auto chunk = memory_c::alloc(size);
    size       = AVI_read_audio_chunk(m_avi, reinterpret_cast<char *>(chunk->get_buffer()));

    if (0 >= size)
      continue;

    PTZR(demuxer.m_ptzr)->add_avi_block_size(size);
    PTZR(demuxer.m_ptzr)->process(new packet_t(chunk));

    m_bytes_processed += size;

    return AVI_get_audio_position_index(m_avi) < AVI_max_audio_chunk(m_avi) ? FILE_STATUS_MOREDATA : flush_packetizer(demuxer.m_ptzr);
  }
}

file_status_e
avi_reader_c::read_subtitles(avi_subs_demuxer_t &demuxer) {
  if (!demuxer.m_subs->empty())
    demuxer.m_subs->process(PTZR(demuxer.m_ptzr));

  return demuxer.m_subs->empty() ? flush_packetizer(demuxer.m_ptzr) : FILE_STATUS_MOREDATA;
}

file_status_e
avi_reader_c::read(generic_packetizer_c *ptzr,
                   bool) {
  if ((-1 != m_vptzr) && (PTZR(m_vptzr) == ptzr))
    return read_video();

  for (auto &demuxer : m_audio_demuxers)
    if ((-1 != demuxer.m_ptzr) && (PTZR(demuxer.m_ptzr) == ptzr))
      return read_audio(demuxer);

  for (auto &subs_demuxer : m_subtitle_demuxers)
    if ((-1 != subs_demuxer.m_ptzr) && (PTZR(subs_demuxer.m_ptzr) == ptzr))
      return read_subtitles(subs_demuxer);

  return flush_packetizers();
}

int
avi_reader_c::get_progress() {
  return 0 == m_bytes_to_process ? 0 : 100 * m_bytes_processed / m_bytes_to_process;
}

void
avi_reader_c::extended_identify_mpeg4_l2(std::vector<std::string> &extended_info) {
  int size = AVI_frame_size(m_avi, 0);
  if (0 >= size)
    return;

  memory_cptr af_buffer = memory_c::alloc(size);
  unsigned char *buffer = af_buffer->get_buffer();
  int dummy_key;

  AVI_read_frame(m_avi, reinterpret_cast<char *>(buffer), &dummy_key);

  uint32_t par_num, par_den;
  if (mpeg4::p2::extract_par(buffer, size, par_num, par_den)) {
    int width          = AVI_video_width(m_avi);
    int height         = AVI_video_height(m_avi);
    float aspect_ratio = static_cast<float>(width) * par_num / height / par_den;

    int disp_width, disp_height;
    if (aspect_ratio > (static_cast<float>(width) / height)) {
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
  id_result_container();
  identify_video();
  identify_audio();
  identify_subtitles();
  identify_attachments();
}

void
avi_reader_c::identify_video() {
  if (!m_video_track_ok)
    return;

  std::vector<std::string> extended_info;

  const char *fourcc_str = AVI_video_compressor(m_avi);
  std::string type       = fourcc_str;

  if (IS_MPEG4_L2_FOURCC(fourcc_str))
    extended_identify_mpeg4_l2(extended_info);

  else if (mpeg4::p10::is_avc_fourcc(fourcc_str))
    extended_info.push_back("packetizer:mpeg4_p10_es_video");

  else if (mpeg1_2::is_fourcc(get_uint32_le(fourcc_str)))
    type = "MPEG-1/2";

  else if (type == "VP80")
    type = "VP8";

  id_result_track(0, ID_RESULT_TRACK_VIDEO, type, join(" ", extended_info));
}

void
avi_reader_c::identify_audio() {
  int i;
  for (i = 0; i < AVI_audio_tracks(m_avi); i++) {
    AVI_set_audio_track(m_avi, i);
    unsigned int audio_format = AVI_audio_format(m_avi);
    alWAVEFORMATEX *wfe       = m_avi->wave_format_ex[i];
    if ((0xfffe == audio_format) && (get_uint16_le(&wfe->cb_size) >= (sizeof(alWAVEFORMATEXTENSION)))) {
      alWAVEFORMATEXTENSIBLE *ext = reinterpret_cast<alWAVEFORMATEXTENSIBLE *>(wfe);
      audio_format = get_uint32_le(&ext->extension.guid.data1);
    }

    std::string type
      = (0x0001 == audio_format) || (0x0003 == audio_format) ? "PCM"
      : (0x0050 == audio_format)                             ? "MP2"
      : (0x0055 == audio_format)                             ? "MP3"
      : (0x2000 == audio_format)                             ? "AC3"
      : (0x2001 == audio_format)                             ? "DTS"
      : (0x0050 == audio_format)                             ? "MP2"
      : (0x00ff == audio_format) || (0x706d == audio_format) ? "AAC"
      : (0x566f == audio_format)                             ? "Vorbis"
      :                                                        (boost::format("unsupported (0x%|1$04x|)") % audio_format).str();

    id_result_track(i + 1, ID_RESULT_TRACK_AUDIO, type);
  }
}

void
avi_reader_c::identify_subtitles() {
  size_t i;
  for (i = 0; m_subtitle_demuxers.size() > i; ++i)
    id_result_track(1 + AVI_audio_tracks(m_avi) + i, ID_RESULT_TRACK_SUBTITLES,
                      avi_subs_demuxer_t::TYPE_SRT == m_subtitle_demuxers[i].m_type ? "SRT"
                    : avi_subs_demuxer_t::TYPE_SSA == m_subtitle_demuxers[i].m_type ? "SSA/ASS"
                    :                                                                 "unknown");
}

void
avi_reader_c::identify_attachments() {
  size_t i;

  for (i = 0; m_subtitle_demuxers.size() > i; ++i) {
    try {
      avi_subs_demuxer_t &demuxer = m_subtitle_demuxers[i];
      mm_text_io_c text_io(new mm_mem_io_c(demuxer.m_subtitles->get_buffer(), demuxer.m_subtitles->get_size()));
      ssa_parser_c parser(this, &text_io, m_ti.m_fname, i + 1 + AVI_audio_tracks(m_avi));

      parser.set_attachment_id_base(g_attachments.size());
      parser.parse();

    } catch (...) {
    }
  }

  for (i = 0; i < g_attachments.size(); i++)
    id_result_attachment(g_attachments[i].ui_id, g_attachments[i].mime_type, g_attachments[i].data->get_size(), g_attachments[i].name, g_attachments[i].description);
}

void
avi_reader_c::add_available_track_ids() {
  // Yes, '>=' is correct. Don't forget the video track!
  add_available_track_id_range(AVI_audio_tracks(m_avi) + m_subtitle_demuxers.size() + 1);
}

void
avi_reader_c::debug_dump_video_index() {
  int num_video_frames = AVI_video_frames(m_avi), i;

  mxinfo(boost::format("AVI video index dump: %1% entries; frame rate: %2%\n") % num_video_frames % m_fps);
  for (i = 0; num_video_frames > i; ++i)
    mxinfo(boost::format("  %1%: %2% bytes\n") % i % AVI_frame_size(m_avi, i));
}
