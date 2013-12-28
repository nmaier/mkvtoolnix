/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IVF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include "common/endian.h"
#include "common/ivf.h"
#include "input/r_ivf.h"
#include "output/p_vpx.h"
#include "merge/output_control.h"

int
ivf_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
  if (sizeof(ivf::file_header_t) > size)
    return 0;

  ivf::file_header_t header;
  io->setFilePointer(0, seek_beginning);
  if (io->read(&header, sizeof(ivf::file_header_t)) < sizeof(ivf::file_header_t))
    return 0;

  if (memcmp(header.file_magic, "DKIF", 4) || header.get_codec().is(CT_UNKNOWN))
    return 0;

  return 1;
}

ivf_reader_c::ivf_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
{
}

void
ivf_reader_c::read_headers() {
  try {
    ivf::file_header_t header;
    m_in->read(&header, sizeof(ivf::file_header_t));

    m_width          = get_uint16_le(&header.width);
    m_height         = get_uint16_le(&header.height);
    m_frame_rate_num = get_uint32_le(&header.frame_rate_num);
    m_frame_rate_den = get_uint32_le(&header.frame_rate_den);
    m_codec          = header.get_codec();

    m_ti.m_id        = 0;       // ID for this track.

  } catch (...) {
    throw mtx::input::open_x();
  }

  show_demuxer_info();
}

ivf_reader_c::~ivf_reader_c() {
}

void
ivf_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  auto packetizer = new vpx_video_packetizer_c(this, m_ti, m_codec.get_type());
  add_packetizer(packetizer);

  packetizer->set_video_pixel_width(m_width);
  packetizer->set_video_pixel_height(m_height);

  uint64_t default_duration = 1000000000ll * m_frame_rate_den / m_frame_rate_num;
  if (default_duration >= 1000000)
    packetizer->set_track_default_duration(default_duration);

  show_packetizer_info(0, packetizer);
}

file_status_e
ivf_reader_c::read(generic_packetizer_c *,
                   bool) {
  size_t remaining_bytes = m_size - m_in->getFilePointer();

  ivf::frame_header_t header;
  if ((sizeof(ivf::frame_header_t) > remaining_bytes) || (m_in->read(&header, sizeof(ivf::frame_header_t)) != sizeof(ivf::frame_header_t)))
    return flush_packetizers();

  remaining_bytes     -= sizeof(ivf::frame_header_t);
  uint32_t frame_size  = get_uint32_le(&header.frame_size);

  if (remaining_bytes < frame_size) {
    m_in->setFilePointer(0, seek_end);
    return flush_packetizers();
  }

  memory_cptr buffer = memory_c::alloc(frame_size);
  if (m_in->read(buffer->get_buffer(), frame_size) < frame_size) {
    m_in->setFilePointer(0, seek_end);
    return flush_packetizers();
  }

  int64_t timestamp = get_uint64_le(&header.timestamp) * 1000000000ull * m_frame_rate_den / m_frame_rate_num;

  mxverb(3, boost::format("r_ivf.cpp: key %5% header.ts %1% num %2% den %3% res %4%\n") % get_uint64_le(&header.timestamp) % m_frame_rate_num % m_frame_rate_den % timestamp % ivf::is_keyframe(buffer, m_codec.get_type()));

  PTZR0->process(new packet_t(buffer, timestamp));

  return FILE_STATUS_MOREDATA;
}

void
ivf_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, m_codec.get_name());
}
