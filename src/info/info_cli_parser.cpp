/** \brief command line parsing

   mkvinfo -- info tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <boost/bind.hpp>
#include <stdexcept>
#include <typeinfo>

#include "common/ebml.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/translation.h"
#include "info/info_cli_parser.h"
#include "info/options.h"

info_cli_parser_c::info_cli_parser_c(const std::vector<std::string> &args)
  : cli_parser_c(args)
{
  verbose = 0;
}

#define OPT(spec, func, description) add_option(spec, boost::bind(&info_cli_parser_c::func, this), description)

void
info_cli_parser_c::init_parser() {
  add_information(YT("mkvinfo [options] <inname>"));

  add_section_header(YT("Options"));

#if defined(HAVE_QT) || defined(HAVE_WXWIDGETS)
  OPT("g|gui",          set_gui,          YT("Start the GUI (and open inname if it was given)."));
#endif
  OPT("c|checksum",     set_checksum,     YT("Calculate and display checksums of frame contents."));
  OPT("C|check-mode",   set_check_mode,   YT("Calculate and display checksums and use verbosity level 4."));
  OPT("s|summary",      set_summary,      YT("Only show summaries of the contents, not each element."));
  OPT("t|track-info",   set_track_info,   YT("Show statistics for each track in verbose mode."));
  OPT("x|hexdump",      set_hexdump,      YT("Show the first 16 bytes of each frame as a hex dump."));
  OPT("X|full-hexdump", set_full_hexdump, YT("Show all bytes of each frame as a hex dump."));
  OPT("z|size",         set_size,         YT("Show the size of each element including its header."));

  add_common_options();

  add_hook(cli_parser_c::ht_unknown_option, boost::bind(&info_cli_parser_c::set_file_name, this));
}

#undef OPT

void
info_cli_parser_c::set_gui() {
  if (!ui_graphical_available())
    mxerror("mkvinfo was compiled without GUI support.\n");

  m_options.m_use_gui = true;
}

void
info_cli_parser_c::set_checksum() {
  m_options.m_calc_checksums = true;
}

void
info_cli_parser_c::set_check_mode() {
  m_options.m_calc_checksums = true;
  verbose                    = 4;
}

void
info_cli_parser_c::set_summary() {
  m_options.m_calc_checksums = true;
  m_options.m_show_summary   = true;
}


void
info_cli_parser_c::set_hexdump() {
  m_options.m_show_hexdump = true;
}

void
info_cli_parser_c::set_full_hexdump() {
  m_options.m_show_hexdump     = true;
  m_options.m_hexdump_max_size = INT_MAX;
}

void
info_cli_parser_c::set_size() {
  m_options.m_show_size = true;
}

void
info_cli_parser_c::set_track_info() {
  m_options.m_show_track_info = true;
  if (0 == verbose)
    verbose = 1;
}

void
info_cli_parser_c::set_file_name() {
  if (!m_options.m_file_name.empty())
    mxerror(Y("Only one input file is allowed.\n"));

  m_options.m_file_name = m_current_arg;
}

options_c
info_cli_parser_c::run() {
  init_parser();
  parse_args();

  m_options.m_verbose = verbose;
  verbose             = 0;

  return m_options;
}

