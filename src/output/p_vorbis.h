/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the Vorbis packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VORBIS_H
#define __P_VORBIS_H

#include "common/common_pch.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "merge/pr_generic.h"
#include "merge/pr_generic.h"

namespace mtx {
  namespace output {
    class vorbis_x: public exception {
    protected:
      std::string m_message;
    public:
      vorbis_x(const std::string &message)  : m_message(message)       { }
      vorbis_x(const boost::format &message): m_message(message.str()) { }
      virtual ~vorbis_x() throw() { }

      virtual const char *what() const throw() {
        return m_message.c_str();
      }
    };
  }
}

class vorbis_packetizer_c: public generic_packetizer_c {
private:
  int64_t m_previous_bs, m_samples, m_previous_samples_sum, m_previous_timecode, m_timecode_offset;
  std::vector<memory_cptr> m_headers;
  vorbis_info m_vi;
  vorbis_comment m_vc;

public:
  vorbis_packetizer_c(generic_reader_c *p_reader,  track_info_c &p_ti,
                      unsigned char *d_header,     int l_header,
                      unsigned char *d_comments,   int l_comments,
                      unsigned char *d_codecsetup, int l_codecsetup);
  virtual ~vorbis_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("Vorbis") : "Vorbis";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

  virtual bool is_compatible_with(output_compatibility_e compatibility);
};

#endif  // __P_VORBIS_H
