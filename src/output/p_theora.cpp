/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Theora video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/string_formatting.h"
#include "common/theora_common.h"
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
  if (packet->data->get_size() && (0x00 == (packet->data->get()[0] & 0x40)))
    packet->bref = VFT_IFRAME;
  else
    packet->bref = VFT_PFRAMEAUTOMATIC;

  packet->fref   = VFT_NOBFRAME;

  return video_packetizer_c::process(packet);
}

void
theora_video_packetizer_c::extract_aspect_ratio() {
  if (ti.display_dimensions_given || ti.aspect_ratio_given || (NULL == ti.private_data) || (0 == ti.private_size))
    return;

  memory_cptr private_data         = memory_cptr(new memory_c(ti.private_data, ti.private_size, false));
  std::vector<memory_cptr> packets = unlace_memory_xiph(private_data);

  std::vector<memory_cptr>::iterator i;
  mxforeach(i, packets) {
    if ((0 == (*i)->get_size()) || (THEORA_HEADERTYPE_IDENTIFICATION != (*i)->get()[0]))
      continue;

    try {
      theora_identification_header_t theora;
      theora_parse_identification_header((*i)->get(), (*i)->get_size(), theora);

      if ((0 == theora.display_width) || (0 == theora.display_height))
        return;

      ti.display_dimensions_given = true;
      ti.display_width            = theora.display_width;
      ti.display_height           = theora.display_height;

      mxinfo_tid(ti.fname, ti.id,
                 boost::format(Y("Extracted the aspect ratio information from the Theora video headers and set the display dimensions to %1%/%2%.\n"))
                 % theora.display_width % theora.display_height);
    } catch (...) {
    }

    return;
  }
}
