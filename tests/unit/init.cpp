/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for unit tests

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "tests/unit/init.h"

#include "src/common/unique_numbers.h"

namespace {

void
mxmsg_handler(unsigned int level,
              std::string const &message) {
  if (MXMSG_WARNING == level)
    g_warning_issued = true;

  else if (MXMSG_ERROR == level)
    throw mtxut::mxerror_x{message};
}

}

void
mtxut::init_suite() {
  clear_list_of_unique_numbers(UNIQUE_ALL_IDS);
  mtx_common_init("UNITTESTS");

  set_mxmsg_handler(MXMSG_INFO,    mxmsg_handler);
  set_mxmsg_handler(MXMSG_WARNING, mxmsg_handler);
  set_mxmsg_handler(MXMSG_ERROR,   mxmsg_handler);
}

void
mtxut::init_case() {
  clear_list_of_unique_numbers(UNIQUE_ALL_IDS);
  g_warning_issued = false;
}
