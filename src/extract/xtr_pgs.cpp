/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Extraction of Blu-Ray subtitles.

   Written by Moritz Bunkus and Mike Chen.
*/

#include "common/os.h"

#include <matroska/KaxBlock.h>

#include "common/common.h"
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
xtr_pgs_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  binary sup_header[10];
  binary *mybuffer = frame->get_buffer();
  int data_size    = frame->get_size();
  int offset       = 0;
  uint64_t pts     = (timecode * 9) / 100000;

  memcpy(sup_header, "PG", 2);
  put_uint32_be(&sup_header[2], (uint32)pts);
  put_uint32_be(&sup_header[6], 0);

  while (offset < data_size) {
    int packet_size = get_uint16_be(mybuffer + offset + 1) + 3;

    if ((offset + packet_size) >= data_size)
      return;

    m_out->write(sup_header, 10);
    m_out->write(mybuffer + offset, packet_size);
    offset += packet_size;
  }
}
