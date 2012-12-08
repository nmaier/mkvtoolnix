/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the Theora video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_THEORA_H
#define MTX_P_THEORA_H

#include "common/common_pch.h"

#include "output/p_video.h"

class theora_video_packetizer_c: public video_packetizer_c {
public:
  theora_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height);
  virtual void set_headers();
  virtual int process(packet_cptr packet);

  virtual const std::string get_format_name(bool translate = true) const {
    return translate ? Y("Theora") : "Theora";
  }

protected:
  virtual void extract_aspect_ratio();
};

#endif  // MTX_P_THEORA_H
