/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   the split point class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/split_point.h"
#include "common/strings/formatting.h"

std::string
split_point_c::str()
  const {
  return (boost::format("<%1% %2% once:%3% discard:%4% create_file:%5%>")
          % format_timecode(m_point)
          % (  duration          == m_type ? "duration"
             : size              == m_type ? "size"
             : timecode          == m_type ? "timecode"
             : chapter           == m_type ? "chapter"
             : parts             == m_type ? "part"
             : parts_frame_field == m_type ? "part(frame/field)"
             : frame_field       == m_type ? "frame/field"
             :                               "unknown")
          % m_use_once % m_discard % m_create_new_file).str();
}
