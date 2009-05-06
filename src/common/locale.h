/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_LOCALE_H
#define __MTX_COMMON_LOCALE_H

#include "common/os.h"

#include <string>
#include <vector>

extern int MTX_DLL_API cc_local_utf8;

std::string MTX_DLL_API get_local_charset();
std::string MTX_DLL_API get_local_console_charset();

int MTX_DLL_API utf8_init(const std::string &charset);
void MTX_DLL_API utf8_done();

std::string MTX_DLL_API to_utf8(int handle, const std::string &local);
std::string MTX_DLL_API from_utf8(int handle, const std::string &utf8);

std::vector<std::string> MTX_DLL_API command_line_utf8(int argc, char **argv);

#endif  // __MTX_COMMON_LOCALE_H
