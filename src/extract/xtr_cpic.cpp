/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "os.h"

#include <matroska/KaxBlock.h>

#include "common.h"
#include "commonebml.h"
#include "xtr_cpic.h"
#include "r_corepicture.h"

xtr_cpic_c::xtr_cpic_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  file_name_root(tspec.out_name), frame_counter(0) {
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

  binary *mybuffer;
  int data_size, header_size;
  string file_name;

  content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  mybuffer = frame->get();
  data_size = frame->get_size();
  if (data_size < 2) {
    mxinfo("CorePicture frame %d not supported.\n", frame_counter);
    frame_counter++;
    return;
  }

  header_size = get_uint16_be(mybuffer);
  if (header_size >= data_size) {
    mxinfo("CorePicture frame %d has an invalid header size %d.\n", frame_counter, header_size);
    frame_counter++;
    return;
  }

  file_name = file_name_root + "_" + to_string(frame_counter);
  if (7 == header_size) {
    uint8 picture_type = mybuffer[6];
    if (COREPICTURE_TYPE_JPEG == picture_type)
      file_name += ".jpeg";
    else if (COREPICTURE_TYPE_PNG == picture_type)
      file_name += ".png";
  }

 out = new mm_file_io_c(file_name, MODE_CREATE);
 out->write(&mybuffer[header_size], data_size - header_size);
 delete out;
 out = NULL;

 frame_counter++;
}

void
xtr_cpic_c::create_file(xtr_base_c *_master,
                        KaxTrackEntry &track) {
  init_content_decoder(track);
}
