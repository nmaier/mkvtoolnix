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

#include <iostream>

#include "common/common_pch.h"

#include "common/checksums.h"
#include "common/clpi.h"
#include "common/endian.h"
#include "common/math.h"
#include "common/mp3.h"
#include "common/ac3.h"
#include "common/iso639.h"
#include "common/truehd.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "common/strings/formatting.h"
#include "input/r_mpeg_ts.h"
#include "output/p_mpeg1_2.h"
#include "output/p_avc.h"
#include "output/p_mp3.h"
#include "output/p_ac3.h"
#include "output/p_dts.h"
#include "output/p_pgs.h"
#include "output/p_truehd.h"
#include "output/p_vc1.h"

namespace bfs = boost::filesystem;

#define TS_CONSECUTIVE_PACKETS 16
#define TS_PROBE_SIZE          (2 * TS_CONSECUTIVE_PACKETS * 204)
#define TS_PIDS_DETECT_SIZE    10 * 1024 * 1024
#define TS_PACKET_SIZE         188
#define TS_MAX_PACKET_SIZE     204

#define GET_PID(p)             (((static_cast<uint16_t>(p->pid_msb) << 8) | p->pid_lsb) & 0x1FFF)
#define CONTINUITY_COUNTER(p)  (static_cast<unsigned char>(p->continuity_counter) & 0x0F)
#define SECTION_LENGTH(p)      (((static_cast<uint16_t>(p->section_length_msb) << 8) | p->section_length_lsb) & 0x0FFF)
#define CRC32(p)               ((static_cast<uint32_t>(p->crc_3msb) << 24) | (static_cast<uint32_t>(p->crc_2msb) << 16) | (static_cast<uint32_t>(p->crc_1msb) <<  8) | (static_cast<uint32_t>(p->crc_lsb)))
#define PROGRAM_NUMBER(p)      ((static_cast<uint16_t>(p->program_number_msb) << 8)| p->program_number_lsb)
#define PCR_PID(p)             (((static_cast<uint16_t>(p->pcr_pid_msb) << 8) | p->pcr_pid_lsb) & 0x1FFF)
#define PROGRAM_INFO_LENGTH(p) (((static_cast<uint16_t>(p->program_info_length_msb) << 8 ) | p->program_info_length_lsb) & 0x0FFF)
#define ES_INFO_LENGTH(p)      (((static_cast<uint16_t>(p->es_info_length_msb) << 8 ) | p->es_info_length_lsb) & 0x0FFF)

int mpeg_ts_reader_c::potential_packet_sizes[] = { 188, 192, 204, 0 };

// ------------------------------------------------------------

void
mpeg_ts_track_c::send_to_packetizer() {
  if (timecode < reader.m_global_timecode_offset)
    timecode = 0;
  else
    timecode = (uint64_t)(timecode - reader.m_global_timecode_offset) * 100000ll / 9;

  if ((type == ES_AUDIO_TYPE) && reader.m_dont_use_audio_pts)
    timecode = -1;

  mxverb(3, boost::format("mpeg_ts: PTS in nanoseconds: %1%\n") % timecode);

  if (ptzr != -1) {
    int64_t timecode_to_use = m_apply_dts_timecode_fix && (m_previous_timecode == timecode) ? -1 : timecode;
    reader.m_reader_packetizers[ptzr]->process(new packet_t(clone_memory(pes_payload->get_buffer(), pes_payload->get_size()), timecode_to_use));
  }

  pes_payload->remove(pes_payload->get_size());
  processed                          = false;
  data_ready                         = false;
  pes_payload_size                   = 0;
  m_previous_timecode                = timecode;
  reader.m_packet_sent_to_packetizer = true;
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
  if (!m_probe_data.is_set())
    m_probe_data = byte_buffer_cptr(new byte_buffer_c);
  m_probe_data->add(pes_payload->get_buffer(), pes_payload->get_size());
}

int
mpeg_ts_track_c::new_stream_v_mpeg_1_2() {
  if (!m_m2v_parser.is_set()) {
    m_m2v_parser = counted_ptr<M2VParser>(new M2VParser);
    m_m2v_parser->SetProbeMode();
  }

  m_m2v_parser->WriteData(pes_payload->get_buffer(), pes_payload->get_size());

  int state = m_m2v_parser->GetState();
  if (state != MPV_PARSER_STATE_FRAME) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: no valid frame in %1% bytes\n") % pes_payload->get_size());
    return FILE_STATUS_MOREDATA;
  }

  MPEG2SequenceHeader seq_hdr = m_m2v_parser->GetSequenceHeader();
  counted_ptr<MPEGFrame> frame(m_m2v_parser->ReadFrame());
  if (!frame.is_set())
    return FILE_STATUS_MOREDATA;

  fourcc         = FOURCC('M', 'P', 'G', '0' + m_m2v_parser->GetMPEGVersion());
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
  if (NULL != raw_seq_hdr_chunk) {
    mxverb(3, boost::format("new_stream_v_mpeg_1_2: sequence header size: %1%\n") % raw_seq_hdr_chunk->GetSize());
    raw_seq_hdr = clone_memory(raw_seq_hdr_chunk->GetPointer(), raw_seq_hdr_chunk->GetSize());
  }

  mxverb(3, boost::format("new_stream_v_mpeg_1_2: width: %1%, height: %2%\n") % v_width % v_height);
  if (v_width == 0 || v_height == 0)
    return FILE_STATUS_MOREDATA;

  return 0;
}

int
mpeg_ts_track_c::new_stream_v_avc() {
  if (!m_avc_parser.is_set()) {
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

  v_avcc = m_avc_parser->get_avcc();

  try {
    mm_mem_io_c avcc(v_avcc->get_buffer(), v_avcc->get_size());
    mm_mem_io_c new_avcc(NULL, v_avcc->get_size(), 1024);
    memory_cptr nalu(new memory_c());
    int num_sps, sps, sps_length;
    sps_info_t sps_info;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);

    num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;

    for (sps = 0; sps < num_sps; sps++) {
      bool abort;

      sps_length = avcc.read_uint16_be();
      if ((sps_length + avcc.getFilePointer()) >= v_avcc->get_size())
        sps_length = v_avcc->get_size() - avcc.getFilePointer();
      avcc.read(nalu, sps_length);

      abort = false;
      if ((0 < sps_length) && ((nalu->get_buffer()[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
        if (!mpeg4::p10::parse_sps(nalu, sps_info, true))
          throw false;
        rbsp_to_nalu(nalu);
        abort = true;
      }

      new_avcc.write_uint16_be(nalu->get_size());
      new_avcc.write(nalu);

      if (abort) {
        avcc.read(nalu, v_avcc->get_size() - avcc.getFilePointer());
        new_avcc.write(nalu);
        break;
      }
    }

    fourcc   = FOURCC('A', 'V', 'C', '1');
    v_avcc   = memory_cptr(new memory_c(new_avcc.get_and_lock_buffer(), new_avcc.getFilePointer(), true));
    v_width  = sps_info.width;
    v_height = sps_info.height;

    mxverb(3, boost::format("new_stream_v_avc: timing_info_present %1%, num_units_in_tick %2%, time_scale %3%, fixed_frame_rate %4%\n")
           % sps_info.timing_info_present % sps_info.num_units_in_tick % sps_info.time_scale % sps_info.fixed_frame_rate);

    if (sps_info.timing_info_present && sps_info.num_units_in_tick)
      v_frame_rate = static_cast<float>(sps_info.time_scale) / sps_info.num_units_in_tick;

    if (sps_info.ar_found) {
      float aspect_ratio = (float)sps_info.width / (float)sps_info.height * (float)sps_info.par_num / (float)sps_info.par_den;
      v_aspect_ratio = aspect_ratio;

      if (aspect_ratio > ((float)v_width / (float)v_height)) {
        v_dwidth  = irnd(v_height * aspect_ratio);
        v_dheight = v_height;

      } else {
        v_dwidth  = v_width;
        v_dheight = irnd(v_width / aspect_ratio);
      }
    }
    mxverb(3, boost::format("new_stream_v_avc: width: %1%, height: %2%\n") % v_width % v_height);

  } catch (...) {
    throw false;
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
  fourcc        = FOURCC('M', 'P', '0' + header.layer, ' ');

  mxverb(3, boost::format("new_stream_a_mpeg: Channels: %1%, sample rate: %2%\n") %a_channels % a_sample_rate);
  return 0;
}

int
mpeg_ts_track_c::new_stream_a_ac3() {
  add_pes_payload_to_probe_data();

  ac3_header_t header;

  if (-1 == find_ac3_header(m_probe_data->get_buffer(), m_probe_data->get_size(), &header, false))
    return FILE_STATUS_MOREDATA;

  mxverb(2,
         boost::format("first ac3 header bsid %1% channels %2% sample_rate %3% bytes %4% samples %5%\n")
         % header.bsid % header.channels % header.sample_rate % header.bytes % header.samples);

  a_channels    = header.channels;
  a_sample_rate = header.sample_rate;
  a_bsid        = header.bsid;

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
  if (!m_truehd_parser.is_set())
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

// ------------------------------------------------------------

bool
mpeg_ts_reader_c::probe_file(mm_io_c *io,
                             uint64_t size) {
  return -1 != detect_packet_size(io, size);
}

int
mpeg_ts_reader_c::detect_packet_size(mm_io_c *io,
                                     uint64_t size) {
  try {
    std::vector<int> positions;
    size = std::min(static_cast<uint64_t>(TS_PROBE_SIZE), size);
    memory_cptr buffer = memory_c::alloc(size);
    unsigned char *mem = buffer->get_buffer();
    size_t i, k;

    io->setFilePointer(0, seek_beginning);
    size = io->read(mem, size);

    for (i = 0; i < size; ++i)
      if (0x47 == mem[i])
        positions.push_back(i);

    for (i = 0; positions.size() > i; ++i) {
      for (k = 0; 0 != potential_packet_sizes[k]; ++k) {
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

mpeg_ts_reader_c::mpeg_ts_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
  , bytes_processed(0)
  , size(0)
  , PAT_found(false)
  , PMT_found(false)
  , PMT_pid(-1)
  , es_to_process(0)
  , m_global_timecode_offset(-1)
  , input_status(INPUT_PROBE)
  , track_buffer_ready(-1)
  , file_done(false)
  , m_packet_sent_to_packetizer(false)
  , m_dont_use_audio_pts(debugging_requested("mpeg_ts_dont_use_audio_pts") || debugging_requested("mpeg_ts"))
  , m_debug_resync(debugging_requested("mpeg_ts_resync") || debugging_requested("mpeg_ts"))
  , m_debug_pat_pmt(debugging_requested("mpeg_ts_pat") || debugging_requested("mpeg_ts_pmt") || debugging_requested("mpeg_ts"))
  , m_detected_packet_size(0)
{
  mm_io_cptr temp_io;

  try {
    temp_io = mm_io_cptr(new mm_file_io_c(m_ti.m_fname));

  } catch (...) {
    throw error_c(Y("mpeg_ts_reader_c: Could not open the file."));
  }

  try {
    size                     = temp_io->get_size();
    size_t size_to_probe     = std::min(size, static_cast<int64_t>(TS_PIDS_DETECT_SIZE));
    memory_cptr probe_buffer = memory_c::alloc(size_to_probe);

    temp_io->read(probe_buffer, size_to_probe);

    m_io                   = mm_io_cptr(new mm_mem_io_c(probe_buffer->get_buffer(), probe_buffer->get_size()));
    m_detected_packet_size = detect_packet_size(m_io.get_object(), size_to_probe);
    m_io->setFilePointer(0);

    mxverb(3, boost::format("mpeg_ts: Starting to build PID list. (packet size: %1%)\n") % m_detected_packet_size);

    mpeg_ts_track_ptr PAT(new mpeg_ts_track_c(*this));
    PAT->pid  = 0;
    PAT->type = PAT_TYPE;
    tracks.push_back(PAT);

    unsigned char buf[TS_MAX_PACKET_SIZE]; // maximum TS packet size + 1

    bool done = m_io->eof();
    while (!done) {
      if (m_io->read(buf, m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
        break;

      if (buf[0] != 0x47) {
        if (resync(m_io->getFilePointer() - m_detected_packet_size))
          continue;
        break;
      }

      parse_packet(buf);
      done  = PAT_found && PMT_found && (0 == es_to_process);
      done |= m_io->eof() || (m_io->getFilePointer() >= TS_PIDS_DETECT_SIZE);
    }
  } catch (...) {
  }
  mxverb(3, boost::format("mpeg_ts: Detection done on %1% bytes\n") % m_io->getFilePointer());

  m_io = temp_io;
  m_io->setFilePointer(0, seek_beginning); // rewind file for later remux

  foreach(mpeg_ts_track_ptr &track, tracks) {
    track->pes_payload->remove(track->pes_payload->get_size());
    track->processed        = false;
    track->data_ready       = false;
    track->pes_payload_size = 0;
    // track->timecode_offset = -1;
  }

  parse_clip_info_file();
}

mpeg_ts_reader_c::~mpeg_ts_reader_c() {
}

void
mpeg_ts_reader_c::identify() {
  id_result_container("MPEG2 Transport Stream (TS)");

  size_t i;
  for (i = 0; i < tracks.size(); i++) {
    mpeg_ts_track_ptr &track = tracks[i];

    if (!track->probed_ok)
      continue;

    const char *fourcc = FOURCC('M', 'P', 'G', '1') == track->fourcc ? "MPEG-1"
                       : FOURCC('M', 'P', 'G', '2') == track->fourcc ? "MPEG-2"
                       : FOURCC('A', 'V', 'C', '1') == track->fourcc ? "AVC/h.264"
                       : FOURCC('W', 'V', 'C', '1') == track->fourcc ? "VC1"
                       : FOURCC('M', 'P', '1', ' ') == track->fourcc ? "MPEG-1 layer 1"
                       : FOURCC('M', 'P', '2', ' ') == track->fourcc ? "MPEG-1 layer 2"
                       : FOURCC('M', 'P', '3', ' ') == track->fourcc ? "MPEG-1 layer 3"
                       : FOURCC('A', 'C', '3', ' ') == track->fourcc ? "AC3"
                       : FOURCC('D', 'T', 'S', ' ') == track->fourcc ? "DTS"
                       : FOURCC('T', 'R', 'H', 'D') == track->fourcc ? "TrueHD"
                       : FOURCC('P', 'G', 'S', ' ') == track->fourcc ? "HDMV PGS"
                       // : FOURCC('P', 'C', 'M', ' ') == track->fourcc ? "PCM"
                       // : FOURCC('L', 'P', 'C', 'M') == track->fourcc ? "LPCM"
                       :                                               NULL;

    if (!fourcc)
      continue;

    std::vector<std::string> verbose_info;
    if (!track->language.empty())
      verbose_info.push_back((boost::format("language:%1%") % escape(track->language)).str());

    if (debugging_requested("mpeg_ts_pid"))
      verbose_info.push_back((boost::format("ts_pid:%1%") % track->pid).str());

    std::string type = ES_AUDIO_TYPE == track->type ? ID_RESULT_TRACK_AUDIO
                     : ES_VIDEO_TYPE == track->type ? ID_RESULT_TRACK_VIDEO
                     :                                ID_RESULT_TRACK_SUBTITLES;

    id_result_track(i, type, fourcc, verbose_info);
  }
}

int
mpeg_ts_reader_c::parse_pat(unsigned char *pat) {
  if (pat == NULL) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid parameters!\n");
    return -1;
  }

  mpeg_ts_pat_t *pat_header = (mpeg_ts_pat_t *)pat;

  if (pat_header->table_id != 0x00) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid PAT table_id!\n");
    return -1;
  }

  if (pat_header->section_syntax_indicator != 1 || pat_header->current_next_indicator == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Invalid PAT section_syntax_indicator/current_next_indicator!\n");
    return -1;
  }

  if (pat_header->section_number != 0 || pat_header->last_section_number != 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pat: Unsupported multiple section PAT!\n");
    return -1;
  }

  unsigned short pat_section_length = SECTION_LENGTH(pat_header);
  uint32_t elapsed_CRC              = crc_calc_mpeg2(pat, 3 + pat_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = CRC32(((mpeg_ts_crc_t *)(pat + 3 + pat_section_length - 4)));

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
    unsigned short local_program_number = PROGRAM_NUMBER(pat_section);
    uint16_t tmp_pid                    = GET_PID(pat_section);

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
      PMT->pid        = tmp_pid;
      PMT->type       = PMT_TYPE;
      PMT->processed  = false;
      PMT->data_ready = false;
      es_to_process   = 0;
      tracks.push_back(PMT);
    }
  }

  return 0;
}

int
mpeg_ts_reader_c::parse_pmt(unsigned char *pmt) {
  if (pmt == NULL) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid parameters!\n");
    return -1;
  }

  mpeg_ts_pmt_t *pmt_header = (mpeg_ts_pmt_t *)pmt;

  if (pmt_header->table_id != 0x02) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid PMT table_id!\n");
    return -1;
  }

  if (pmt_header->section_syntax_indicator != 1 || pmt_header->current_next_indicator == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Invalid PMT section_syntax_indicator/current_next_indicator!\n");
    return -1;
  }

  if (pmt_header->section_number != 0 || pmt_header->last_section_number != 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Unsupported multiple section PMT!\n");
    return -1;
  }

  unsigned short pmt_section_length = SECTION_LENGTH(pmt_header);
  uint32_t elapsed_CRC              = crc_calc_mpeg2(pmt, 3 + pmt_section_length - 4/*CRC32*/);
  uint32_t read_CRC                 = CRC32(((mpeg_ts_crc_t *)(pmt + 3 + pmt_section_length - 4)));

  if (elapsed_CRC != read_CRC) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Wrong PMT CRC !!! Elapsed = 0x%|1$08x|, read 0x%|2$08x|\n") % elapsed_CRC % read_CRC);
    return -1;
  }

  if (pmt_section_length < 13 || pmt_section_length > 1021) {
    mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Wrong PMT section_length (=%1%)\n") % pmt_section_length);
    return -1;
  }

  mpeg_ts_pmt_descriptor_t *pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t));
  unsigned short program_info_length       = PROGRAM_INFO_LENGTH(pmt_header);

  while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)(pmt + sizeof(mpeg_ts_pmt_t) + program_info_length))
    pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_descriptor + sizeof(mpeg_ts_pmt_descriptor_t) + pmt_descriptor->length);

  mpeg_ts_pmt_pid_info_t *pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  // Calculate pids_count
  size_t pids_found = 0;
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    pids_found++;
    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + ES_INFO_LENGTH(pmt_pid_info));
  }

  mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: program number     (%1%)\n") % PROGRAM_NUMBER(pmt_header));
  mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: pcr pid            (%1%)\n") % PCR_PID(pmt_header));

  if (pids_found == 0) {
    mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: There's no information about elementary PIDs\n");
    return 0;
  }

  pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)pmt_descriptor;

  // Extract pid_info
  while (pmt_pid_info < (mpeg_ts_pmt_pid_info_t *)(pmt + 3 + pmt_section_length - 4/*CRC32*/)) {
    mpeg_ts_track_ptr track(new mpeg_ts_track_c(*this));
    unsigned short es_info_length = ES_INFO_LENGTH(pmt_pid_info);
    track->pid                    = GET_PID(pmt_pid_info);
    track->type                   = ES_UNKNOWN;

    switch(pmt_pid_info->stream_type) {
      case ISO_11172_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '1');
        break;
      case ISO_13818_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '2');
        break;
      case ISO_14496_PART2_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('M', 'P', 'G', '4');
        break;
      case ISO_14496_PART10_VIDEO:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('A', 'V', 'C', '1');
        break;
      case STREAM_VIDEO_VC1:
        track->type   = ES_VIDEO_TYPE;
        track->fourcc = FOURCC('W', 'V', 'C', '1');
        break;
      case ISO_11172_AUDIO:
      case ISO_13818_AUDIO:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('M', 'P', '2', ' ');
        break;
      case ISO_13818_PART7_AUDIO:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'A', 'C', ' ');
        break;
      case ISO_14496_PART3_AUDIO:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'A', 'C', ' ');
        break;
      case STREAM_AUDIO_AC3:
      case STREAM_AUDIO_AC3_PLUS: // EAC3
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('A', 'C', '3', ' ');
        break;
      case STREAM_AUDIO_AC3_LOSSLESS:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('T', 'R', 'H', 'D');
        break;
      case STREAM_AUDIO_DTS:
      case STREAM_AUDIO_DTS_HD:
      case STREAM_AUDIO_DTS_HD_MA:
        track->type   = ES_AUDIO_TYPE;
        track->fourcc = FOURCC('D', 'T', 'S', ' ');
        break;
      case STREAM_SUBTITLES_HDMV_PGS:
        track->type      = ES_SUBT_TYPE;
        track->fourcc    = FOURCC('P', 'G', 'S', ' ');
        track->probed_ok = true;
        break;
      case ISO_13818_PES_PRIVATE:
        break;
      default:
        mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: Unknown stream type: %1%\n") % (int)pmt_pid_info->stream_type);
        track->type   = ES_UNKNOWN;
        break;
    }

    pmt_descriptor = (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t));

    while (pmt_descriptor < (mpeg_ts_pmt_descriptor_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length)) {
      mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: PMT descriptor tag 0x%|1$02x| length %2%\n") % static_cast<unsigned int>(pmt_descriptor->tag) % static_cast<unsigned int>(pmt_descriptor->length));

      switch(pmt_descriptor->tag) {
        case 0x56: // Teletext descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type   = ES_UNKNOWN;
            mxdebug_if(m_debug_pat_pmt, "mpeg_ts:parse_pmt: Teletext found but not handled !!\n");
          }
          break;
        case 0x59: // Subtitles descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type   = ES_SUBT_TYPE;
            track->fourcc = FOURCC('V', 'S', 'U', 'B');
          }
          break;
        case 0x6A: // AC3 descriptor
        case 0x7A: // EAC3 descriptor
          if (pmt_pid_info->stream_type == ISO_13818_PES_PRIVATE) { // PES containig private data
            track->type   = ES_AUDIO_TYPE;
            track->fourcc = FOURCC('A', 'C', '3', ' ');
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

    pmt_pid_info = (mpeg_ts_pmt_pid_info_t *)((unsigned char *)pmt_pid_info + sizeof(mpeg_ts_pmt_pid_info_t) + es_info_length);
    if (track->type != ES_UNKNOWN) {
      PMT_found         = true;
      track->pid        = track->pid;
      track->processed  = false;
      track->data_ready = false;
      tracks.push_back(track);
      es_to_process++;
      uint32_t fourcc = get_uint32_be(&track->fourcc);
      mxdebug_if(m_debug_pat_pmt, boost::format("mpeg_ts:parse_pmt: PID %1% has type: 0x%|2$08x| (%3%)\n") % track->pid % fourcc % std::string(reinterpret_cast<char *>(&fourcc), 4));
    }
  }

  return 0;
}

int64_t
mpeg_ts_reader_c::read_timestamp(unsigned char *p) {
  int64_t pts  =  static_cast<int64_t>(             ( p[0]   >> 1) & 0x07) << 30;
  pts         |= (static_cast<int64_t>(get_uint16_be(&p[1])) >> 1)         << 15;
  pts         |=  static_cast<int64_t>(get_uint16_be(&p[3]))               >>  1;

  return pts;
}

bool
mpeg_ts_reader_c::parse_packet(unsigned char *buf) {
  mpeg_ts_packet_header_t *hdr = (mpeg_ts_packet_header_t *)buf;
  int16_t table_pid            = (int16_t)GET_PID(hdr);

  if (hdr->transport_error_indicator == 1) //corrupted packet
    return false;

  if (!(hdr->adaptation_field_control & 0x01)) //no ts_payload
    return false;

  size_t tidx;
  for (tidx = 0; tracks.size() > tidx; ++tidx)
    if ((tracks[tidx]->pid == table_pid) && !tracks[tidx]->processed)
      break;

  if (tidx >= tracks.size())
    return false;

  unsigned char *ts_payload                 = (unsigned char *)hdr + sizeof(mpeg_ts_packet_header_t);
  unsigned char adf_discontinuity_indicator = 0;
  if (hdr->adaptation_field_control & 0x02) {
    mpeg_ts_adaptation_field_t *adf  = reinterpret_cast<mpeg_ts_adaptation_field_t *>(ts_payload);
    adf_discontinuity_indicator      = adf->discontinuity_indicator;
    ts_payload                      += static_cast<unsigned int>(adf->adaptation_field_length) + 1;

    if (ts_payload >= (buf + TS_PACKET_SIZE))
      return false;
  }

  unsigned char ts_payload_size = TS_PACKET_SIZE - (ts_payload - (unsigned char *)hdr);

  // Copy the counted_ptr instead of referencing it because functions
  // called from this one will modify tracks.
  mpeg_ts_track_ptr track = tracks[tidx];

  if (!track.is_set())
    return false;

  if (hdr->payload_unit_start_indicator) {
    if (!parse_start_unit_packet(track, hdr, ts_payload, ts_payload_size))
      return false;

  } else if (0 == track->pes_payload->get_size())
    return false;

  else {
    // Check continuity counter
    if (!adf_discontinuity_indicator) {
      track->continuity_counter++;
      track->continuity_counter %= 16;
      if (CONTINUITY_COUNTER(hdr) != track->continuity_counter) {
        mxverb(3, boost::format("mpeg_ts: Continuity error on PID: %1%. Continue anyway...\n") % table_pid);
        track->continuity_counter = CONTINUITY_COUNTER(hdr);
      }

    } else
      track->continuity_counter = CONTINUITY_COUNTER(hdr);

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

  if (!track->data_ready)
    return true;

  mxverb(3, boost::format("mpeg_ts: Table/PES completed (%1%) for PID %2%\n") % track->pes_payload->get_size() % table_pid);

  if (input_status == INPUT_PROBE)
    probe_packet_complete(track, tidx);

  else
    // PES completed, set track to quicly send it to the rightpacketizer
    track_buffer_ready = tidx;

  return true;
}

void
mpeg_ts_reader_c::probe_packet_complete(mpeg_ts_track_ptr &track,
                                        int tidx) {
  int result = -1;

  if (track->type == PAT_TYPE)
    result = parse_pat(track->pes_payload->get_buffer());

  else if (track->type == PMT_TYPE)
    result = parse_pmt(track->pes_payload->get_buffer());

  else if (track->type == ES_VIDEO_TYPE) {
    if ((FOURCC('M', 'P', 'G', '1') == track->fourcc) || (FOURCC('M', 'P', 'G', '2') == track->fourcc))
      result = track->new_stream_v_mpeg_1_2();
    else if (FOURCC('A', 'V', 'C', '1') == track->fourcc)
      result = track->new_stream_v_avc();
    else if (FOURCC('W', 'V', 'C', '1') == track->fourcc)
      result = track->new_stream_v_vc1();

    track->pes_payload->set_chunk_size(512 * 1024);

  } else if (track->type == ES_AUDIO_TYPE) {
    if (FOURCC('M', 'P', '2', ' ') == track->fourcc)
      result = track->new_stream_a_mpeg();
    else if (FOURCC('A', 'C', '3', ' ') == track->fourcc)
      result = track->new_stream_a_ac3();
    else if (FOURCC('D', 'T', 'S', ' ') == track->fourcc)
      result = track->new_stream_a_dts();
    else if (FOURCC('T', 'R', 'H', 'D') == track->fourcc)
      result = track->new_stream_a_truehd();

  }

  track->pes_payload->remove(track->pes_payload->get_size());

  if (result == 0) {
    if (track->type == PAT_TYPE || track->type == PMT_TYPE)
      tracks.erase(tracks.begin() + tidx);
    else {
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
    track->pes_payload_size    = SECTION_LENGTH(table_data) + 3;
    ts_payload                 = (unsigned char *)table_data;

  } else {
    mpeg_ts_pes_header_t *pes_data = (mpeg_ts_pes_header_t *)ts_payload;
    track->pes_payload_size        = ((uint16_t) (pes_data->PES_packet_length_msb) << 8) | (pes_data->PES_packet_length_lsb);

    if (0 < track->pes_payload_size)
      track->pes_payload_size = track->pes_payload_size - 3 - pes_data->PES_header_data_length;

    if (   (track->pes_payload_size        == 0)
        && (track->pes_payload->get_size() != 0)
        && (input_status                   == INPUT_READ)) {
      track->pes_payload_size = track->pes_payload->get_size();
      mxverb(3, boost::format("mpeg_ts: Table/PES completed (%1%) for PID %2%\n") % track->pes_payload_size % GET_PID(ts_packet_header));
      track->send_to_packetizer();
    }

    mxverb(4, boost::format("   PES info: stream_id = %1%\n") % (int)pes_data->stream_id);
    mxverb(4, boost::format("   PES info: PES_packet_length = %1%\n") % track->pes_payload_size);
    mxverb(4, boost::format("   PES info: PTS_DTS = %1% ESCR = %2% ES_rate = %3%\n") % (int)pes_data->PTS_DTS % (int)pes_data->ESCR % (int)pes_data->ES_rate);
    mxverb(4, boost::format("   PES info: DSM_trick_mode = %1%, add_copy = %2%, CRC = %3%, ext = %4%\n") % (int)pes_data->DSM_trick_mode % (int)pes_data->additional_copy_info % (int)pes_data->PES_CRC % (int)pes_data->PES_extension);
    mxverb(4, boost::format("   PES info: PES_header_data_length = %1%\n") % (int)pes_data->PES_header_data_length);

    ts_payload = &pes_data->PES_header_data_length + pes_data->PES_header_data_length + 1;
    if (ts_payload >= ((unsigned char *)ts_packet_header + TS_PACKET_SIZE))
      return false;

    ts_payload_size = ((unsigned char *)ts_packet_header + TS_PACKET_SIZE) - (unsigned char *) ts_payload;

    if (pes_data->PTS_DTS_flags > 1) { // 10 and 11 mean PTS is present
      int64_t PTS = read_timestamp(&pes_data->PTS_DTS);

      if ((-1 == m_global_timecode_offset) || (PTS < m_global_timecode_offset)) {
        mxverb(3, boost::format("global_timecode_offset %1%\n") % PTS);
        m_global_timecode_offset = PTS;
      }

      if (PTS == track->timecode) {
        mxverb(3, boost::format("     Adding PES with same PTS as previous !!\n"));
        track->add_pes_payload(ts_payload, ts_payload_size);
        return false;

      } else if ((0 != track->pes_payload->get_size()) && (INPUT_READ == input_status))
        track->send_to_packetizer();

      track->timecode = PTS;

      mxverb(3, boost::format("     PTS found: %1%\n") % track->timecode);
    }

    // this condition is for ES probing when there is still not enough data for detection
    if (track->pes_payload_size == 0 && track->pes_payload->get_size() != 0)
      track->data_ready = true;

  }

  track->continuity_counter = CONTINUITY_COUNTER(ts_packet_header);

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
    if (   (FOURCC('M', 'P', '1', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '2', ' ') == track->fourcc)
        || (FOURCC('M', 'P', '3', ' ') == track->fourcc)) {
      mxinfo_tid(m_ti.m_fname, id, Y("Using the MPEG audio output module.\n"));
      track->ptzr = add_packetizer(new mp3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, (0 != track->a_sample_rate) && (0 != track->a_channels)));

    } else if (FOURCC('A', 'C', '3', ' ') == track->fourcc) {
      mxinfo_tid(m_ti.m_fname, id, boost::format(Y("Using the %1%AC3 output module.\n")) % (16 == track->a_bsid ? "E" : ""));
      track->ptzr = add_packetizer(new ac3_packetizer_c(this, m_ti, track->a_sample_rate, track->a_channels, track->a_bsid));

    } else if (FOURCC('D', 'T', 'S', ' ') == track->fourcc) {
      mxinfo_tid(m_ti.m_fname, id, Y("Using the DTS output module.\n"));
      track->ptzr = add_packetizer(new dts_packetizer_c(this, m_ti, track->a_dts_header));

    } else if (FOURCC('T', 'R', 'H', 'D') == track->fourcc) {
      mxinfo_tid(m_ti.m_fname, id, Y("Using the TrueHD output module.\n"));
      track->ptzr = add_packetizer(new truehd_packetizer_c(this, m_ti, truehd_frame_t::truehd, track->a_sample_rate, track->a_channels));
    }

  } else if (ES_VIDEO_TYPE == track->type) {
    if (   (FOURCC('M', 'P', 'G', '1') == track->fourcc)
        || (FOURCC('M', 'P', 'G', '2') == track->fourcc))
      create_mpeg1_2_video_packetizer(track);

    else if (track->fourcc == FOURCC('A', 'V', 'C', '1'))
      create_mpeg4_p10_es_video_packetizer(track);

    else if (track->fourcc == FOURCC('W', 'V', 'C', '1'))
      create_vc1_video_packetizer(track);

  } else if (FOURCC('P', 'G', 'S', ' ') == track->fourcc)
    create_hdmv_pgs_subtitles_packetizer(track);

  if (-1 != track->ptzr)
    m_ptzr_to_track_map[PTZR(track->ptzr)] = track;
}

void
mpeg_ts_reader_c::create_mpeg1_2_video_packetizer(mpeg_ts_track_ptr &track) {
  mxinfo_tid(m_ti.m_fname, m_ti.m_id, Y("Using the MPEG-1/2 video output module.\n"));

  if (track->raw_seq_hdr.is_set() && (0 < track->raw_seq_hdr->get_size())) {
    m_ti.m_private_data = track->raw_seq_hdr->get_buffer();
    m_ti.m_private_size = track->raw_seq_hdr->get_size();
  } else {
    m_ti.m_private_data = NULL;
    m_ti.m_private_size = 0;
  }

  generic_packetizer_c *m2vpacketizer = new mpeg1_2_video_packetizer_c(this, m_ti, track->v_version, track->v_frame_rate, track->v_width, track->v_height,
                                                                       track->v_dwidth, track->v_dheight, false);
  track->ptzr                         = add_packetizer(m2vpacketizer);
  m_ti.m_private_data                 = NULL;
  m_ti.m_private_size                 = 0;
  m2vpacketizer->set_video_interlaced_flag(track->v_interlaced);
}

void
mpeg_ts_reader_c::create_mpeg4_p10_es_video_packetizer(mpeg_ts_track_ptr &track) {
  mxinfo_tid(m_ti.m_fname, m_ti.m_id, Y("Using the MPEG-4 part 10 ES video output module.\n"));

  mpeg4_p10_es_video_packetizer_c *avcpacketizer = new mpeg4_p10_es_video_packetizer_c(this, m_ti, track->v_avcc, track->v_width, track->v_height);
  track->ptzr                                    = add_packetizer(avcpacketizer);

  if (track->v_frame_rate)
    avcpacketizer->set_track_default_duration(static_cast<int64_t>(1000000000.0 / track->v_frame_rate));

  // This is intentional so that the AVC parser knows the actual
  // default duration from above but only generates timecode in
  // emergencies.
  avcpacketizer->enable_timecode_generation(false);
}

void
mpeg_ts_reader_c::create_vc1_video_packetizer(mpeg_ts_track_ptr &track) {
  mxinfo_tid(m_ti.m_fname, m_ti.m_id, Y("Using the VC1 video output module.\n"));
  track->ptzr = add_packetizer(new vc1_video_packetizer_c(this, m_ti));
}

void
mpeg_ts_reader_c::create_hdmv_pgs_subtitles_packetizer(mpeg_ts_track_ptr &track) {
  mxinfo_tid(m_ti.m_fname, m_ti.m_id, Y("Using the PGS output module.\n"));

  pgs_packetizer_c *ptzr = new pgs_packetizer_c(this, m_ti);
  ptzr->set_aggregate_packets(true);
  track->ptzr = add_packetizer(ptzr);
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

int
mpeg_ts_reader_c::get_progress() {
  return 100 * m_io->getFilePointer() / size;
}

file_status_e
mpeg_ts_reader_c::finish() {
  if (file_done)
    return flush_packetizers();

  foreach(mpeg_ts_track_ptr &track, tracks)
    if ((-1 != track->ptzr) && (0 < track->pes_payload->get_size()))
      PTZR(track->ptzr)->process(new packet_t(clone_memory(track->pes_payload->get_buffer(), track->pes_payload->get_size())));

  file_done = true;

  return flush_packetizers();
}

file_status_e
mpeg_ts_reader_c::read(generic_packetizer_c *requested_ptzr,
                       bool force) {
  int64_t num_queued_bytes = get_queued_bytes();
  if (!force && (20 * 1024 * 1024 < num_queued_bytes)) {
    mpeg_ts_track_ptr requested_ptzr_track = m_ptzr_to_track_map[requested_ptzr];
    if (!requested_ptzr_track.is_set() || ((ES_AUDIO_TYPE != requested_ptzr_track->type) && (ES_VIDEO_TYPE != requested_ptzr_track->type)) || (512 * 1024 * 1024 < num_queued_bytes))
      return FILE_STATUS_HOLDING;
  }

  unsigned char buf[TS_MAX_PACKET_SIZE + 1];

  track_buffer_ready = -1;

  if (file_done)
    return flush_packetizers();

  while (true) {
    if (m_io->read(buf, m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
      return finish();

    if (buf[0] != 0x47) {
      if (resync(m_io->getFilePointer() - m_detected_packet_size))
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
  bool debug = debugging_requested("clpi");

  bfs::path clpi_file(m_ti.m_fname);
  clpi_file.replace_extension(".clpi");

  mxdebug_if(debug, boost::format("Checking %1%\n") % clpi_file.string());

  if (bfs::exists(clpi_file))
    return clpi_file;

  bfs::path file_name(clpi_file.filename());
  bfs::path path(clpi_file.remove_filename());

  // clpi_file = path / ".." / file_name;
  // if (bfs::exists(clpi_file))
  //   return clpi_file;

  clpi_file = path / ".." / "clipinf" / file_name;
  mxdebug_if(debug, boost::format("Checking %1%\n") % clpi_file.string());
  if (bfs::exists(clpi_file))
    return clpi_file;

  clpi_file = path / ".." / "CLIPINF" / file_name;
  mxdebug_if(debug, boost::format("Checking %1%\n") % clpi_file.string());
  if (bfs::exists(clpi_file))
    return clpi_file;

  mxdebug_if(debug, "CLPI not found\n");

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

  foreach(mpeg_ts_track_ptr &track, tracks) {
    bool found = false;

    foreach(clpi::program_cptr &program, parser.m_programs) {
      foreach(clpi::program_stream_cptr &stream, program->program_streams) {
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
    m_io->setFilePointer(start_at);

    unsigned char buf[TS_MAX_PACKET_SIZE + 1];

    while (!m_io->eof()) {
      int64_t curr_pos = m_io->getFilePointer();
      buf[0] = m_io->read_uint8();

      if (0x47 != buf[0])
        continue;

      if (m_io->read(&buf[1], m_detected_packet_size) != static_cast<unsigned int>(m_detected_packet_size))
        return false;

      if (0x47 != buf[m_detected_packet_size]) {
        m_io->setFilePointer(curr_pos + 1);
        continue;
      }

      mxdebug_if(m_debug_resync, boost::format("Re-established at %1%\n") % curr_pos);

      m_io->setFilePointer(curr_pos);
      return true;
    }

  } catch (...) {
    return false;
  }

  return false;
}
