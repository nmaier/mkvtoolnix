/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Theora video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/strings/formatting.h"
#include "common/theora.h"
#include "output/p_theora.h"

theora_video_packetizer_c::
theora_video_packetizer_c(generic_reader_c *p_reader,
                          track_info_c &p_ti,
                          double fps,
                          int width,
                          int height)
  : video_packetizer_c(p_reader, p_ti, MKV_V_THEORA, fps, width, height)
{
}

void
theora_video_packetizer_c::set_headers() {
  extract_aspect_ratio();
  video_packetizer_c::set_headers();
}

int
theora_video_packetizer_c::process(packet_cptr packet) {
  if (packet->data->get_size() && (0x00 == (packet->data->get_buffer()[0] & 0x40)))
    packet->bref = VFT_IFRAME;
  else
    packet->bref = VFT_PFRAMEAUTOMATIC;

  packet->fref   = VFT_NOBFRAME;

  return video_packetizer_c::process(packet);
}

void
theora_video_packetizer_c::extract_aspect_ratio() {
  if (display_dimensions_or_aspect_ratio_set() || !m_ti.m_private_data || (0 == m_ti.m_private_data->get_size()))
    return;

  auto packets = unlace_memory_xiph(m_ti.m_private_data);

  for (auto &packet : packets) {
    if ((0 == packet->get_size()) || (THEORA_HEADERTYPE_IDENTIFICATION != packet->get_buffer()[0]))
      continue;

    try {
      theora_identification_header_t theora;
      theora_parse_identification_header(packet->get_buffer(), packet->get_size(), theora);

      if ((0 == theora.display_width) || (0 == theora.display_height))
        return;

      set_video_display_dimensions(theora.display_width, theora.display_height, PARAMETER_SOURCE_BITSTREAM);

      mxinfo_tid(m_ti.m_fname, m_ti.m_id,
                 boost::format(Y("Extracted the aspect ratio information from the Theora video headers and set the display dimensions to %1%/%2%.\n"))
                 % theora.display_width % theora.display_height);
    } catch (...) {
    }

    return;
  }
}
