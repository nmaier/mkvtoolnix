/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <string>

#include "common/base64.h"
#include "common/extern_data.h"
#include "common/locale.h"
#include "common/matroska.h"
#include "input/r_ssa.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"


int
ssa_reader_c::probe_file(mm_text_io_c *io,
                         int64_t) {
  return ssa_parser_c::probe(io);
}

ssa_reader_c::ssa_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
{
  counted_ptr<mm_text_io_c> io;

  try {
    io = counted_ptr<mm_text_io_c>(new mm_text_io_c(new mm_file_io_c(ti.m_fname)));
  } catch (...) {
    throw error_c(Y("ssa_reader: Could not open the source file."));
  }

  if (!ssa_reader_c::probe_file(io.get_object(), 0))
    throw error_c(Y("ssa_reader: Source is not a valid SSA/ASS file."));

  charset_converter_cptr cc_utf8 = map_has_key(ti.m_sub_charsets,  0) ? charset_converter_c::init(ti.m_sub_charsets[ 0])
                                 : map_has_key(ti.m_sub_charsets, -1) ? charset_converter_c::init(ti.m_sub_charsets[-1])
                                 : io->get_byte_order() != BO_NONE    ? charset_converter_c::init("UTF-8")
                                 :                                      g_cc_local_utf8;

  ti.m_id = 0;
  m_subs  = ssa_parser_cptr(new ssa_parser_c(this, io.get_object(), ti.m_fname, 0));

  m_subs->set_charset_converter(cc_utf8);
  m_subs->parse();

  if (verbose)
    mxinfo_fn(ti.m_fname, Y("Using the SSA/ASS subtitle reader.\n"));
}

ssa_reader_c::~ssa_reader_c() {
}

void
ssa_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  std::string global = m_subs->get_global();
  add_packetizer(new textsubs_packetizer_c(this, ti, m_subs->is_ass() ?  MKV_S_TEXTASS : MKV_S_TEXTSSA, global.c_str(), global.length(), false, false));
  mxinfo_tid(ti.m_fname, 0, Y("Using the text subtitle output module.\n"));
}

file_status_e
ssa_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (m_subs->empty())
    return FILE_STATUS_DONE;

  m_subs->process((textsubs_packetizer_c *)PTZR0);

  if (m_subs->empty()) {
    flush_packetizers();
    return FILE_STATUS_DONE;
  }

  return FILE_STATUS_MOREDATA;
}

int
ssa_reader_c::get_progress() {
  int num_entries = m_subs->get_num_entries();

  return 0 == num_entries ? 100 : 100 * m_subs->get_num_processed() / num_entries;
}

void
ssa_reader_c::identify() {
  id_result_container("SSA/ASS");
  id_result_track(0, ID_RESULT_TRACK_SUBTITLES, "SSA/ASS");

  int i;
  for (i = 0; i < g_attachments.size(); i++)
    id_result_attachment(g_attachments[i].ui_id, g_attachments[i].mime_type, g_attachments[i].data->get_size(), g_attachments[i].name, g_attachments[i].description);
}
