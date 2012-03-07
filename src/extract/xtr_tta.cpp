/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <time.h>

#include "common/checksums.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/mm_write_buffer_io.h"
#include "common/tta.h"
#include "extract/xtr_tta.h"

const double xtr_tta_c::ms_tta_frame_time = 1.04489795918367346939l;

xtr_tta_c::xtr_tta_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_previous_duration(0)
  , m_bps(0)
  , m_channels(0)
  , m_sfreq(0)
  , m_temp_file_name((boost::format("mkvextract-%1%-temp-tta-%2%") % tid % time(nullptr)).str())
{
}

void
xtr_tta_c::create_file(xtr_base_c *,
                       KaxTrackEntry &track) {
  try {
    m_out = mm_write_buffer_io_c::open(m_temp_file_name, 5 * 1024 * 1024);
  } catch (...) {
    mxerror(boost::format(Y("Failed to create the temporary file '%1%': %2% (%3%)\n")) % m_temp_file_name % errno % strerror(errno));
  }

  m_bps      = kt_get_a_bps(track);
  m_channels = kt_get_a_channels(track);
  m_sfreq    = (int)kt_get_a_sfreq(track);
}

void
xtr_tta_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *,
                        int64_t,
                        int64_t duration,
                        int64_t,
                        int64_t,
                        bool,
                        bool,
                        bool) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  m_frame_sizes.push_back(frame->get_size());
  m_out->write(frame);

  if (0 < duration)
    m_previous_duration = duration;
}

void
xtr_tta_c::finish_file() {
  m_out.clear();

  mm_io_cptr in;
  try {
    in = mm_file_io_c::open(m_temp_file_name);
  } catch (...) {
    mxerror(boost::format(Y("The temporary file '%1%' could not be opened for reading (%2%).\n")) % m_temp_file_name % strerror(errno));
  }

  try {
    m_out = mm_write_buffer_io_c::open(m_file_name, 5 * 1024 * 1024);
  } catch (...) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for writing (%2%).\n")) % m_file_name % strerror(errno));
  }

  tta_file_header_t tta_header;
  memcpy(tta_header.signature, "TTA1", 4);
  if (3 != m_bps)
    put_uint16_le(&tta_header.audio_format, 1);
  else
    put_uint16_le(&tta_header.audio_format, 3);
  put_uint16_le(&tta_header.channels, m_channels);
  put_uint16_le(&tta_header.bits_per_sample, m_bps);
  put_uint32_le(&tta_header.sample_rate, m_sfreq);

  if (0 >= m_previous_duration)
    m_previous_duration = (int64_t)(TTA_FRAME_TIME * m_sfreq) * 1000000000ll;
  put_uint32_le(&tta_header.data_length, (uint32_t)(m_sfreq * (TTA_FRAME_TIME * (m_frame_sizes.size() - 1) + (double)m_previous_duration / 1000000000.0l)));
  put_uint32_le(&tta_header.crc, 0xffffffff ^ crc_calc(crc_get_table(CRC_32_IEEE_LE), 0xffffffff, (unsigned char *)&tta_header, sizeof(tta_file_header_t) - 4));

  m_out->write(&tta_header, sizeof(tta_file_header_t));

  unsigned char *buffer = (unsigned char *)safemalloc(m_frame_sizes.size() * 4);
  size_t k;
  for (k = 0; m_frame_sizes.size() > k; ++k)
    put_uint32_le(buffer + 4 * k, m_frame_sizes[k]);

  m_out->write(buffer, m_frame_sizes.size() * 4);

  m_out->write_uint32_le(0xffffffff ^ crc_calc(crc_get_table(CRC_32_IEEE_LE), 0xffffffff, buffer, m_frame_sizes.size() * 4));

  safefree(buffer);

  mxinfo(boost::format(Y("\nThe temporary TTA file for track ID %1% is being copied into the final TTA file. This may take some time.\n")) % m_tid);

  buffer = (unsigned char *)safemalloc(128000);
  int nread;
  do {
    nread = in->read(buffer, 128000);
    m_out->write(buffer, nread);
  } while (nread == 128000);

  m_out.clear();
  unlink(m_temp_file_name.c_str());
}
