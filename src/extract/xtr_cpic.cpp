/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>

#include "common/corepicture.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/strings/formatting.h"
#include "extract/xtr_cpic.h"

xtr_cpic_c::xtr_cpic_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_file_name_root(tspec.out_name)
  , m_frame_counter(0)
{
}

void
xtr_cpic_c::handle_frame(memory_cptr &frame,
                         KaxBlockAdditions *additions,
                         int64_t timecode,
                         int64_t duration,
                         int64_t bref,
                         int64_t fref,
                         bool keyframe,
                         bool discardable,
                         bool references_valid) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  binary *mybuffer = frame->get_buffer();
  int data_size    = frame->get_size();
  if (2 > data_size) {
    mxinfo(boost::format(Y("CorePicture frame %1% not supported.\n")) % m_frame_counter);
    ++m_frame_counter;
    return;
  }

  int header_size = get_uint16_be(mybuffer);
  if (header_size >= data_size) {
    mxinfo(boost::format(Y("CorePicture frame %1% has an invalid header size %2%.\n")) % m_frame_counter % header_size);
    ++m_frame_counter;
    return;
  }

  std::string frame_file_name = m_file_name_root + "_" + to_string(m_frame_counter);
  if (7 == header_size) {
    uint8 picture_type = mybuffer[6];
    if (COREPICTURE_TYPE_JPEG == picture_type)
      frame_file_name += ".jpeg";
    else if (COREPICTURE_TYPE_PNG == picture_type)
      frame_file_name += ".png";
  }

  m_out = mm_file_io_c::open(frame_file_name, MODE_CREATE);
  m_out->write(&mybuffer[header_size], data_size - header_size);
  m_out.clear();

  ++m_frame_counter;
}

void
xtr_cpic_c::create_file(xtr_base_c *master,
                        KaxTrackEntry &track) {
  init_content_decoder(track);
}
