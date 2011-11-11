/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   TTA demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/error.h"
#include "common/id3.h"
#include "common/math.h"
#include "input/r_tta.h"
#include "merge/output_control.h"
#include "output/p_tta.h"

int
tta_reader_c::probe_file(mm_io_c *in,
                         uint64_t size) {
  unsigned char buf[4];

  if (26 > size)
    return 0;
  try {
    in->setFilePointer(0, seek_beginning);
    int tag_size = skip_id3v2_tag(*in);
    if (-1 == tag_size)
      return 0;
    if (in->read(buf, 4) != 4)
      return 0;
    in->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  if (!strncmp((char *)buf, "TTA1", 4))
    return 1;
  return 0;
}

tta_reader_c::tta_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
{
}

void
tta_reader_c::read_headers() {
  if (g_identifying)
    return;

  try {
    int tag_size = skip_id3v2_tag(*m_in);
    if (0 > tag_size)
      mxerror_fn(m_ti.m_fname, boost::format(Y("tta_reader: tag_size < 0 in the c'tor. %1%\n")) % BUGMSG);
    m_size -= tag_size;

    if (m_in->read(&header, sizeof(tta_file_header_t)) != sizeof(tta_file_header_t))
      mxerror_fn(m_ti.m_fname, Y("The file header is too short.\n"));

    uint64_t seek_sum  = m_in->getFilePointer() + 4 - tag_size;
    m_size            -= id3_tag_present_at_end(*m_in);

    uint32_t seek_point;

    do {
      seek_point  = m_in->read_uint32_le();
      seek_sum   += seek_point + 4;
      seek_points.push_back(seek_point);
    } while (seek_sum < m_size);

    mxverb(2,
           boost::format("tta: ch %1% bps %2% sr %3% dl %4% seek_sum %5% size %6% num %7%\n")
           % get_uint16_le(&header.channels)    % get_uint16_le(&header.bits_per_sample)
           % get_uint32_le(&header.sample_rate) % get_uint32_le(&header.data_length)
           % seek_sum      % m_size             % seek_points.size());

    if (seek_sum != m_size)
      mxerror_fn(m_ti.m_fname, Y("The seek table in this TTA file seems to be broken.\n"));

    m_in->skip(4);

    pos             = 0;
    m_ti.m_id       = 0;        // ID for this track.

  } catch (...) {
    throw mtx::input::open_x();
  }
  show_demuxer_info();
}

tta_reader_c::~tta_reader_c() {
}

void
tta_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new tta_packetizer_c(this, m_ti, get_uint16_le(&header.channels), get_uint16_le(&header.bits_per_sample), get_uint32_le(&header.sample_rate)));
  show_packetizer_info(0, PTZR0);
}

file_status_e
tta_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (seek_points.size() <= pos)
    return flush_packetizers();

  unsigned char *buf = (unsigned char *)safemalloc(seek_points[pos]);
  int nread          = m_in->read(buf, seek_points[pos]);

  if (0 >= nread)
    return flush_packetizers();
  pos++;

  memory_cptr mem(new memory_c(buf, nread, true));
  if (seek_points.size() <= pos) {
    double samples_left = (double)get_uint32_le(&header.data_length) - (seek_points.size() - 1) * TTA_FRAME_TIME * get_uint32_le(&header.sample_rate);
    mxverb(2, boost::format("tta: samples_left %1%\n") % samples_left);

    PTZR0->process(new packet_t(mem, -1, irnd(samples_left * 1000000000.0l / get_uint32_le(&header.sample_rate))));
  } else
    PTZR0->process(new packet_t(mem));

  return seek_points.size() <= pos ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

void
tta_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "TTA");
}
