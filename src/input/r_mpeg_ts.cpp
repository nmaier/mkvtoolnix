/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG TS (transport stream) demultiplexer module

   Written by Massimo Callegari <massimocallegari@yahoo.it>
   and Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <iostream>

#include "common/checksums.h"
#include "common/clpi.h"
#include "common/endian.h"
#include "common/math.h"
#include "common/mp3.h"
#include "common/mm_mpls_multi_file_io.h"
#include "common/ac3.h"
#include "common/iso639.h"
#include "common/truehd.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "common/strings/formatting.h"
#include "input/r_mpeg_ts.h"
#include "output/p_aac.h"
#include "output/p_ac3.h"
#include "output/p_avc.h"
#include "output/p_dts.h"
#include "output/p_mp3.h"
#include "output/p_mpeg1_2.h"
#include "output/p_pgs.h"
#include "output/p_truehd.h"
#include "output/p_vc1.h"

#define TS_CONSECUTIVE_PACKETS 16
#define TS_PROBE_SIZE          (2 * TS_CONSECUTIVE_PACKETS * 204)
#define TS_PIDS_DETECT_SIZE    10 * 1024 * 1024
#define TS_PACKET_SIZE         188
#define TS_MAX_PACKET_SIZE     204

int mpeg_ts_reader_c::potential_packet_sizes[] = { 188, 192, 204, 0 };

// ------------------------------------------------------------

void
mpeg_ts_track_c::send_to_packetizer() {
  auto timecode_to_use = !m_timecode.valid()                                             ? timecode_c{}
                       : reader.m_dont_use_audio_pts && (ES_AUDIO_TYPE == type)          ? timecode_c{}
                       : m_apply_dts_timecode_fix && (m_previous_timecode == m_timecode) ? timecode_c{}
                       :                                                                   std::max(m_timecode, reader.m_global_timecode_offset);

  auto timecode_to_check = timecode_to_use.valid() ? timecode_to_use : reader.m_stream_timecode;
  auto const &min        = reader.get_timecode_restriction_min();
  auto const &max        = reader.get_timecode_restriction_max();
  auto use_packet        = ptzr != -1;

  if (   (min.valid() && (timecode_to_check < min))
      || (max.valid() && (timecode_to_check > max)))
    use_packet = false;

  if (timecode_to_use.valid()) {
    reader.m_stream_timecode  = timecode_to_use;
    timecode_to_use          -= std::max(reader.m_global_timecode_offset, min.valid() ? min : timecode_c::ns(0));
  }

  mxdebug_if(m_debug_delivery, boost::format("send_to_packetizer() PID %1% expected %2% actual %3% timecode_to_use %4% m_previous_timecode %5%\n")
             % pid % pes_payload_size % pes_payload->get_size() % timecode_to_use % m_previous_timecode);

  if (use_packet)
    reader.m_reader_packetizers[ptzr]->process(new packet_t(memory_c::clone(pes_payload->get_buffer(), pes_payload->get_size()), timecode_to_use.to_ns(-1)));

  pes_payload->remove(pes_payload->get_size());
  processed                          = false;
  data_ready                         = false;
  pes_payload_size                   = 0;
  reader.m_packet_sent_to_packetizer = true;
  m_previous_timecode                = m_timecode;
  m_timecode.reset();
}

void
mpeg_ts_track_c::add_pes_payload(unsigned char *ts_payload,
                                 size_t ts_payload_size) {
  pes_payload->add(ts_payload, ts_payload_size);

  // if (pid == 0x90f) {
  //   static int the_unicorn = 0;
  //   uint32_t cccsum = 0;
  //   unsigned char *cccidx;
  //   for (cccidx = ts_payload; (cccidx - ts_payload) < ts_payload_size; ++cccidx)
  //     cccsum += *cccidx;
  //   mxinfo(boost::format("  adding payload pnum %1% size %2% sum %|3$08x|\n") % the_unicorn % ts_payload_size % cccsum);
  //   the_unicorn++;
  // }
}

void
mpeg_ts_track_c::add_pes_payload_to_probe_data() {
  if (!m_probe_data)
    m_probe_data = byte_buffer_cptr(new byte_buffer_c);
  m_probe_data->add(pes_payload->get_buffer(), pes_payload->get_size());
}

int
mpeg_ts_track_c::new_stream_v_mpeg_1_2() {
  if (!m_m2v_parser) {
    m_m2v_parser = std::shared_ptr<M2VParser>(new M2VParser);
    m_m2v_parser->SetProbeMode();
  }

  m_m2v_parser->WriteData(pes_payload->get_buffer(), pes_payload->get_size());

  int state = m_m2v_parser->GetState();
  if (state != MPV_PARSER_STATE_FRAME) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: no valid frame in %1% bytes\n") % pes_payload->get_size());
    return FILE_STATUS_MOREDATA;
  }

  MPEG2SequenceHeader seq_hdr = m_m2v_parser->GetSequenceHeader();
  std::shared_ptr<MPEGFrame> frame(m_m2v_parser->ReadFrame());
  if (!frame)
    return FILE_STATUS_MOREDATA;

  codec          = codec_c::look_up(CT_V_MPEG12);
  v_interlaced   = !seq_hdr.progressiveSequence;
  v_version      = m_m2v_parser->GetMPEGVersion();
  v_width        = seq_hdr.width;
  v_height       = seq_hdr.height;
  v_frame_rate   = seq_hdr.frameOrFieldRate;
  v_aspect_ratio = seq_hdr.aspectRatio;

  if ((0 >= v_aspect_ratio) || (1 == v_aspect_ratio))
    v_dwidth = v_width;
  else
    v_dwidth = (int)(v_height * v_aspect_ratio);
  v_dheight  = v_height;

  MPEGChunk *raw_seq_hdr_chunk = m_m2v_parser->GetRealSequenceHeader();
  if (raw_seq_hdr_chunk) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: sequence header size: %1%\n") % raw_seq_hdr_chunk->GetSize());
    raw_seq_hdr = memory_c::clone(raw_seq_hdr_chunk->GetPointer(), raw_seq_hdr_chunk->GetSize());
  }

  mxverb(3, boost::format("new_stream_v_mpeg_1_2: width: %1%, height: %2%\n") % v_width % v_height);
  if (v_width == 0 || v_height == 0)
    return FILE_STATUS_MOREDATA;

  return 0;
}

int
mpeg_ts_track_c::new_stream_v_avc() {
  if (!m_avc_parser) {
    m_avc_parser = mpeg4::p10::avc_es_parser_cptr(new mpeg4::p10::avc_es_parser_c);
    m_avc_parser->ignore_nalu_size_length_errors();

    if (map_has_key(reader.m_ti.m_nalu_size_lengths, reader.tracks.size()))
      m_avc_parser->set_nalu_size_length(reader.m_ti.m_nalu_size_lengths[0]);
    else if (map_has_key(reader.m_ti.m_nalu_size_lengths, -1))
      m_avc_parser->set_nalu_size_length(reader.m_ti.m_nalu_size_lengths[-1]);
  }

  mxverb(3, boost::format("new_stream_v_avc: packet size: %1%\n") % pes_payload->get_size());

  m_avc_parser->add_bytes(pes_payload->get_buffer(), pes_payload->get_size());

  if (!m_avc_parser->headers_parsed())
    return FILE_STATUS_MOREDATA;

  codec    = codec_c::look_up(CT_V_MPEG4_P10);
  v_width  = m_avc_parser->get_width();
  v_height = m_avc_parser->get_height();

  if (m_avc_parser->has_par_been_found()) {
    auto dimensions = m_avc_parser->get_display_dimensions();
    v_dwidth        = dimensions.first;
    v_dheight       = dimensions.second;
  }

  return 0;
}

int
mpeg_ts_track_c::new_stream_v_vc1() {
  return 0 == pes_payload->get_size() ? FILE_STATUS_MOREDATA : 0;
}

int
mpeg_ts_track_c::new_stream_a_mpeg() {
  add_pes_payload_to_probe_data();

  mp3_header_t header;

  int offset = find_mp3_header(m_probe_data->get_buffer(), m_probe_data->get_size());
  if (-1 == offset)
    return FILE_STATUS_MOREDATA;

  decode_mp3_header(m_probe_data->get_buffer() + offset, &header);
  a_channels    = header.channels;
  a_sample_rate = header.sampling_frequency;
  codec         = codec_c::look_up(CT_A_MP3);

  mxverb(3, boost::format("new_stream_a_mpeg: Channels: %1%, sample rate: %2%\n") %a_channels % a_sample_rate);
  return 0;
}

int
mpeg_ts_track_c::new_stream_a_aac() {
  add_pes_payload_to_probe_data();

  if (0 > find_aac_header(m_probe_data->get_buffer(), m_probe_data->get_size(), &m_aac_header, false))
    return FILE_STATUS_MOREDATA;

  mxdebug_if(reader.m_debug_aac, boost::format("first AAC header: %1%\n") % m_aac_header.to_string());

  a_channels    = m_aac_header.channels;
  a_sample_rate = m_aac_header.sample_rate;

  return 0;
}

int
mpeg_ts_track_c::new_stream_a_ac3() {
  add_pes_payload_to_probe_data();

  ac3::parser_c parser;
  parser.add_bytes(m_probe_data->get_buffer(), m_probe_data->get_size());
  if (!parser.frame_available())
    return FILE_STATUS_MOREDATA;

  ac3::frame_c header = parser.get_frame();

  mxverb(2,
         boost::format("first ac3 header bsid %1% channels %2% sample_rate %3% bytes %4% samples %5%\n")
         % header.m_bs_id % header.m_channels % header.m_sample_rate % header.m_bytes % header.m_samples);

  a_channels    = header.m_channels;
  a_sample_rate = header.m_sample_rate;
  a_bsid        = header.m_bs_id;

  return 0;
}

int
mpeg_ts_track_c::new_stream_a_dts() {
  add_pes_payload_to_probe_data();

  if (-1 == find_dts_header(m_probe_data->get_buffer(), m_probe_data->get_size(), &a_dts_header))
    return FILE_STATUS_MOREDATA;

  m_apply_dts_timecode_fix = true;

  return 0;
}

int
mpeg_ts_track_c::new_stream_a_truehd() {
  if (!m_truehd_parser)
    m_truehd_parser = truehd_parser_cptr(new truehd_parser_c);

  static int added = 0;
  added += pes_payload->get_size();

  m_truehd_parser->add_data(pes_payload->get_buffer(), pes_payload->get_size());
  pes_payload->remove(pes_payload->get_size());

  while (m_truehd_parser->frame_available()) {
    truehd_frame_cptr frame = m_truehd_parser->get_next_frame();
    if (truehd_frame_t::sync != frame->m_type)
      continue;

    mxverb(2,
           boost::format("first TrueHD header channels %1% sampling_rate %2% samples_per_frame %3%\n")
           % frame->m_channels % frame->m_sampling_rate % frame->m_samples_per_frame);

    a_channels    = frame->m_channels;
    a_sample_rate = frame->m_sampling_rate;

    return 0;
  }

  return FILE_STATUS_MOREDATA;
}

void
mpeg_ts_track_c::set_pid(uint16_t new_pid) {
  pid = new_pid;

  std::string arg;
  m_debug_delivery = debugging_c::requested("mpeg_ts")
                   || (   debugging_c::requested("mpeg_ts_delivery", &arg)
                      && (arg.empty() || (arg == to_string(pid))));

  m_debug_timecode_wrapping = debugging_c::requested("mpeg_ts")
                            || (   debugging_c::requested("mpeg_ts_timecode_wrapping", &arg)
                               && (arg.empty() || (arg == to_string(pid))));
}

bool
mpeg_ts_track_c::detect_timecode_wrap(timecode_c &timecode) {
  static auto const s_wrap_limit = timecode_c::mpeg(1 << 30);

  if (!timecode.valid() || !m_previous_valid_timecode.valid() || ((timecode - m_previous_valid_timecode).abs() < s_wrap_limit))
    return false;
  return true;
}

void
mpeg_ts_track_c::adjust_timecode_for_wrap(timecode_c &timecode) {
  static auto const s_wrap_limit = timecode_c::mpeg(1ll << 30);
  static auto const s_bad_limit  = timecode_c::m(5);

  if (!timecode.valid())
    return;

  if (timecode < s_wrap_limit)
    timecode += m_timecode_wrap_add;

  if ((timecode - m_previous_valid_timecode).abs() >= s_bad_limit)
    timecode = timecode_c{};
}

void
mpeg_ts_track_c::handle_timecode_wrap(timecode_c &pts,
                                      timecode_c &dts) {
  static auto const s_wrap_add    = timecode_c::mpeg(1ll << 33);
  static auto const s_wrap_limit  = timecode_c::mpeg(1ll << 30);
  static auto const s_reset_limit = timecode_c::h(1);

  if (!m_timecodes_wrapped) {
    m_timecodes_wrapped = detect_timecode_wrap(pts) || detect_timecode_wrap(dts);
    if (m_timecodes_wrapped) {
      m_timecode_wrap_add += s_wrap_add;
      mxdebug_if(m_debug_timecode_wrapping,
                 boost::format("Timecode wrapping detected for PID %1% pts %2% dts %3% previous_valid %4% global_offset %5% new wrap_add %6%\n")
                 % pid % pts % dts % m_previous_valid_timecode % reader.m_global_timecode_offset % m_timecode_wrap_add);

    }

  } else if (pts.valid() && (pts < s_wrap_limit) && (pts > s_reset_limit)) {
    m_timecodes_wrapped = false;
    mxdebug_if(m_debug_timecode_wrapping,
               boost::format("Timecode wrapping reset for PID %1% pts %2% dts %3% previous_valid %4% global_offset %5% current wrap_add %6%\n")
               % pid % pts % dts % m_previous_valid_timecode % reader.m_global_timecode_offset % m_timecode_wrap_add);
  }

  adjust_timecode_for_wrap(pts);
  adjust_timecode_for_wrap(dts);

  // mxinfo(boost::format("pid %5% PTS before %1% now %2% wrapped %3% reset prevvalid %4% diff %6%\n") % before % pts % m_timecodes_wrapped % m_previous_valid_timecode % pid % (pts - m_previous_valid_timecode));
}

// ------------------------------------------------------------

bool
mpeg_ts_reader_c::probe_file(mm_io_c *in,
                             uint64_t) {
  auto mpls_in   = dynamic_cast<mm_mpls_multi_file_io_c *>(in);
  auto in_to_use = mpls_in ? static_cast<mm_io_c *>(mpls_in) : in;
  bool result    = -1 != detect_packet_size(in_to_use, in_to_use->get_size());

  return result;
}

int
mpeg_ts_reader_c::detect_packet_size(mm_io_c *in,
                                     uint64_t size) {
  try {
    size        = std::min(static_cast<uint64_t>(TS_PROBE_SIZE), size);
    auto buffer = memory_c::alloc(size);
    auto mem    = buffer->get_buffer();

    in->setFilePointer(0, seek_beginning);
    size = in->read(mem, size);

    std::vector<int> positions;
    for (size_t i = 0; i < size; ++i)
      if (0x47 == mem[i])
        positions.push_back(i);

    for (size_t i = 0; positions.size() > i; ++i) {
      for (size_t k = 0; 0 != potential_packet_sizes[k]; ++k) {
        unsigned int pos            = positions[i];
        unsigned int packet_size    = potential_packet_sizes[k];
        unsigned int num_startcodes = 1;

        while ((TS_CONSECUTIVE_PACKETS > num_startcodes) && (pos < size) && (0x47 == mem[pos])) {
          pos += packet_size;
          ++num_startcodes;
        }

        if (TS_CONSECUTIVE_PACKETS <= num_startcodes)
          return packet_size;
      }
    }
  } catch (...) {
  }

  return -1;
}

mpeg_ts_reader_c::mpeg_ts_reader_c(const track_info_c &ti,
                                   const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , PAT_found{}
  , PMT_found{}
  , PMT_pid(-1)
  , es_to_process{}
  , m_global_timecode_offset{}
  , m_stream_timecode{timecode_c::ns(0)}
  , input_status(INPUT_PROBE)
  , track_buffer_ready(-1)
  , file_done{}
  , m_packet_sent_to_packetizer{}
  , m_dont_use_audio_pts{     "mpeg_ts|mpeg_ts_dont_use_audio_pts"}
  , m_debug_resync{           "mpeg_ts|mpeg_ts_resync"}
  , m_debug_pat_pmt{          "mpeg_ts|mpeg_ts_pat|mpeg_ts_pmt"}
  , m_debug_aac{              "mpeg_ts|mpeg_aac"}
  , m_debug_timecode_wrapping{"mpeg_ts|mpeg_ts_timecode_wrapping"}
  , m_debug_clpi{             "clpi"}
  , m_detected_packet_size{}
{
  auto mpls_in = dynamic_cast<mm_mpls_multi_file_io_c *>(get_underlying_input());
  if (mpls_in)
    m_chapter_timecodes = mpls_in->get_chapters();
}

void
mpeg_ts_reader_c::read_headers() {
  try {
    size_t size_to_probe   = std::min(m_size, static_cast<uint64_t>(TS_PIDS_DETECT_SIZE));
    auto probe_buffer      = memory_c::alloc(size_to_probe);

    m_detected_packet_size = detect_packet_size(m_in.get(), size_to_probe);
    m_in->setFilePointer(0);

    mxverb(3, boost::format("mpeg_ts: Starting to build PID list. (packet size: %1%)\n") % m_detected_packet_size);

    mpeg_ts_track_ptr PAT(new mpeg_ts_track_c(*this));
    PAT->type = PAT_TYPE;
    tracks.push_back(PAT);

    unsigned char buf[TS_MAX_PACKET_SIZE]; // maximum TS packet size + 1

    bool done = m_in->eof();
    while (!done) {
      if (m_in->read(buf, m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
        break;

      if (buf[0] != 0x47) {
        if (resync(m_in->getFilePointer() - m_detected_packet_size))
          continue;
        break;
      }

      parse_packet(buf);
      done  = PAT_found && PMT_found && (0 == es_to_process);
      done |= m_in->eof() || (m_in->getFilePointer() >= TS_PIDS_DETECT_SIZE);
    }
  } catch (...) {
  }
  mxverb(3, boost::format("mpeg_ts: Detection done on %1% bytes\n") % m_in->getFilePointer());

  m_in->setFilePointer(0, seek_beginning); // rewind file for later remux

  for (auto &track : tracks) {
    track->pes_payload->remove(track->pes_payload->get_size());
    track->processed        = false;
    track->data_ready       = false;
    track->pes_payload_size = 0;
    // track->timecode_offset = -1;
  }

  parse_clip_info_file();
  process_chapter_entries();

  show_demuxer_info();
}

void
mpeg_ts_reader_c::process_chapter_entries() {
  if (m_chapter_timecodes.empty() || m_ti.m_no_chapters)
    return;

  std::stable_sort(m_chapter_timecodes.begin(), m_chapter_timecodes.end());

  mm_mem_io_c out{nullptr, 0, 1000};
  out.set_file_name(m_ti.m_fname);
  out.write_bom("UTF-8");

  size_t idx = 0;
  for (auto &timecode : m_chapter_timecodes) {
    ++idx;
    auto ms = timecode.to_ms();
    out.puts(boost::format("CHAPTER%|1$02d|=%|2$02d|:%|3$02d|:%|4$02d|.%|5$03d|\n"
                           "CHAPTER%|1$02d|NAME=Chapter %1%\n")
             % idx
             % ( ms / 60 / 60 / 1000)
             % ((ms      / 60 / 1000) %   60)
             % ((ms           / 1000) %   60)
             % ( ms                   % 1000));
  }

  mm_text_io_c text_out(&out, false);
  try {
    m_chapters = parse_chapters(&text_out, 0, -1, 0, m_ti.m_chapter_language, "", true);
    align_chapter_edition_uids(m_chapters.get());

  } catch (mtx::chapter_parser_x &ex) {
  }
}

mpeg_ts_reader_c::~mpeg_ts_reader_c() {
}

void
mpeg_ts_reader_c::identify() {
  std::vector<std::string> verbose_info;
  auto mpls_in = dynamic_cast<mm_mpls_multi_file_io_c *>(get_underlying_input());
  if (mpls_in)
    mpls_in->create_verbose_identification_info(verbose_info);

  id_result_container(verbose_info);

  size_t i;
  for (i = 0; i < tracks.size(); i++) {
    mpeg_ts_track_ptr &track = tracks[i];

    if (!track->probed_ok || !track->codec)
      continue;

    verbose_info.clear();
    if (!track->language.empty())
      verbose_info.push_back((boost::format("language:%1%") % escape(track->language)).str());

    verbose_info.push_back((boost::format("ts_pid:%1%") % track->pid).str());

    std::string type = ES_AUDIO_TYPE == track->type ? ID_RESULT_TRACK_AUDIO
                     : ES_VIDEO_TYPE == track->type ? ID_RESULT_TRACK_VIDEO
                     :                                ID_RESULT_TRACK_SUBTITLES;

    id_result_track(i, type, track->codec.get_name(), verbose_info);
  }

  if (!m_chapter_timecodes.empty())
    id_result_chapters(m_chapter_timecodes.size());
}

int
mpeg_ts_reader_c::parse_pat(unsigned char *pat) {
  if (!pat) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid parameters!\n");
    return -1;
  }

  mpeg_ts_pat_t *pat_header = (mpeg_ts_pat_t *)pat;

  if (pat_header->table_id != 0x00) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid PAT table_id!\n");
    return -1;
  }

  if (pat_header->get_section_syntax_indicator() != 1 || pat_header->get_current_next_indicator() == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid PAT section_syntax_indicator/current_next_indicator!\n");
    return -1;
  }

  if (pat_header->section_number != 0 || pat_header->last_section_number != 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Unsupported multiple section PAT!\n");
    return -1;
  }

  unsigned short pat_section_length = pat_header->get_section_length();
  uint32_t elapsed_CRC              = crc_calc_mpeg2(pat, 3 + pat_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = get_uint32_be(pat + 3 + pat_section_length - 4);

  if (elapsed_CRC != read_CRC) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pat: Wrong PAT CRC !!! Elapsed = 0x%|1$08x|, read 0x%|2$08x|\n") % elapsed_CRC % read_CRC);
    return -1;
  }

  if (pat_section_length < 13 || pat_section_length > 1021) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pat: Wrong PAT section_length (= %1%)\n") % pat_section_length);
    return -1;
  }

  unsigned int prog_count = (pat_section_length - 5 - 4/*CRC32*/) / 4;

  unsigned int i                     = 0;
  mpeg_ts_pat_section_t *pat_section = (mpeg_ts_pat_section_t *)(pat + sizeof(mpeg_ts_pat_t));
  for (; i < prog_count; i++, pat_section++) {
    unsigned short local_program_number = pat_section->get_program_number();
    uint16_t tmp_pid                    = pat_section->get_pid();

    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pat: program_number: %1%; %2%_pid: %3%\n")
               % local_program_number
               % (0 == local_program_number ? "nit" : "pmt")
               % tmp_pid);

    if (0 != local_program_number) {
      PAT_found = true;

      bool skip = false;
      for (uint16_t i = 0; i < tracks.size(); i++) {
        if (tracks[i]->pid == tmp_pid) {
          skip = true;
          break;
        }
      }

      if (skip == true)
        continue;

      mpeg_ts_track_ptr PMT(new mpeg_ts_track_c(*this));
      PMT->type       = PMT_TYPE;
      PMT->processed  = false;
      PMT->data_ready = false;
      es_to_process   = 0;

      PMT->set_pid(tmp_pid);

      tracks.push_back(PMT);
    }
  }

  return 0;
}

int
mpeg_ts_reader_c::parse_pmt(unsigned char *pmt) {
  if (!pmt) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid parameters!\n");
    return -1;
  }

  mpeg_ts_pmt_t *pmt_header = (mpeg_ts_pmt_t *)pmt;

  if (pmt_header->table_id != 0x02) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid PMT table_id!\n");
    return -1;
  }

  if (pmt_header->get_section_syntax_indicator() != 1 || pmt_header->get_current_next_indicator() == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid PMT section_syntax_indicator/current_next_indicator!\n");
    return -1;
  }

  if (pmt_header->section_number != 0 || pmt_header->last_section_number != 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Unsupported multiple section PMT!\n");
    return -1;
  }

  unsigned short pmt_section_length = pmt_header->get_section_length();
  uint32_t elapsed_CRC              = crc_calc_mpeg2(pmt, 3 + pmt_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = get_uint32_be(pmt + 3 + pmt_section_length - 4);

  if (elapsed_CRC != read_CRC) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Wrong PMT CRC !!! Elapsed = 0x%|1$08x|, read 0x%|2$08x|\n") % elapsed_CRC % read_CRC);
    return -1;
  }

  if (pmt_section_length < 13 || pmt_section_length > 1021) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Wrong PMT section_length (=%1%)\n") % pmt_section_length);
    return -1;
  }

  mpeg_ts_pmt_descriptor_t *pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t));
  unsigned short program_info_length       = pmt_header->get_program_info_length();

  while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t) + program_info_length))
    pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_descriptor + sizeof(mpeg_ts_pmt_descriptor_t) + pmt_descriptor->length);

  mpeg_ts_pmt_pid_info_t *pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  // Calculate pids_count
  size_t pids_found = 0;
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    pids_found++;
    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + pmt_pid_info->get_es_info_length());
  }

  mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: program number     (%1%)\n") % pmt_header->get_program_number());
  mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: pcr pid            (%1%)\n") % pmt_header->get_pcr_pid());

  if (pids_found == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: There's no information about elementary PIDs\n");
    return 0;
  }

  pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  // Extract pid_info
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    mpeg_ts_track_ptr track(new mpeg_ts_track_c(*this));
    unsigned short es_info_length = pmt_pid_info->get_es_info_length();
    track->type                   = ES_UNKNOWN;

    track->set_pid(pmt_pid_info->get_pid());

    switch(pmt_pid_info->stream_type) {
      case ISO_11172_VIDEO:
      case ISO_13818_VIDEO:
        track->type      = ES_VIDEO_TYPE;
        track->codec     = codec_c::look_up(CT_V_MPEG12);
        break;
      case ISO_14496_PART2_VIDEO:
        track->type      = ES_VIDEO_TYPE;
        track->codec     = codec_c::look_up(CT_V_MPEG4_P2);
        break;
      case ISO_14496_PART10_VIDEO:
        track->type      = ES_VIDEO_TYPE;
        track->codec     = codec_c::look_up(CT_V_MPEG4_P10);
        break;
      case STREAM_VIDEO_VC1:
        track->type      = ES_VIDEO_TYPE;
        track->codec     = codec_c::look_up(CT_V_VC1);
        break;
      case ISO_11172_AUDIO:
      case ISO_13818_AUDIO:
        track->type      = ES_AUDIO_TYPE;
        track->codec     = codec_c::look_up(CT_A_MP3);
        break;
      case ISO_13818_PART7_AUDIO:
      case ISO_14496_PART3_AUDIO:
        track->type      = ES_AUDIO_TYPE;
        track->codec     = codec_c::look_up(CT_A_AAC);
        break;
      case STREAM_AUDIO_AC3:
      case STREAM_AUDIO_AC3_PLUS: // EAC3
        track->type      = ES_AUDIO_TYPE;
        track->codec     = codec_c::look_up(CT_A_AC3);
        break;
      case STREAM_AUDIO_AC3_LOSSLESS:
        track->type      = ES_AUDIO_TYPE;
        track->codec     = codec_c::look_up(CT_A_TRUEHD);
        break;
      case STREAM_AUDIO_DTS:
      case STREAM_AUDIO_DTS_HD:
      case STREAM_AUDIO_DTS_HD_MA:
        track->type      = ES_AUDIO_TYPE;
        track->codec     = codec_c::look_up(CT_A_DTS);
        break;
      case STREAM_SUBTITLES_HDMV_PGS:
        track->type      = ES_SUBT_TYPE;
        track->codec     = codec_c::look_up(CT_S_PGS);
        track->probed_ok = true;
        break;
      case ISO_13818_PES_PRIVATE:
        break;
      default:
        mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Unknown stream type: %1%\n") % (int)pmt_pid_info->stream_type);
        track->type      = ES_UNKNOWN;
        break;
    }

    pmt_descriptor  = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t));
    bool type_known = false;

    while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length)) {
      mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: PMT descriptor tag 0x%|1$02x| length %2%\n") % static_cast<unsigned int>(pmt_descriptor->tag) % static_cast<unsigned int>(pmt_descriptor->length));

      switch(pmt_descriptor->tag) {
        case 0x56: // Teletext descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type = ES_UNKNOWN;
            type_known  = true;
            mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Teletext found but not handled !!\n");
          }
          break;
        case 0x59: // Subtitles descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type  = ES_SUBT_TYPE;
            track->codec = codec_c::look_up(CT_S_VOBSUB);
            type_known   = true;
          }
          break;
        case 0x6A: // AC3 descriptor
        case 0x7A: // EAC3 descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type  = ES_AUDIO_TYPE;
            track->codec = codec_c::look_up(CT_A_AC3);
            type_known   = true;
          }
          break;
        case 0x7b: // DTS descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type  = ES_AUDIO_TYPE;
            track->codec = codec_c::look_up(CT_A_DTS);
            type_known   = true;
          }
          break;
        case 0x0a: // ISO 639 language descriptor
          if (3 <= pmt_descriptor->length) {
            int language_idx = map_to_iso639_2_code(std::string(reinterpret_cast<char *>(pmt_descriptor + 1), 3).c_str());
            if (-1 != language_idx)
              track->language = iso639_languages[language_idx].iso639_2_code;
          }
          break;
      }

      pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_descriptor + sizeof(mpeg_ts_pmt_descriptor_t) + pmt_descriptor->length);
    }

    // Default to AC3 if it's a PES private stream type that's missing
    // a known/more concrete descriptor tag.
    if ((pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) && !type_known) {
      track->type  = ES_AUDIO_TYPE;
      track->codec = codec_c::look_up(CT_A_AC3);
    }

    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length);
    if (track->type != ES_UNKNOWN) {
      PMT_found         = true;
      track->pid        = track->pid;
      track->processed  = false;
      track->data_ready = false;
      tracks.push_back(track);
      es_to_process++;
      mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: PID %1% has type: %2%\n") % track->pid % track->codec.get_name());
    }
  }

  return 0;
}

timecode_c
mpeg_ts_reader_c::read_timecode(unsigned char *p) {
  int64_t mpeg_timecode  =  static_cast<int64_t>(             ( p[0]   >> 1) & 0x07) << 30;
  mpeg_timecode         |= (static_cast<int64_t>(get_uint16_be(&p[1])) >> 1)         << 15;
  mpeg_timecode         |=  static_cast<int64_t>(get_uint16_be(&p[3]))               >>  1;

  return std::move(timecode_c::mpeg(mpeg_timecode));
}

bool
mpeg_ts_reader_c::parse_packet(unsigned char *buf) {
  mpeg_ts_packet_header_t *hdr = (mpeg_ts_packet_header_t *)buf;
  uint16_t table_pid           = hdr->get_pid();

  if (hdr->get_transport_error_indicator()) //corrupted packet
    return false;

  if (!(hdr->get_adaptation_field_control() & 0x01)) //no ts_payload
    return false;

  size_t tidx;
  for (tidx = 0; tracks.size() > tidx; ++tidx)
    if ((tracks[tidx]->pid == table_pid) && !tracks[tidx]->processed)
      break;

  if (tidx >= tracks.size())
    return false;

  unsigned char *ts_payload                 = (unsigned char *)hdr + sizeof(mpeg_ts_packet_header_t);
  unsigned char adf_discontinuity_indicator = 0;
  if (hdr->get_adaptation_field_control() & 0x02) {
    mpeg_ts_adaptation_field_t *adf  = reinterpret_cast<mpeg_ts_adaptation_field_t *>(ts_payload);
    adf_discontinuity_indicator      = adf->get_discontinuity_indicator();
    ts_payload                      += static_cast<unsigned int>(adf->length) + 1;

    if (ts_payload >= (buf + TS_PACKET_SIZE))
      return false;
  }

  unsigned char ts_payload_size = TS_PACKET_SIZE - (ts_payload - (unsigned char *)hdr);

  // Copy the std::shared_ptr instead of referencing it because functions
  // called from this one will modify tracks.
  mpeg_ts_track_ptr track = tracks[tidx];

  if (!track)
    return false;

  if (hdr->get_payload_unit_start_indicator()) {
    if (!parse_start_unit_packet(track, hdr, ts_payload, ts_payload_size))
      return false;

  } else if (0 == track->pes_payload->get_size())
    return false;

  else {
    // Check continuity counter
    if (!adf_discontinuity_indicator) {
      track->continuity_counter++;
      track->continuity_counter %= 16;
      if (hdr->get_continuity_counter() != track->continuity_counter) {
        mxverb(3, boost::format("mpeg_ts: Continuity error on PID: %1%. Continue anyway...\n") % table_pid);
        track->continuity_counter = hdr->get_continuity_counter();
      }

    } else
      track->continuity_counter = hdr->get_continuity_counter();

    if (track->pes_payload_size != 0 && ts_payload_size > track->pes_payload_size - track->pes_payload->get_size())
      ts_payload_size = track->pes_payload_size - track->pes_payload->get_size();
  }

  if ((buf + TS_PACKET_SIZE) < (ts_payload + ts_payload_size))
    ts_payload_size = buf + TS_PACKET_SIZE - ts_payload;

  if (0 == ts_payload_size)
    return false;

  track->add_pes_payload(ts_payload, ts_payload_size);

  if (static_cast<int>(track->pes_payload->get_size()) == track->pes_payload_size)
    track->data_ready = true;

  mxdebug_if(track->m_debug_delivery, boost::format("PID %1%: Adding PES payload (normal case) num %2% bytes; expected %3% actual %4%\n")
             % track->pid % static_cast<unsigned int>(ts_payload_size) % track->pes_payload_size % track->pes_payload->get_size());

  if (!track->data_ready)
    return true;

  mxverb(3, boost::format("mpeg_ts: Table/PES completed (%1%) for PID %2%\n") % track->pes_payload->get_size() % table_pid);

  if (input_status == INPUT_PROBE)
    probe_packet_complete(track);

  else
    // PES completed, set track to quicly send it to the rightpacketizer
    track_buffer_ready = tidx;

  return true;
}

void
mpeg_ts_reader_c::probe_packet_complete(mpeg_ts_track_ptr &track) {
  int result = -1;

  if (track->type == PAT_TYPE)
    result = parse_pat(track->pes_payload->get_buffer());

  else if (track->type == PMT_TYPE)
    result = parse_pmt(track->pes_payload->get_buffer());

  else if (track->type == ES_VIDEO_TYPE) {
    if (track->codec.is(CT_V_MPEG12))
      result = track->new_stream_v_mpeg_1_2();
    else if (track->codec.is(CT_V_MPEG4_P10))
      result = track->new_stream_v_avc();
    else if (track->codec.is(CT_V_VC1))
      result = track->new_stream_v_vc1();

    track->pes_payload->set_chunk_size(512 * 1024);

  } else if (track->type == ES_AUDIO_TYPE) {
    if (track->codec.is(CT_A_MP3))
      result = track->new_stream_a_mpeg();
    else if (track->codec.is(CT_A_AAC))
      result = track->new_stream_a_aac();
    else if (track->codec.is(CT_A_AC3))
      result = track->new_stream_a_ac3();
    else if (track->codec.is(CT_A_DTS))
      result = track->new_stream_a_dts();
    else if (track->codec.is(CT_A_TRUEHD))
      result = track->new_stream_a_truehd();

  }

  track->pes_payload->remove(track->pes_payload->get_size());

  if (result == 0) {
    if (track->type == PAT_TYPE || track->type == PMT_TYPE) {
      auto it = brng::find(tracks, track);
      if (tracks.end() != it)
        tracks.erase(it);

    } else {
      track->processed = true;
      track->probed_ok = true;
      es_to_process--;
      mxverb(3, boost::format("mpeg_ts: ES to process: %1%\n") % es_to_process);
    }

  } else if (result == FILE_STATUS_MOREDATA) {
    mxverb(3, boost::format("mpeg_ts: Need more data to probe ES\n"));
    track->processed  = false;
    track->data_ready = false;

  } else {
    mxverb(3, boost::format("mpeg_ts: Failed to parse packet. Reset and retry\n"));
    track->pes_payload_size = 0;
    track->processed        = false;
    track->data_ready       = false;
  }
}

bool
mpeg_ts_reader_c::parse_start_unit_packet(mpeg_ts_track_ptr &track,
                                          mpeg_ts_packet_header_t *ts_packet_header,
                                          unsigned char *&ts_payload,
                                          unsigned char &ts_payload_size) {
  if ((track->type == PAT_TYPE) || (track->type == PMT_TYPE)) {
    if ((1 + *ts_payload) > ts_payload_size)
      return false;

    mpeg_ts_pat_t *table_data  = (mpeg_ts_pat_t *)(ts_payload + 1 + *ts_payload);
    ts_payload_size           -=  1 + *ts_payload;
    track->pes_payload_size    = table_data->get_section_length() + 3;
    ts_payload                 = (unsigned char *)table_data;

  } else {
    auto previous_pes_payload_size = track->pes_payload_size;
    mpeg_ts_pes_header_t *pes_data = (mpeg_ts_pes_header_t *)ts_payload;
    track->pes_payload_size        = pes_data->get_pes_packet_length();

    if (0 < track->pes_payload_size)
      track->pes_payload_size = track->pes_payload_size - 3 - pes_data->pes_header_data_length;

    if (track->pes_payload->get_size() && (previous_pes_payload_size != track->pes_payload_size)) {
      if (!previous_pes_payload_size) {
        if (INPUT_READ == input_status) {
          track->pes_payload_size = track->pes_payload->get_size();
          track->send_to_packetizer();
        } else
          probe_packet_complete(track);
      }

      track->pes_payload->remove(track->pes_payload->get_size());
      track->data_ready = false;
    }

    mxverb(4, boost::format("   PES info: stream_id = %1%\n") % (int)pes_data->stream_id);
    mxverb(4, boost::format("   PES info: PES_packet_length = %1%\n") % track->pes_payload_size);
    mxverb(4, boost::format("   PES info: PTS_DTS = %1% ESCR = %2% ES_rate = %3%\n") % (int)pes_data->pts_dts % (int)pes_data->get_escr() % (int)pes_data->get_es_rate());
    mxverb(4, boost::format("   PES info: DSM_trick_mode = %1%, add_copy = %2%, CRC = %3%, ext = %4%\n")
           % (int)pes_data->get_dsm_trick_mode() % (int)pes_data->get_additional_copy_info() % (int)pes_data->get_pes_crc() % (int)pes_data->get_pes_extension());
    mxverb(4, boost::format("   PES info: PES_header_data_length = %1%\n") % (int)pes_data->pes_header_data_length);

    ts_payload = &pes_data->pes_header_data_length + pes_data->pes_header_data_length + 1;
    if (ts_payload >= ((unsigned char *)ts_packet_header + TS_PACKET_SIZE))
      return false;

    ts_payload_size = ((unsigned char *)ts_packet_header + TS_PACKET_SIZE) - (unsigned char *) ts_payload;

    timecode_c pts, dts;
    if ((pes_data->get_pts_dts_flags() & 0x02) == 0x02) { // 10 and 11 mean PTS is present
      pts = read_timecode(&pes_data->pts_dts);
      dts = pts;
    }

    if ((pes_data->get_pts_dts_flags() & 0x01) == 0x01)   // 01 and 11 mean DTS is present
      dts = read_timecode(&pes_data->pts_dts + 5);

    if (!track->m_use_dts)
      dts = pts;

    track->handle_timecode_wrap(pts, dts);

    if (pts.valid()) {
      track->m_previous_valid_timecode = pts;

      if (!m_global_timecode_offset.valid() || (dts < m_global_timecode_offset))
        m_global_timecode_offset = dts;

      if (pts == track->m_timecode) {
        mxverb(3, boost::format("     Adding PES with same PTS as previous !!\n"));
        track->add_pes_payload(ts_payload, ts_payload_size);

        mxdebug_if(track->m_debug_delivery, boost::format("PID %1%: Adding PES payload (same PTS case) num %2% bytes; expected %3% actual %4%\n")
                   % track->pid % static_cast<unsigned int>(ts_payload_size) % track->pes_payload_size % track->pes_payload->get_size());

        return false;

      } else if ((0 != track->pes_payload->get_size()) && (INPUT_READ == input_status))
        track->send_to_packetizer();

      track->m_timecode = dts;

      mxverb(3, boost::format("     PTS/DTS found: %1%\n") % track->m_timecode);
    }

    // this condition is for ES probing when there is still not enough data for detection
    if (track->pes_payload_size == 0 && track->pes_payload->get_size() != 0)
      track->data_ready = true;

  }

  track->continuity_counter = ts_packet_header->get_continuity_counter();

  if ((track->pes_payload_size != 0) && (ts_payload_size + track->pes_payload->get_size()) > static_cast<size_t>(track->pes_payload_size))
    ts_payload_size = track->pes_payload_size - track->pes_payload->get_size();

  return true;
}

void
mpeg_ts_reader_c::create_packetizer(int64_t id) {
  if ((0 > id) || (tracks.size() <= static_cast<size_t>(id)))
    return;

  mpeg_ts_track_ptr &track = tracks[id];
  char type                = ES_AUDIO_TYPE == track->type ? 'a'
                           : ES_SUBT_TYPE  == track->type ? 's'
                           :                                'v';

  if (!track->probed_ok || (0 == track->ptzr) || !demuxing_requested(type, id))
    return;

  m_ti.m_id       = id;
  m_ti.m_language = track->language;

  if (ES_AUDIO_TYPE == track->type) {
    if (track->codec.is(CT_A_MP3)) {
      track->ptzr = add_packetizer(new mp3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, (0 != track->a_sample_rate) && (0 != track->a_channels)));
      show_packetizer_info(id, PTZR(track->ptzr));

    } else if (track->codec.is(CT_A_AAC)) {
      aac_packetizer_c *aac_packetizer = new aac_packetizer_c(this, m_ti, track->m_aac_header.id, track->m_aac_header.profile, track->m_aac_header.sample_rate, track->m_aac_header.channels, false);
      track->ptzr                      = add_packetizer(aac_packetizer);

      if (AAC_PROFILE_SBR == track->m_aac_header.profile)
        aac_packetizer->set_audio_output_sampling_freq(track->m_aac_header.sample_rate * 2);
      show_packetizer_info(id, aac_packetizer);

    } else if (track->codec.is(CT_A_AC3)) {
      track->ptzr = add_packetizer(new ac3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, track->a_bsid));
      show_packetizer_info(id, PTZR(track->ptzr));

    } else if (track->codec.is(CT_A_DTS)) {
      track->ptzr = add_packetizer(new dts_packetizer_c(this, m_ti, track->a_dts_header));
      show_packetizer_info(id, PTZR(track->ptzr));

    } else if (track->codec.is(CT_A_TRUEHD)) {
      track->ptzr = add_packetizer(new truehd_packetizer_c(this, m_ti, truehd_frame_t::truehd, track->a_sample_rate, track->a_channels));
      show_packetizer_info(id, PTZR(track->ptzr));
    }

  } else if (ES_VIDEO_TYPE == track->type) {
    if (track->codec.is(CT_V_MPEG12))
      create_mpeg1_2_video_packetizer(track);

    else if (track->codec.is(CT_V_MPEG4_P10))
      create_mpeg4_p10_es_video_packetizer(track);

    else if (track->codec.is(CT_V_VC1))
      create_vc1_video_packetizer(track);

  } else if (track->codec.is(CT_S_PGS))
    create_hdmv_pgs_subtitles_packetizer(track);

  if (-1 != track->ptzr)
    m_ptzr_to_track_map[PTZR(track->ptzr)] = track;
}

void
mpeg_ts_reader_c::create_mpeg1_2_video_packetizer(mpeg_ts_track_ptr &track) {
  m_ti.m_private_data = track->raw_seq_hdr;
  auto m2vpacketizer  = new mpeg1_2_video_packetizer_c(this, m_ti, track->v_version, track->v_frame_rate, track->v_width, track->v_height, track->v_dwidth, track->v_dheight, false);
  track->ptzr         = add_packetizer(m2vpacketizer);

  show_packetizer_info(m_ti.m_id, PTZR(track->ptzr));
  m2vpacketizer->set_video_interlaced_flag(track->v_interlaced);
}

void
mpeg_ts_reader_c::create_mpeg4_p10_es_video_packetizer(mpeg_ts_track_ptr &track) {
  generic_packetizer_c *ptzr = new mpeg4_p10_es_video_packetizer_c(this, m_ti);
  track->ptzr                = add_packetizer(ptzr);
  ptzr->set_video_pixel_dimensions(track->v_width, track->v_height);

  show_packetizer_info(m_ti.m_id, PTZR(track->ptzr));
}

void
mpeg_ts_reader_c::create_vc1_video_packetizer(mpeg_ts_track_ptr &track) {
  track->m_use_dts = true;
  track->ptzr      = add_packetizer(new vc1_video_packetizer_c(this, m_ti));
  show_packetizer_info(m_ti.m_id, PTZR(track->ptzr));
}

void
mpeg_ts_reader_c::create_hdmv_pgs_subtitles_packetizer(mpeg_ts_track_ptr &track) {
  pgs_packetizer_c *ptzr = new pgs_packetizer_c(this, m_ti);
  ptzr->set_aggregate_packets(true);
  track->ptzr = add_packetizer(ptzr);

  show_packetizer_info(m_ti.m_id, PTZR(track->ptzr));
}

void
mpeg_ts_reader_c::create_packetizers() {
  size_t i;

  input_status = INPUT_READ;
  mxverb(3, boost::format("mpeg_ts: create packetizers...\n"));
  for (i = 0; i < tracks.size(); i++)
    create_packetizer(i);
}

void
mpeg_ts_reader_c::add_available_track_ids() {
  size_t i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->probed_ok)
      add_available_track_id(i);
}

file_status_e
mpeg_ts_reader_c::finish() {
  if (file_done)
    return flush_packetizers();

  for (auto &track : tracks)
    if ((-1 != track->ptzr) && (0 < track->pes_payload->get_size()))
      PTZR(track->ptzr)->process(new packet_t(memory_c::clone(track->pes_payload->get_buffer(), track->pes_payload->get_size())));

  file_done = true;

  return flush_packetizers();
}

file_status_e
mpeg_ts_reader_c::read(generic_packetizer_c *requested_ptzr,
                       bool force) {
  int64_t num_queued_bytes = get_queued_bytes();
  if (!force && (20 * 1024 * 1024 < num_queued_bytes)) {
    mpeg_ts_track_ptr requested_ptzr_track = m_ptzr_to_track_map[requested_ptzr];
    if (!requested_ptzr_track || ((ES_AUDIO_TYPE != requested_ptzr_track->type) && (ES_VIDEO_TYPE != requested_ptzr_track->type)) || (512 * 1024 * 1024 < num_queued_bytes))
      return FILE_STATUS_HOLDING;
  }

  unsigned char buf[TS_MAX_PACKET_SIZE + 1];

  track_buffer_ready = -1;

  if (file_done)
    return flush_packetizers();

  while (true) {
    if (m_in->read(buf, m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
      return finish();

    if (buf[0] != 0x47) {
      if (resync(m_in->getFilePointer() - m_detected_packet_size))
        continue;
      return finish();
    }

    parse_packet(buf);

    if (track_buffer_ready != -1) { // ES buffer ready
      tracks[track_buffer_ready]->send_to_packetizer();
      track_buffer_ready = -1;
    }

    if (m_packet_sent_to_packetizer)
      return FILE_STATUS_MOREDATA;
  }
}

bfs::path
mpeg_ts_reader_c::find_clip_info_file() {
  auto mpls_multi_in = dynamic_cast<mm_mpls_multi_file_io_c *>(get_underlying_input());
  auto clpi_file     = mpls_multi_in ? mpls_multi_in->get_file_names()[0] : bfs::path{m_ti.m_fname};

  clpi_file.replace_extension(".clpi");

  mxdebug_if(m_debug_clpi, boost::format("Checking %1%\n") % clpi_file.string());

  if (bfs::exists(clpi_file))
    return clpi_file;

  bfs::path file_name(clpi_file.filename());
  bfs::path path(clpi_file.remove_filename());

  // clpi_file = path / ".." / file_name;
  // if (bfs::exists(clpi_file))
  //   return clpi_file;

  clpi_file = path / ".." / "clipinf" / file_name;
  mxdebug_if(m_debug_clpi, boost::format("Checking %1%\n") % clpi_file.string());
  if (bfs::exists(clpi_file))
    return clpi_file;

  clpi_file = path / ".." / "CLIPINF" / file_name;
  mxdebug_if(m_debug_clpi, boost::format("Checking %1%\n") % clpi_file.string());
  if (bfs::exists(clpi_file))
    return clpi_file;

  mxdebug_if(m_debug_clpi, "CLPI not found\n");

  return bfs::path();
}

void
mpeg_ts_reader_c::parse_clip_info_file() {
  bfs::path clpi_file(find_clip_info_file());
  if (clpi_file.empty())
    return;

  clpi::parser_c parser(clpi_file.string());
  if (!parser.parse())
    return;

  for (auto &track : tracks) {
    bool found = false;

    for (auto &program : parser.m_programs) {
      for (auto &stream : program->program_streams) {
        if ((stream->pid != track->pid) || stream->language.empty())
          continue;

        int language_idx = map_to_iso639_2_code(stream->language.c_str());
        if (-1 == language_idx)
          continue;

        track->language = iso639_languages[language_idx].iso639_2_code;
        found = true;
        break;
      }

      if (found)
        break;
    }
  }
}

bool
mpeg_ts_reader_c::resync(int64_t start_at) {
  try {
    mxdebug_if(m_debug_resync, boost::format("Start resync for data from %1%\n") % start_at);
    m_in->setFilePointer(start_at);

    unsigned char buf[TS_MAX_PACKET_SIZE + 1];

    while (!m_in->eof()) {
      int64_t curr_pos = m_in->getFilePointer();
      buf[0] = m_in->read_uint8();

      if (0x47 != buf[0])
        continue;

      if (m_in->read(&buf[1], m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
        return false;

      if (0x47 != buf[m_detected_packet_size]) {
        m_in->setFilePointer(curr_pos + 1);
        continue;
      }

      mxdebug_if(m_debug_resync, boost::format("Re-established at %1%\n") % curr_pos);

      m_in->setFilePointer(curr_pos);
      return true;
    }

  } catch (...) {
    return false;
  }

  return false;
}
