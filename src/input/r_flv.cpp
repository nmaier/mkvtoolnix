/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Flash Video (FLV) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <avilib.h>

#include "common/amf.h"
#include "common/endian.h"
#include "common/matroska.h"
#include "common/mm_io_x.h"
#include "common/mpeg4_p10.h"
#include "input/r_flv.h"
#include "output/p_aac.h"
#include "output/p_mpeg4_p10.h"
#include "output/p_mp3.h"
#include "output/p_video.h"

#define FLV_DETECT_SIZE 1 * 1024 * 1024

flv_header_t::flv_header_t()
  : signature{ 0, 0, 0 }
  , version{0}
  , type_flags{0}
  , data_offset{0}
{
}

bool
flv_header_t::has_video()
  const {
  return (type_flags & 0x01) == 0x01;
}

bool
flv_header_t::has_audio()
  const {
  return (type_flags & 0x04) == 0x04;
}

bool
flv_header_t::is_valid()
  const {
  return std::string(signature, 3) == "FLV";
}

bool
flv_header_t::read(mm_io_cptr const &in) {
  return read(in.get());
}

bool
flv_header_t::read(mm_io_c *in) {
  if (sizeof(*this) != in->read(this, sizeof(*this)))
    return false;

  data_offset = get_uint32_be(&data_offset);

  return true;
}

// --------------------------------------------------

flv_tag_c::flv_tag_c()
  : m_previous_tag_size{}
  , m_flags{}
  , m_data_size{}
  , m_timecode{}
  , m_timecode_extended{}
  , m_next_position{}
  , m_ok{}
  , m_debug{debugging_requested("flv_full|flv_tags|flv_tag")}
{
}

bool
flv_tag_c::is_encrypted()
  const {
  return (m_flags & 0x20) == 0x20;
}

bool
flv_tag_c::is_audio()
  const {
  return 0x08 == m_flags;
}

bool
flv_tag_c::is_video()
  const {
  return 0x09 == m_flags;
}

bool
flv_tag_c::is_script_data()
  const {
  return 0x12 == m_flags;
}

bool
flv_tag_c::read(mm_io_cptr const &in) {
  try {
    auto position       = in->getFilePointer();
    m_ok                = false;
    m_previous_tag_size = in->read_uint32_be();
    m_flags             = in->read_uint8();
    m_data_size         = in->read_uint24_be();
    m_timecode          = in->read_uint24_be();
    m_timecode_extended = in->read_uint8();
    in->skip(3);
    m_next_position     = in->getFilePointer() + m_data_size;
    m_ok                = true;

    mxdebug_if(m_debug, boost::format("Tag @ %1%: %2%\n") % position % *this);

    return true;

  } catch (mtx::mm_io::exception &) {
    return false;
  }
}

// --------------------------------------------------

flv_track_c::flv_track_c(char type)
  : m_type{type}
  , m_headers_read{}
  , m_ptzr{-1}
  , m_timecode{}
  , m_v_version{}
  , m_v_width{}
  , m_v_height{}
  , m_v_dwidth{}
  , m_v_dheight{}
  , m_v_frame_rate{}
  , m_v_aspect_ratio{}
  , m_v_cts_offset{}
  , m_v_frame_type{}
  , m_a_channels{}
  , m_a_sample_rate{}
  , m_a_bits_per_sample{}
  , m_a_profile{}
{
}

bool
flv_track_c::is_audio()
  const {
  return 'a' == m_type;
}

bool
flv_track_c::is_video()
  const {
  return 'v' == m_type;
}

bool
flv_track_c::is_valid()
  const {
  return m_headers_read && !!m_fourcc;
}

bool
flv_track_c::is_ptzr_set()
  const {
  return -1 != m_ptzr;
}

// --------------------------------------------------

int
flv_reader_c::probe_file(mm_io_c *io,
                         uint64_t) {
  try {
    flv_header_t header;

    io->setFilePointer(0);
    return header.read(io) && header.is_valid() ? 1 : 0;

  } catch (...) {
    return 0;
  }
}

flv_reader_c::flv_reader_c(track_info_c const &ti,
                           mm_io_cptr const &in)
  : generic_reader_c{ti, in}
  , m_audio_track_idx{0}
  , m_video_track_idx{0}
  , m_selected_track_idx{-1}
  , m_file_done{false}
  , m_debug{debugging_requested("flv|flv_full")}
{
}

translatable_string_c
flv_reader_c::get_format_name()
  const {
  return YT("Flash Video");
}

unsigned int
flv_reader_c::add_track(char type) {
  m_tracks.push_back(flv_track_cptr{new flv_track_c(type)});
  return m_tracks.size() - 1;
}

void
flv_reader_c::read_headers() {
  flv_header_t header;
  header.read(m_in);

  mxdebug_if(m_debug, boost::format("Header dump: %1%\n") % header);

  if (header.has_video())
    m_video_track_idx = add_track('v');

  if (header.has_audio())
    m_audio_track_idx = add_track('a');

  bool headers_read = false;

  try {
    while (!headers_read && !m_file_done && (m_in->getFilePointer() < FLV_DETECT_SIZE)) {
      if (process_tag(true)) {
        headers_read = true;
        for (auto &track : m_tracks)
          if (!track->is_valid()) {
            headers_read = false;
            break;
          }
      }

      if (!m_tag.m_ok)
        break;
      m_in->setFilePointer(m_tag.m_next_position);
    }

  } catch (...) {
    throw mtx::input::invalid_format_x();
  }

  m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(), [](flv_track_cptr &t) { return !t->is_valid(); }),
                 m_tracks.end());

  m_audio_track_idx = -1;
  m_video_track_idx = -1;
  for (int idx = m_tracks.size(); idx > 0; --idx)
    if (m_tracks[idx - 1]->is_video())
      m_video_track_idx = idx - 1;
    else
      m_audio_track_idx = idx - 1;

  mxdebug_if(m_debug, boost::format("Detection finished at %1%; headers read? %2%; number valid tracks: %3%\n")
             % m_in->getFilePointer() % headers_read % m_tracks.size());

  m_in->setFilePointer(9, seek_beginning); // rewind file for later remux
  m_file_done = false;
}

flv_reader_c::~flv_reader_c() {
}

void
flv_reader_c::identify() {
  id_result_container();

  size_t idx = 0;
  for (auto track : m_tracks) {
    std::vector<std::string> verbose_info;
    if (track->m_fourcc.equiv("avc1"))
      verbose_info.push_back("packetizer:mpeg4_p10_video");

    id_result_track(idx, track->is_audio() ? ID_RESULT_TRACK_AUDIO : ID_RESULT_TRACK_VIDEO,
                      track->m_fourcc.equiv("AVC1") ? "AVC/h.264"
                    : track->m_fourcc.equiv("FLV1") ? "Sorenson h.263 (Flash version)"
                    : track->m_fourcc.equiv("VP6F") ? "On2 VP6 (Flash version)"
                    : track->m_fourcc.equiv("VP6A") ? "On2 VP6 (Flash version with alpha channel)"
                    : track->m_fourcc.equiv("AAC ") ? "AAC"
                    : track->m_fourcc.equiv("MP3 ") ? "MP3"
                    :                                 "Unknown",
                    verbose_info);
    ++idx;
  }
}

void
flv_reader_c::add_available_track_ids() {
  add_available_track_id_range(m_tracks.size());
}

void
flv_reader_c::create_packetizer(int64_t id) {
  if ((0 > id) || (m_tracks.size() <= static_cast<size_t>(id)) || m_tracks[id]->is_ptzr_set())
    return;

  auto &track = m_tracks[id];

  if (!demuxing_requested(track->m_type, id))
    return;

  m_ti.m_id = id;
  m_ti.m_private_data.reset();

  if (track->m_fourcc.equiv("AVC1"))
    create_v_avc_packetizer(track);

  else if (track->m_fourcc.equiv("FLV1") || track->m_fourcc.equiv("VP6F") || track->m_fourcc.equiv("VP6A"))
    create_v_generic_packetizer(track);

  else if (track->m_fourcc.equiv("AAC "))
    create_a_aac_packetizer(track);

  else if (track->m_fourcc.equiv("MP3 "))
    create_a_mp3_packetizer(track);
}

void
flv_reader_c::create_v_avc_packetizer(flv_track_cptr &track) {
  m_ti.m_private_data = track->m_private_data;
  track->m_ptzr       = add_packetizer(new mpeg4_p10_video_packetizer_c(this, m_ti, track->m_v_frame_rate, track->m_v_width, track->m_v_height));
  show_packetizer_info(m_video_track_idx, PTZR(track->m_ptzr));
}

void
flv_reader_c::create_v_generic_packetizer(flv_track_cptr &track) {
  alBITMAPINFOHEADER bih;

  memset(&bih, 0, sizeof(bih));
  put_uint32_le(&bih.bi_size,        sizeof(bih));
  put_uint32_le(&bih.bi_width,       track->m_v_width);
  put_uint32_le(&bih.bi_height,      track->m_v_height);
  put_uint16_le(&bih.bi_planes,      1);
  put_uint16_le(&bih.bi_bit_count,   24);
  put_uint32_le(&bih.bi_size_image,  track->m_v_width * track->m_v_height * 3);
  track->m_fourcc.write(reinterpret_cast<unsigned char *>(&bih.bi_compression));

  m_ti.m_private_data = memory_c::clone(&bih, sizeof(bih));

  track->m_ptzr = add_packetizer(new video_packetizer_c(this, m_ti, MKV_V_MSCOMP, track->m_v_frame_rate, track->m_v_width, track->m_v_height));
  show_packetizer_info(m_video_track_idx, PTZR(track->m_ptzr));
}

void
flv_reader_c::create_a_aac_packetizer(flv_track_cptr &track) {
  track->m_ptzr = add_packetizer(new aac_packetizer_c(this, m_ti, AAC_ID_MPEG4, track->m_a_profile, track->m_a_sample_rate, track->m_a_channels, false, true));
  show_packetizer_info(m_audio_track_idx, PTZR(track->m_ptzr));
}

void
flv_reader_c::create_a_mp3_packetizer(flv_track_cptr &track) {
  track->m_ptzr = add_packetizer(new mp3_packetizer_c(this, m_ti, track->m_a_sample_rate, track->m_a_channels, true));
  show_packetizer_info(m_audio_track_idx, PTZR(track->m_ptzr));
}

void
flv_reader_c::create_packetizers() {
  for (auto id = 0u; m_tracks.size() > id; ++id)
    create_packetizer(id);
}

bool
flv_reader_c::new_stream_v_avc(flv_track_cptr &track,
                               memory_cptr const &data) {
  try {
    auto avcc = mpeg4::p10::avcc_c::unpack(data);
    avcc.parse_sps_list(true);

    for (auto &sps_info : avcc.m_sps_info_list) {
      if (!track->m_v_width)
        track->m_v_width = sps_info.width;
      if (!track->m_v_height)
        track->m_v_height = sps_info.height;
      if (!track->m_v_frame_rate && sps_info.timing_info.num_units_in_tick && sps_info.timing_info.time_scale)
        track->m_v_frame_rate = sps_info.timing_info.time_scale / sps_info.timing_info.num_units_in_tick;
    }

    if (!track->m_v_frame_rate)
      track->m_v_frame_rate = 25;

  } catch (mtx::mm_io::exception &) {
  }

  mxdebug_if(m_debug, boost::format("new_stream_v_avc: video width: %1%, height: %2%, frame rate: %3%\n") % track->m_v_width % track->m_v_height % track->m_v_frame_rate);

  return true;
}

bool
flv_reader_c::process_audio_tag_sound_format(flv_track_cptr &track,
                                             uint8_t sound_format) {
  static const std::vector<std::string> s_formats{
      "Linear PCM platform endian"
    , "ADPCM"
    , "MP3"
    , "Linear PCM little endian"
    , "Nellymoser 16 kHz mono"
    , "Nellymoser 8 kHz mono"
    , "Nellymoser"
    , "G.711 A-law logarithmic PCM"
    , "G.711 mu-law logarithmic PCM"
    , ""
    , "AAC"
    , "Speex"
    , "MP3 8 kHz"
    , "Device-specific sound"
  };

  if (15 < sound_format) {
    mxdebug_if(m_debug, boost::format("Sound format: not handled (%1%)\n") % static_cast<unsigned int>(sound_format));
    return true;
  }

  mxdebug_if(m_debug, boost::format("Sound format: %1%\n") % s_formats[sound_format]);

  // AAC
  if (10 == sound_format) {
    if (!m_tag.m_data_size)
      return false;

    track->m_fourcc = "AAC ";
    uint8_t aac_packet_type = m_in->read_uint8();
    m_tag.m_data_size--;
    if (aac_packet_type != 0) {
      // Raw AAC
      mxdebug_if(m_debug, boost::format("  AAC sub type: raw\n"));
      return true;
    }

    auto size = std::min<size_t>(m_tag.m_data_size, 5);
    if (!size)
      return false;

    m_tag.m_data_size -= size;

    unsigned char specific_codec_buf[5];
    if (m_in->read(specific_codec_buf, size) != size)
       return false;

    int profile, channels, sample_rate, output_sample_rate;
    bool sbr;
    if (!parse_aac_data(specific_codec_buf, size, profile, channels, sample_rate, output_sample_rate, sbr))
      return false;

    mxdebug_if(m_debug, boost::format("  AAC sub type: sequence header (profile: %1%, channels: %2%, s_rate: %3%, out_s_rate: %4%, sbr %5%)\n") % profile % channels % sample_rate % output_sample_rate % sbr);
    if (sbr)
      profile = AAC_PROFILE_SBR;

    track->m_a_profile     = profile;
    track->m_a_channels    = channels;
    track->m_a_sample_rate = sample_rate;

    return true;
  }

  switch (sound_format) {
    case 2:  track->m_fourcc = "MP3 "; break;
    case 14: track->m_fourcc = "MP3 "; break;
    default:
      return false;
  }

  return true;
}

bool
flv_reader_c::process_audio_tag(flv_track_cptr &track) {
  uint8_t audiotag_header = m_in->read_uint8();
  uint8_t format          = (audiotag_header & 0xf0) >> 4;
  uint8_t rate            = (audiotag_header & 0x0c) >> 2;
  uint8_t size            = (audiotag_header & 0x02) >> 1;
  uint8_t type            =  audiotag_header & 0x01;

  mxdebug_if(m_debug, boost::format("Audio packet found\n"));

  if (!m_tag.m_data_size)
    return false;
  m_tag.m_data_size--;

  process_audio_tag_sound_format(track, format);

  if (!track->m_a_sample_rate && (4 > rate)) {
    static unsigned int s_rates[] = { 5512, 11025, 22050, 44100 };
    track->m_a_sample_rate = s_rates[rate];
  }

  if (!track->m_a_channels)
    track->m_a_channels = 0 == type ? 1 : 2;

  track->m_headers_read = true;

  mxdebug_if(m_debug,
             boost::format("  sampling frequency: %1%; sample size: %2%; channels: %3%\n")
             % track->m_a_sample_rate % (0 == size ? "8 bits" : "16 bits") % track->m_a_channels);

  return true;
}

bool
flv_reader_c::process_video_tag_avc(flv_track_cptr &track) {
  if (4 > m_tag.m_data_size)
    return false;

  track->m_fourcc          = "AVC1";
  uint8_t avc_packet_type  = m_in->read_uint8();
  track->m_v_cts_offset    = m_in->read_int24_be();
  m_tag.m_data_size       -= 4;

  // The CTS offset is only valid for NALUs.
  if (1 != avc_packet_type)
    track->m_v_cts_offset = 0;

  // Only sequence headers need more processing
  if (0 != avc_packet_type)
    return true;

  mxdebug_if(m_debug, boost::format("  AVC sequence header at %1%\n") % m_in->getFilePointer());

  auto data         = m_in->read(m_tag.m_data_size);
  m_tag.m_data_size = 0;

  if (!track->m_headers_read) {
    if (!new_stream_v_avc(track, data))
      return false;

    track->m_headers_read = true;
  }

  track->m_extra_data = data;
  if (!track->m_private_data)
    track->m_private_data = data->clone();

  return true;
}

bool
flv_reader_c::process_video_tag_generic(flv_track_cptr &track,
                                        flv_tag_c::codec_type_e codec_id) {
  track->m_fourcc       = flv_tag_c::CODEC_SORENSON_H263  == codec_id ? "FLV1"
                        : flv_tag_c::CODEC_VP6            == codec_id ? "VP6F"
                        : flv_tag_c::CODEC_VP6_WITH_ALPHA == codec_id ? "VP6A"
                        :                                               "BUG!";
  track->m_headers_read = true;

  if ((track->m_fourcc == "VP6A") || (track->m_fourcc == "VP6F")) {
    if (!m_tag.m_data_size)
      return false;
    m_tag.m_data_size--;
    m_in->skip(1);
  }

  return true;

}

bool
flv_reader_c::process_video_tag(flv_track_cptr &track) {
  static struct {
    std::string name;
    bool is_key;
  } s_frame_types[] = {
      { "key frame",                true  }
    , { "inter frame",              false }
    , { "disposable inter frame",   false }
    , { "generated key frame",      true  }
    , { "video info/command frame", false }
  };

  mxdebug_if(m_debug, boost::format("Video packet found\n"));

  if (!m_tag.m_data_size)
    return false;

  uint8_t video_tag_header = m_in->read_uint8();
  m_tag.m_data_size--;

  uint8_t frame_type = (video_tag_header >> 4) & 0x0f;

  if ((1 <= frame_type) && (5 >= frame_type)) {
    auto type = s_frame_types[frame_type - 1];
    mxdebug_if(m_debug, boost::format("  Frame type: %1%\n") % type.name);
    track->m_v_frame_type = type.is_key ? 'I' : 'P';
  } else {
    mxdebug_if(m_debug, boost::format("  Frame type unknown (%1%)\n") % static_cast<unsigned int>(frame_type));
    track->m_v_frame_type = 'P';
  }

  static std::string s_codecs[] {
      "Sorenson H.263"
    , "Screen video"
    , "On2 VP6"
    , "On2 VP6 with alpha channel"
    , "Screen video version 2"
    , "H.264"
  };

  auto codec_id = static_cast<flv_tag_c::codec_type_e>(video_tag_header & 0x0f);

  if ((flv_tag_c::CODEC_SORENSON_H263 <= codec_id) && (flv_tag_c::CODEC_H264 >= codec_id)) {
    mxdebug_if(m_debug, boost::format("  Codec type: %1%\n") % s_codecs[codec_id - flv_tag_c::CODEC_SORENSON_H263]);

    if (flv_tag_c::CODEC_H264 == codec_id)
      return process_video_tag_avc(track);

    else if (   (flv_tag_c::CODEC_SORENSON_H263  == codec_id)
             || (flv_tag_c::CODEC_VP6            == codec_id)
             || (flv_tag_c::CODEC_VP6_WITH_ALPHA == codec_id))
      return process_video_tag_generic(track, codec_id);

  } else
    mxdebug_if(m_debug, boost::format("  Codec type unknown (%1%)\n") % static_cast<unsigned int>(codec_id));

  track->m_headers_read = true;

  return true;
}

bool
flv_reader_c::process_script_tag() {
  if (!m_tag.m_data_size || (-1 == m_video_track_idx))
    return true;

  try {
    mtx::amf::script_parser_c parser{m_in->read(m_tag.m_data_size)};
    parser.parse();

    double const *number;

    if ((number = parser.get_meta_data_value<double>("framerate"))) {
      m_tracks[m_video_track_idx]->m_v_frame_rate = *number;
      mxdebug_if(m_debug, boost::format("Video frame rate from meta data: %1%\n") % *number);
    }

    if ((number = parser.get_meta_data_value<double>("width"))) {
      m_tracks[m_video_track_idx]->m_v_width = *number;
      mxdebug_if(m_debug, boost::format("Video width from meta data: %1%\n") % *number);
    }

    if ((number = parser.get_meta_data_value<double>("height"))) {
      m_tracks[m_video_track_idx]->m_v_height = *number;
      mxdebug_if(m_debug, boost::format("Video height from meta data: %1%\n") % *number);
    }

  } catch (mtx::mm_io::exception &) {
  }

  return true;
}

bool
flv_reader_c::process_tag(bool skip_payload) {
  m_selected_track_idx = -1;

  if (!m_tag.read(m_in)) {
    m_file_done = true;
    return false;
  }

  if (m_tag.is_encrypted())
    return false;

  if (m_tag.is_script_data())
    return process_script_tag();

  if (m_tag.is_audio())
    m_selected_track_idx = m_audio_track_idx;
  else if (m_tag.is_video())
    m_selected_track_idx = m_video_track_idx;
  else
    return false;

  if ((0 > m_selected_track_idx) || (static_cast<int>(m_tracks.size()) <= m_selected_track_idx))
    return false;

  auto &track = m_tracks[m_selected_track_idx];

  if (m_tag.is_audio() && !process_audio_tag(track))
    return false;

  else if (m_tag.is_video() && !process_video_tag(track))
    return false;

  track->m_timecode = m_tag.m_timecode + (m_tag.m_timecode_extended << 24);

  mxdebug_if(m_debug, boost::format("Data size after processing: %1%; timecode in ms: %2%\n") % m_tag.m_data_size % track->m_timecode);

  if (!m_tag.m_data_size || skip_payload)
    return true;

  track->m_payload = m_in->read(m_tag.m_data_size);

  return true;
}

file_status_e
flv_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (m_file_done || m_in->eof())
    return flush_packetizers();

  bool tag_processed = false;
  try {
    tag_processed = process_tag();
  } catch (mtx::mm_io::exception &ex) {
    mxdebug_if(m_debug, boost::format("Exception: %1%\n") % ex);
    m_tag.m_ok = false;
  }

  if (!tag_processed) {
    if (m_tag.m_ok && m_in->setFilePointer2(m_tag.m_next_position))
      return FILE_STATUS_MOREDATA;

    m_file_done = true;
    return flush_packetizers();
  }

  if (-1 == m_selected_track_idx)
    return FILE_STATUS_MOREDATA;

  auto &track = m_tracks[m_selected_track_idx];

  if (!track->m_payload)
    return FILE_STATUS_MOREDATA;

  if (-1 != track->m_ptzr) {
    track->m_timecode = (track->m_timecode + track->m_v_cts_offset) * 1000000ll;
    mxdebug_if(m_debug, boost::format(" PTS in nanoseconds: %1%\n") % track->m_timecode);

    int64_t duration = -1;
    if (track->m_v_frame_rate && track->m_fourcc.equiv("AVC1"))
      duration = 1000000000ll / track->m_v_frame_rate;

    auto packet = new packet_t(track->m_payload, track->m_timecode, duration, 'I' == track->m_v_frame_type ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC, VFT_NOBFRAME);

    if (track->m_extra_data)
      packet->codec_state = track->m_extra_data;

    PTZR(track->m_ptzr)->process(packet);
  }

  track->m_payload.reset();
  track->m_extra_data.reset();

  if (m_in->setFilePointer2(m_tag.m_next_position))
    return FILE_STATUS_MOREDATA;

  m_file_done = true;
  return flush_packetizers();
}
