/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the simple text subtitle packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_TEXTSUBS_H
#define MTX_P_TEXTSUBS_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"

class textsubs_packetizer_c: public generic_packetizer_c {
private:
  int m_packetno;
  charset_converter_cptr m_cc_utf8;
  std::string m_codec_id;
  bool m_recode;

public:
  textsubs_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, const char *codec_id, bool recode, bool is_utf8);
  virtual ~textsubs_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("text subtitles");
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

private:
  static boost::regex s_re_remove_cr, s_re_translate_nl, s_re_remove_trailing_nl;
};

#endif  // MTX_P_TEXTSUBS_H
