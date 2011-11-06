/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Kate packetizer

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ogg/ogg.h>

#include "common/endian.h"
#include "common/kate.h"
#include "common/matroska.h"
#include "merge/pr_generic.h"
#include "output/p_kate.h"

using namespace libmatroska;

kate_packetizer_c::kate_packetizer_c(generic_reader_c *p_reader,
                                     track_info_c &p_ti,
                                     const void *global_data,
                                     int global_size)
  : generic_packetizer_c(p_reader, p_ti)
  , m_global_data(new memory_c((unsigned char *)safememdup(global_data ? global_data : m_ti.m_private_data,
                                                           global_data ? global_size : m_ti.m_private_size),
                               global_data ? global_size : m_ti.m_private_size, true))
  , m_previous_timecode(0)
{
  set_track_type(track_subtitle);

  // the number of headers to expect is stored in the first header
  memory_cptr temp(new memory_c((unsigned char *)m_global_data->get_buffer(), m_global_data->get_size(), false));
  std::vector<memory_cptr> blocks = unlace_memory_xiph(temp);

  kate_parse_identification_header(blocks[0]->get_buffer(), blocks[0]->get_size(), m_kate_id);
  if (blocks.size() != m_kate_id.nheaders)
    throw false;

  set_language((const char *)m_kate_id.language);
  int i;
  for (i = 0; i < m_kate_id.nheaders; ++i)
    m_headers.push_back(memory_cptr(blocks[i]->clone()));
}

kate_packetizer_c::~kate_packetizer_c() {
}

void
kate_packetizer_c::set_headers() {
  set_codec_id(MKV_S_KATE);

  memory_cptr codec_private = lace_memory_xiph(m_headers);
  set_codec_private(codec_private->get_buffer(), codec_private->get_size());

  generic_packetizer_c::set_headers();
}

int
kate_packetizer_c::process(packet_cptr packet) {
  if (packet->data->get_size() < (1 + 3 * sizeof(int64_t))) {
    /* end packet is 1 byte long and has type 0x7f */
    if ((packet->data->get_size() == 1) && (packet->data->get_buffer()[0] == 0x7f)) {
      packet->timecode           = m_previous_timecode;
      packet->duration           = 1;
      packet->duration_mandatory = true;
      add_packet(packet);

    } else
      mxwarn_tid(m_ti.m_fname, m_ti.m_id, Y("Kate packet is too small and is being skipped.\n"));

    return FILE_STATUS_MOREDATA;
  }

  int64_t start_time         = get_uint64_le(packet->data->get_buffer() + 1);
  int64_t duration           = get_uint64_le(packet->data->get_buffer() + 1 + sizeof(int64_t));

  double scaled_timecode     = start_time * (double)m_kate_id.gden / (double)m_kate_id.gnum;
  double scaled_duration     = duration   * (double)m_kate_id.gden / (double)m_kate_id.gnum;

  packet->timecode           = (int64_t)(scaled_timecode * 1000000000ll);
  packet->duration           = (int64_t)(scaled_duration * 1000000000ll);
  packet->duration_mandatory = true;
  packet->gap_following      = true;

  int64_t end                = packet->timecode + packet->duration;
  if (end > m_previous_timecode)
    m_previous_timecode = end;

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
kate_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  std::string &error_message) {
  kate_packetizer_c *psrc = dynamic_cast<kate_packetizer_c *>(src);
  if (NULL == psrc)
    return CAN_CONNECT_NO_FORMAT;

  if (   ((NULL == m_ti.m_private_data) && (NULL != src->m_ti.m_private_data))
      || ((NULL != m_ti.m_private_data) && (NULL == src->m_ti.m_private_data))
      || (m_ti.m_private_size != src->m_ti.m_private_size)) {
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % m_ti.m_private_size % src->m_ti.m_private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}
