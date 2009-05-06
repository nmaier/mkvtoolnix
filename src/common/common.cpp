/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/regex.hpp>
#include <stdlib.h>
#include <string>

#include "common/common.h"

// Global and static variables

int verbose = 1;

extern bool g_warning_issued;

static std::string s_debug_options;

// Functions

void
mxexit(int code) {
  if (code != -1)
    exit(code);

  if (g_warning_issued)
    exit(1);

  exit(0);
}

bool
debugging_requested(const char *option) {
  std::string expression = std::string("\\b") + option + "\\b";
  boost::regex re(expression, boost::regex::perl);

  return boost::regex_search(s_debug_options, re);
}

void
request_debugging(const std::string &options) {
  s_debug_options = options;
}
