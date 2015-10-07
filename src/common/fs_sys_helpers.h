/*
   mkvtoolnix - A set of programs for manipulating Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Cross platform helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_FS_SYS_HELPERS_H
#define MTX_COMMON_FS_SYS_HELPERS_H

#include "common/common_pch.h"

int64_t get_current_time_millis();

#if defined(SYS_WINDOWS)

bool get_registry_key_value(const std::string &key, const std::string &value_name, std::string &value);

void set_environment_variable(const std::string &key, const std::string &value);
std::string get_environment_variable(const std::string &key);

#define WINDOWS_VERSION_UNKNOWN      0x00000000
#define WINDOWS_VERSION_2000         0x00050000
#define WINDOWS_VERSION_XP           0x00050001
#define WINDOWS_VERSION_SERVER2003   0x00050002
#define WINDOWS_VERSION_VISTA        0x00060000
#define WINDOWS_VERSION_SERVER2008   0x00060000
#define WINDOWS_VERSION_SERVER2008R2 0x00060001
#define WINDOWS_VERSION_7            0x00060001

unsigned int get_windows_version();

#endif

namespace mtx {

int system(std::string const &command);
void determine_path_to_current_executable(std::string const &argv0);
bfs::path get_application_data_folder();
bfs::path const &get_installation_path();

}

#endif
