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
#include "common/mm_io.h"
#include "common/translation.h"
#include "common/unique_numbers.h"
#include "common/xml/element_mapping.h"
#include "propedit/setup.h"

/** \brief Initialize global variables
*/
static void
init_globals() {
  clear_list_of_unique_uint32(UNIQUE_ALL_IDS);
  version_info = "mkvpropedit v" VERSION " ('" VERSIONNAME "')";
}

/** \brief Global program initialization

   Both platform dependant and independant initialization is done here.
   The locale's \c LC_MESSAGES is set.
*/
void
setup() {
  mtx_common_init();

  init_globals();
}

