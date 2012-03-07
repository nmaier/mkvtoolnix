/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_DEBUGGING_H
#define __MTX_COMMON_DEBUGGING_H

#include "common/os.h"

#include <string>

bool debugging_requested(const char *option, std::string *arg = nullptr);
bool debugging_requested(const std::string &option, std::string *arg = nullptr);
void request_debugging(const std::string &options);
void clear_debugging_requests();
void init_debugging();

int parse_debug_interval_arg(const std::string &option, int default_value = 1000, int invalid_value = -1);

#endif  // __MTX_COMMON_DEBUGGING_H
