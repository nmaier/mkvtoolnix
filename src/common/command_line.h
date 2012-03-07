/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for command line helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_COMMAND_LINE_H
#define __MTX_COMMON_COMMAND_LINE_H

#include "common/common_pch.h"

extern std::string usage_text, version_info;

void usage(int exit_code = 0);
bool handle_common_cli_args(std::vector<std::string> &args, const std::string &redirect_output_short);

#endif  // __MTX_COMMON_COMMAND_LINE_H
