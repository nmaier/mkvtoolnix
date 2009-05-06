/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   PASSTHROUGH output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/matroska.h"
#include "merge/pr_generic.h"
#include "output/p_passthrough.h"

using namespace libmatroska;

passthrough_packetizer_c::passthrough_packetizer_c(generic_reader_c *p_reader,
                                                   track_info_c &p_ti)
  throw (error_c)
  : generic_packetizer_c(p_reader, p_ti)
{
  timecode_factory_application_mode = TFA_FULL_QUEUEING;
}

void
passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int
passthrough_packetizer_c::process(packet_cptr packet) {
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

#define CMP(member) (member != psrc->member)

connection_result_e
passthrough_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                         string &error_message) {
  passthrough_packetizer_c *psrc = dynamic_cast<passthrough_packetizer_c *>(src);
  if (NULL == psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_id(hcodec_id, psrc->hcodec_id);

  if (CMP(htrack_type) || CMP(hcodec_id))
    return CAN_CONNECT_NO_PARAMETERS;

  if (   ((NULL == ti.private_data) && (NULL != psrc->ti.private_data))
      || ((NULL != ti.private_data) && (NULL == psrc->ti.private_data))
      || (ti.private_size != psrc->ti.private_size)
      || (   (NULL != ti.private_data)
          && (NULL != psrc->ti.private_data)
          && (ti.private_size == psrc->ti.private_size)
          && memcmp(ti.private_data, psrc->ti.private_data, ti.private_size))) {
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % ti.private_size % psrc->ti.private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  switch (htrack_type) {
    case track_video:
      connect_check_v_width(hvideo_pixel_width,      psrc->hvideo_pixel_width);
      connect_check_v_height(hvideo_pixel_height,    psrc->hvideo_pixel_height);
      connect_check_v_dwidth(hvideo_display_width,   psrc->hvideo_display_width);
      connect_check_v_dheight(hvideo_display_height, psrc->hvideo_display_height);
      if (CMP(htrack_min_cache) || CMP(htrack_max_cache) || CMP(htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_audio:
      connect_check_a_samplerate(haudio_sampling_freq, psrc->haudio_sampling_freq);
      connect_check_a_channels(haudio_channels,        psrc->haudio_channels);
      connect_check_a_bitdepth(haudio_bit_depth,       psrc->haudio_bit_depth);
      if (CMP(htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_subtitle:
      break;

    default:
      break;
  }

  return CAN_CONNECT_YES;
}
