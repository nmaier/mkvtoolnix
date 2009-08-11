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
#include <boost/regex.hpp>
#include <string>
#include <typeinfo>
#include <vector>

#include "common/command_line.h"
#include "common/common.h"
#include "common/ebml.h"
#include "propedit/setup.h"

#ifdef SYS_WINDOWS
# include "common/os_windows.h"
#endif

using namespace libmatroska;

/** \brief Outputs usage information
*/
static void
set_usage() {
  usage_text  =   "";
  usage_text += Y("mkvpropedit <file> [options]\n");
  usage_text +=   "\n";
  usage_text += Y(" Global options:\n");
  usage_text += Y("  -v, --verbose            verbose status\n");
  usage_text +=   "\n";
}

options_c
parse_args(std::vector<std::string> args) {
  set_usage();
  while (handle_common_cli_args(args, ""))
    set_usage();

  options_c options;

  return options;
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

