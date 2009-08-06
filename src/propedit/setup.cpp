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

#include "common/command_line.h"
#include "common/common.h"
#include "common/ebml.h"
#include "common/translation.h"
#include "common/unique_numbers.h"
#include "common/xml/element_mapping.h"
#include "propedit/setup.h"

#ifdef SYS_WINDOWS
# include "common/os_windows.h"
#endif

using namespace libmatroska;

propedit_cli_parser_c::propedit_cli_parser_c(const std::vector<std::string> &args)
  : cli_parser_c(args)
  , m_options(options_cptr(new options_c))
  , m_target(m_options->add_target(target_c::tt_segment_info))
{
}

propedit_cli_parser_c::~propedit_cli_parser_c() {
}

void
propedit_cli_parser_c::set_usage() {
  usage_text  =   "";
  usage_text += Y("mkvpropedit [options] <file> [actions]\n");
  usage_text +=   "\n";
  usage_text += Y(" Options:\n");
  usage_text += Y("  -v, --verbose            Show verbose progress reports\n");
  usage_text += Y("  -p, --parse-mode <mode>  Sets the Matroska parser mode to 'fast'\n");
  usage_text += Y("                           (default) or 'full'\n");
  usage_text +=   "\n";
  usage_text += Y(" Actions:\n");
  usage_text += Y("  -e, --edit <selector>    Sets the Matroska file section that all following\n");
  usage_text += Y("                           add/set/delete actions operate on (see man page)\n");
  usage_text += Y("  -a, --add <name=value>   Add a property with a value even if such a\n");
  usage_text += Y("                           property already exists\n");
  usage_text += Y("  -s, --set <name=value>   Set a property to a value if it exists and\n");
  usage_text += Y("                           add it otherwise\n");
  usage_text += Y("  -d, --delete <name>      Delete all occurences of a property\n");
  usage_text += Y(" Other options:\n");
  usage_text += Y("  -l, --list-property-names\n");
  usage_text += Y("                           Lists all valid property names\n");
  usage_text += Y("  --ui-language <code>     Force the translations for 'code' to be used.\n");
  usage_text += Y("  --command-line-charset <charset>\n"
                  "                           Charset for strings on the command line\n");
  usage_text += Y("  --output-charset <cset>  Output messages in this charset\n");
  usage_text += Y("  -r, --redirect-output <file>\n"
                  "                           Redirects all messages into this file.\n");
  usage_text += Y("  @optionsfile             Reads additional command line options from\n"
                  "                           the specified file (see man page).\n");
  usage_text += Y("  -h, --help               Show this help.\n");
  usage_text += Y("  -V, --version            Show version information.\n");
  usage_text +=   "\n\n";
  usage_text += Y("The order of the various options is not important.\n");

  version_info = "mkvpropedit v" VERSION " ('" VERSIONNAME "')";
}

void
propedit_cli_parser_c::add_option(const std::string &spec,
                                  void (propedit_cli_parser_c::*callback)(),
                                  const std::string &description) {
  cli_parser_c::add_option(spec, boost::bind(callback, this), description);
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

options_cptr
propedit_cli_parser_c::run() {
  add_option("e|edit=s",               &propedit_cli_parser_c::add_target,          "");
  add_option("a|add|s|set|d|delete=s", &propedit_cli_parser_c::add_change,          "");
  add_option("p|parse-mode=s",         &propedit_cli_parser_c::set_parse_mode,      "");
  add_option("l|list-propety-names",   &propedit_cli_parser_c::list_property_names, "");
  set_default_callback(boost::bind(&propedit_cli_parser_c::set_file_name, this));

  parse_args();

  m_options->validate();
  m_options->m_show_progress = 1 < verbose;

  return m_options;
}

/** \brief Initialize global variables
*/
static void
init_globals() {
  clear_list_of_unique_uint32(UNIQUE_ALL_IDS);
}

/** \brief Global program initialization

   Both platform dependant and independant initialization is done here.
   The locale's \c LC_MESSAGES is set.
*/
void
setup() {
  atexit(mtx_common_cleanup);

  init_globals();
  init_stdio();
  init_locales();

  mm_file_io_c::setup();
  g_cc_local_utf8 = charset_converter_c::init("");
  init_cc_stdio();

  xml_element_map_init();
}

