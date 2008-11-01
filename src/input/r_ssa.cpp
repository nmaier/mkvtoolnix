/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <boost/regex.hpp>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "base64.h"
#include "extern_data.h"
#include "pr_generic.h"
#include "r_ssa.h"
#include "matroska.h"
#include "output_control.h"

using namespace std;

int
ssa_reader_c::probe_file(mm_text_io_c *io,
                         int64_t) {
  return ssa_parser_c::probe(io);
}

ssa_reader_c::ssa_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
{
  auto_ptr<mm_text_io_c> io;

  try {
    io = auto_ptr<mm_text_io_c>(new mm_text_io_c(new mm_file_io_c(ti.fname)));
  } catch (...) {
    throw error_c(Y("ssa_reader: Could not open the source file."));
  }

  if (!ssa_reader_c::probe_file(io.get(), 0))
    throw error_c(Y("ssa_reader: Source is not a valid SSA/ASS file."));

  int cc_utf8 = map_has_key(ti.sub_charsets, 0)  ? utf8_init(ti.sub_charsets[0])
    :           map_has_key(ti.sub_charsets, -1) ? utf8_init(ti.sub_charsets[-1])
    :           io->get_byte_order() != BO_NONE  ? utf8_init("UTF-8")
    :                                              cc_local_utf8;

  ti.id  = 0;
  m_subs = ssa_parser_cptr(new ssa_parser_c(io.get(), ti.fname, 0));

  m_subs->set_iconv_handle(cc_utf8);
  m_subs->set_ignore_attachments(ti.no_attachments);
  m_subs->parse();

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the SSA/ASS subtitle reader.\n"));
}

ssa_reader_c::~ssa_reader_c() {
}

void
ssa_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  std::string global = m_subs->get_global();
  add_packetizer(new textsubs_packetizer_c(this, ti, m_subs->is_ass() ?  MKV_S_TEXTASS : MKV_S_TEXTSSA, global.c_str(), global.length(), false, false));
  mxinfo_tid(ti.fname, 0, Y("Using the text subtitle output module.\n"));
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
}
