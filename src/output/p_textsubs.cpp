/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Subripper subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "merge/connection_checks.h"
#include "merge/packet_extensions.h"
#include "output/p_textsubs.h"

using namespace libmatroska;

boost::regex textsubs_packetizer_c::s_re_remove_cr("\r", boost::regex::perl);
boost::regex textsubs_packetizer_c::s_re_remove_trailing_nl("\n+$", boost::regex::perl);
boost::regex textsubs_packetizer_c::s_re_translate_nl("\n", boost::regex::perl);

textsubs_packetizer_c::textsubs_packetizer_c(generic_reader_c *p_reader,
                                             track_info_c &p_ti,
                                             const char *codec_id,
                                             bool recode,
                                             bool is_utf8)
  : generic_packetizer_c(p_reader, p_ti)
  , m_packetno(0)
  , m_codec_id(codec_id)
  , m_recode(recode)
{
  if (m_recode)
    m_cc_utf8 = charset_converter_c::init((m_ti.m_sub_charset != "") || !is_utf8 ? m_ti.m_sub_charset : "UTF-8");

  set_track_type(track_subtitle);
  if (m_codec_id == MKV_S_TEXTUSF)
    set_default_compression_method(COMPRESSION_ZLIB);
}

textsubs_packetizer_c::~textsubs_packetizer_c() {
}

void
textsubs_packetizer_c::set_headers() {
  set_codec_id(m_codec_id);
  set_codec_private(m_ti.m_private_data);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
}

int
textsubs_packetizer_c::process(packet_cptr packet) {
  ++m_packetno;

  if (0 > packet->duration) {
    subtitle_number_packet_extension_c *extension = dynamic_cast<subtitle_number_packet_extension_c *>(packet->find_extension(packet_extension_c::SUBTITLE_NUMBER));
    mxwarn_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Ignoring an entry which starts after it ends (%1%).\n")) % (extension ? extension->get_number() : static_cast<unsigned int>(m_packetno)));
    return FILE_STATUS_MOREDATA;
  }

  packet->duration_mandatory = true;

  std::string subs((char *)packet->data->get_buffer());

  subs = boost::regex_replace(subs, s_re_remove_cr,          "",     boost::match_default | boost::match_single_line);
  subs = boost::regex_replace(subs, s_re_remove_trailing_nl, "",     boost::match_default | boost::match_single_line);
  subs = boost::regex_replace(subs, s_re_translate_nl,       "\r\n", boost::match_default | boost::match_single_line);

  if (m_recode)
    subs = m_cc_utf8->utf8(subs);

  packet->data = memory_cptr(new memory_c((unsigned char *)subs.c_str(), subs.length(), false));

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
textsubs_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                      std::string &error_message) {
  textsubs_packetizer_c *psrc = dynamic_cast<textsubs_packetizer_c *>(src);
  if (!psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_private(src);

  return CAN_CONNECT_YES;
}
