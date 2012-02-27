/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ebml.h"
#include "common/endian.h"
#include "common/math.h"
#include "extract/xtr_ivf.h"

xtr_ivf_c::xtr_ivf_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_frame_rate_num(0)
  , m_frame_rate_den(0)
  , m_frame_count(0)
{
}

void
xtr_ivf_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);

  uint64_t default_duration = kt_get_default_duration(track);
  default_duration          = 0 == default_duration ? 1 : default_duration;
  uint64_t gcd              = boost::math::gcd(static_cast<uint64_t>(100000000), default_duration);
  m_frame_rate_num          = 1000000000ull    / gcd;
  m_frame_rate_den          = default_duration / gcd;

  if ((0xffff < m_frame_rate_num) || (0xffff < m_frame_rate_den)) {
    uint64_t scale   = std::max(m_frame_rate_num, m_frame_rate_den) / 0xffff + 1;
    m_frame_rate_num = 1000000000ull    / scale;
    m_frame_rate_den = default_duration / scale;

    if (0 == m_frame_rate_den)
      m_frame_rate_den = 1;
  }

  if (NULL != master)
    mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because "
                            "track %4% with the CodecID '%5%' is already being written to the same file.\n"))
            % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

  memcpy(m_file_header.file_magic, "DKIF", 4);
  memcpy(m_file_header.fourcc,     "VP80", 4);

  put_uint16_le(&m_file_header.header_size,    sizeof(m_file_header));
  put_uint16_le(&m_file_header.width,          kt_get_v_pixel_width(track));
  put_uint16_le(&m_file_header.height,         kt_get_v_pixel_height(track));
  put_uint16_le(&m_file_header.frame_rate_num, m_frame_rate_num);
  put_uint16_le(&m_file_header.frame_rate_den, m_frame_rate_den);

  m_out->write(&m_file_header, sizeof(m_file_header));
}

void
xtr_ivf_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *,
                        int64_t timecode,
                        int64_t,
                        int64_t,
                        int64_t,
                        bool,
                        bool,
                        bool) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  uint64_t frame_number = timecode * m_frame_rate_num / m_frame_rate_den / 1000000000ull;

  mxverb(2, boost::format("timecode %1% num %2% den %3% frame_number %4% calculated back %5%\n")
         % timecode % m_frame_rate_num % m_frame_rate_den % frame_number
         % (frame_number * 1000000000ull * m_frame_rate_den / m_frame_rate_num));

  ivf::frame_header_t frame_header;
  put_uint32_le(&frame_header.frame_size, frame->get_size());
  put_uint32_le(&frame_header.timestamp,  frame_number);

  m_out->write(&frame_header,       sizeof(frame_header));
  m_out->write(frame->get_buffer(), frame->get_size());

  ++m_frame_count;
}

void
xtr_ivf_c::finish_file() {
  put_uint32_le(&m_file_header.frame_count, m_frame_count);

  m_out->setFilePointer(0, seek_beginning);
  m_out->write(&m_file_header, sizeof(m_file_header));
}

