/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   VobSub packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>

#include "common/compression.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "input/subtitles.h"
#include "output/p_vobsub.h"

using namespace libmatroska;

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *_reader,
                                         const void *_idx_data,
                                         int _idx_data_size,
                                         track_info_c &_ti)
  : generic_packetizer_c(_reader, _ti)
  , idx_data(new memory_c((unsigned char *)safememdup(_idx_data, _idx_data_size), _idx_data_size, true))
{
  set_track_type(track_subtitle);
  set_default_compression_method(COMPRESSION_ZLIB);
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
}

void
vobsub_packetizer_c::set_headers() {
  set_codec_id(MKV_S_VOBSUB);
  set_codec_private(idx_data->get_buffer(), idx_data->get_size());

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
}

int
vobsub_packetizer_c::process(packet_cptr packet) {
  packet->duration_mandatory = true;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
vobsub_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    std::string &) {
  vobsub_packetizer_c *vsrc;

  vsrc = dynamic_cast<vobsub_packetizer_c *>(src);
  if (vsrc == nullptr)
    return CAN_CONNECT_NO_FORMAT;
  return CAN_CONNECT_YES;
}
