/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/base64.h"
#include "common/extern_data.h"
#include "common/locale.h"
#include "common/matroska.h"
#include "input/r_ssa.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"


int
ssa_reader_c::probe_file(mm_text_io_c *in,
                         uint64_t) {
  return ssa_parser_c::probe(in);
}

ssa_reader_c::ssa_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
{
}

void
ssa_reader_c::read_headers() {
  mm_text_io_cptr text_in;

  try {
    text_in = std::shared_ptr<mm_text_io_c>(new mm_text_io_c(m_in.get(), false));
  } catch (...) {
    throw mtx::input::open_x();
  }

  if (!ssa_reader_c::probe_file(text_in.get(), 0))
    throw mtx::input::invalid_format_x();

  charset_converter_cptr cc_utf8 = map_has_key(m_ti.m_sub_charsets,  0) ? charset_converter_c::init(m_ti.m_sub_charsets[ 0])
                                 : map_has_key(m_ti.m_sub_charsets, -1) ? charset_converter_c::init(m_ti.m_sub_charsets[-1])
                                 : text_in->get_byte_order() != BO_NONE ? charset_converter_c::init("UTF-8")
                                 :                                        g_cc_local_utf8;

  m_ti.m_id = 0;
  m_subs    = ssa_parser_cptr(new ssa_parser_c(this, text_in.get(), m_ti.m_fname, 0));

  m_subs->set_charset_converter(cc_utf8);
  m_subs->parse();

  show_demuxer_info();
}

ssa_reader_c::~ssa_reader_c() {
}

void
ssa_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('s', 0) || (NPTZR() != 0))
    return;

  std::string global = m_subs->get_global();
  add_packetizer(new textsubs_packetizer_c(this, m_ti, m_subs->is_ass() ?  MKV_S_TEXTASS : MKV_S_TEXTSSA, global.c_str(), global.length(), false, false));
  show_packetizer_info(0, PTZR0);
}

file_status_e
ssa_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (!m_subs->empty())
    m_subs->process((textsubs_packetizer_c *)PTZR0);

  return m_subs->empty() ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

int
ssa_reader_c::get_progress() {
  int num_entries = m_subs->get_num_entries();

  return 0 == num_entries ? 100 : 100 * m_subs->get_num_processed() / num_entries;
}

void
ssa_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_SUBTITLES, "SSA/ASS");

  size_t i;
  for (i = 0; i < g_attachments.size(); i++)
    id_result_attachment(g_attachments[i].ui_id, g_attachments[i].mime_type, g_attachments[i].data->get_size(), g_attachments[i].name, g_attachments[i].description);
}
