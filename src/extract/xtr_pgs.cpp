/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Extraction of Blu-Ray subtitles.

   Written by Moritz Bunkus and Mike Chen.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <matroska/KaxBlock.h>

#include "common/ebml.h"
#include "common/endian.h"
#include "extract/xtr_pgs.h"

xtr_pgs_c::xtr_pgs_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
{
}

void
xtr_pgs_c::handle_frame(xtr_frame_t &f) {
  m_content_decoder.reverse(f.frame, CONTENT_ENCODING_SCOPE_BLOCK);

  binary sup_header[10];
  binary *mybuffer = f.frame->get_buffer();
  int data_size    = f.frame->get_size();
  int offset       = 0;
  uint64_t pts     = (f.timecode * 9) / 100000;

  memcpy(sup_header, "PG", 2);
  put_uint32_be(&sup_header[2], (uint32)pts);
  put_uint32_be(&sup_header[6], 0);

  while ((offset + 3) <= data_size) {
    int packet_size = std::min(static_cast<int>(get_uint16_be(mybuffer + offset + 1) + 3), data_size - offset);

    m_out->write(sup_header, 10);
    m_out->write(mybuffer + offset, packet_size);
    offset += packet_size;
  }
}
