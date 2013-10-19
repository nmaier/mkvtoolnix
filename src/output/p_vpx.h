/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the VPX output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_OUTPUT_P_VPX_H
#define MTX_OUTPUT_P_VPX_H

#include "common/common_pch.h"

#include "common/ivf.h"
#include "merge/pr_generic.h"

class vpx_video_packetizer_c: public generic_packetizer_c {
protected:
  int64_t m_previous_timecode;
  ivf::codec_e m_codec;

public:
  vpx_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, ivf::codec_e p_codec);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("VP8/VP9");
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
  virtual bool is_compatible_with(output_compatibility_e compatibility);
};

#endif // MTX_OUTPUT_P_VPX_H
