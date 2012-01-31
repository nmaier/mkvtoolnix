/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   debugging functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/strings/editing.h"

static std::string s_debug_options;

static std::map<std::string, std::string> s_debugging_options;

bool
debugging_requested(const char *option,
                    std::string *arg) {
  std::map<std::string, std::string>::iterator option_ptr = s_debugging_options.find(std::string(option));
  if (s_debugging_options.end() == option_ptr)
    return false;

  if (NULL != arg)
    *arg = option_ptr->second;

  return true;
}

bool
debugging_requested(const std::string &option,
                    std::string *arg) {
  return debugging_requested(option.c_str(), arg);
}

void
request_debugging(const std::string &options) {
  std::vector<std::string> all_options = split(options);

  for (auto &one_option : all_options) {
    std::vector<std::string> parts = split(one_option, "=", 2);
    if (1 == parts.size())
      s_debugging_options[parts[0]] = "";
    else
      s_debugging_options[parts[0]] = parts[1];
  }
}

void
clear_debugging_requests() {
  s_debugging_options.clear();
}

void
init_debugging() {
  const char *value = getenv("MKVTOOLNIX_DEBUG");
  if (NULL != value)
    request_debugging(value);
  else {
    value = getenv("MTX_DEBUG");
    if (NULL != value)
      request_debugging(value);
  }
}
