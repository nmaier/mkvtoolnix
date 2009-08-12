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
#include <stdexcept>
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
  } catch (std::runtime_error &error) {
    mxerror(boost::format(Y("Invalid change spec (%3%) in '%1% %2%'.\n")) % m_current_arg % m_next_arg % error.what());
  }
}

void
propedit_cli_parser_c::list_property_names() {
  mxinfo(Y("All known property names and their meaning:\n"));

  list_property_names_for_table(property_element_c::get_table_for(KaxInfo::ClassInfos,   NULL, true), Y("Segment information"), "info");
  list_property_names_for_table(property_element_c::get_table_for(KaxTracks::ClassInfos, NULL, true), Y("Track header"),        "track:...");

  mxexit(0);
}

void
propedit_cli_parser_c::list_property_names_for_table(const std::vector<property_element_c> &table,
                                                     const std::string &title,
                                                     const std::string &edit_spec) {
  size_t max_name_len = 0;
  std::vector<property_element_c>::const_iterator table_it;
  mxforeach(table_it, table)
    max_name_len = std::max(max_name_len, table_it->m_name.length());

  static boost::regex s_newline_re("\\s*\\n\\s*", boost::regex::perl);
  boost::format format((boost::format("    %%|1$-%1%s| | %%2%%: %%3%%\n") % max_name_len).str());

  mxinfo("\n");
  mxinfo(boost::format(Y("  Elements in the category '%1%' ('--edit %2%')\n")) % title % edit_spec);

  mxforeach(table_it, table)
    mxinfo(format % table_it->m_name % table_it->m_title.get_translated()
           % boost::regex_replace(table_it->m_description.get_translated(), s_newline_re, " ",  boost::match_default | boost::match_single_line));
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

  add_hook(cli_parser_c::ht_unknown_option, boost::bind(&propedit_cli_parser_c::set_file_name, this));
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

