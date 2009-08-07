/** \brief command line parsing

   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <algorithm>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <string>
#include <typeinfo>
#include <vector>

#include "common/common.h"
#include "common/ebml.h"
#include "common/translation.h"
#include "propedit/propedit_cli_parser.h"

propedit_cli_parser_c::propedit_cli_parser_c(const std::vector<std::string> &args)
  : cli_parser_c(args)
  , m_options(options_cptr(new options_c))
  , m_target(m_options->add_target(target_c::tt_segment_info))
{
}

void
propedit_cli_parser_c::set_parse_mode() {
  try {
    m_options->set_parse_mode(m_next_arg);
  } catch (...) {
    mxerror(boost::format(Y("Unknown parse mode in '%1% %2%'.")) % m_current_arg % m_next_arg);
  }
}

void
propedit_cli_parser_c::add_target() {
  try {
    m_target = m_options->add_target(m_next_arg);
  } catch (...) {
    mxerror(boost::format(Y("Invalid selector in '%1% %2%'.\n")) % m_current_arg % m_next_arg);
  }
}

void
propedit_cli_parser_c::add_change() {
  try {
    change_c::change_type_e type = (m_current_arg == "-a") || (m_current_arg == "--add") ? change_c::ct_add
                                 : (m_current_arg == "-s") || (m_current_arg == "--set") ? change_c::ct_set
                                 :                                                         change_c::ct_delete;
    m_target->add_change(type, m_next_arg);
  } catch (const char *message) {
    mxerror(boost::format(Y("Invalid change spec (%3%) in '%1% %2%'.\n")) % m_current_arg % m_next_arg % message);
  }
}

void
propedit_cli_parser_c::list_property_names() {
  mxinfo(Y("All known property names and their meaning:\n"));
  mxexit(0);
}

void
propedit_cli_parser_c::set_file_name() {
  m_options->set_file_name(m_current_arg);
}

#define OPT(spec, func, description) add_option(spec, boost::bind(&propedit_cli_parser_c::func, this), description)

void
propedit_cli_parser_c::init_parser() {
  add_information(YT("mkvpropedit [options] <file> [actions]"));

  add_section_header(YT("Options"));
  OPT("l|list-property-names", list_property_names, YT("List all valid property names and exit"));
  OPT("p|parse-mode=<mode>",   set_parse_mode,      YT("Sets the Matroska parser mode to 'fast' (default) or 'full'"));

  add_section_header(YT("Actions"));
  OPT("e|edit=<selector>",     add_target,          YT("Sets the Matroska file section that all following add/set/delete "
                                                      "actions operate on (see man page for syntax)"));
  OPT("a|add=<name=value>",    add_change,          YT("Adds a property with the value even if such a property already "
                                                      "exists"));
  OPT("s|set=<name=value>",    add_change,          YT("Sets a property to the value if it exists and add it otherwise"));
  OPT("d|delete=<name>",       add_change,          YT("Delete all occurences of a property"));

  add_section_header(YT("Other options"));
  add_common_options();

  add_separator();
  add_information(YT("The order of the various options is not important."));

  set_default_callback(boost::bind(&propedit_cli_parser_c::set_file_name, this));
}

#undef OPT

options_cptr
propedit_cli_parser_c::run() {
  init_parser();

  parse_args();

  m_options->validate();
  m_options->m_show_progress = 1 < verbose;

  return m_options;
}
