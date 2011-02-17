/*
   mkvtoolnix - A set of programs for manipulating Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Cross platform helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_FS_SYS_HELPERS_H
#define __MTX_COMMON_FS_SYS_HELPERS_H

#include "os.h"

#include <string>

int MTX_DLL_API fs_entry_exists(const char *path);
void MTX_DLL_API create_directory(const char *path);
int64_t MTX_DLL_API get_current_time_millis();
std::string MTX_DLL_API get_application_data_folder();
std::string MTX_DLL_API get_installation_path();

#if defined(SYS_WINDOWS)

bool MTX_DLL_API get_registry_key_value(const std::string &key, const std::string &value_name, std::string &value);

void MTX_DLL_API set_environment_variable(const std::string &key, const std::string &value);
std::string MTX_DLL_API get_environment_variable(const std::string &key);

#define WINDOWS_VERSION_UNKNOWN      0x00000000
#define WINDOWS_VERSION_2000         0x00050000
#define WINDOWS_VERSION_XP           0x00050001
#define WINDOWS_VERSION_SERVER2003   0x00050002
#define WINDOWS_VERSION_VISTA        0x00060000
#define WINDOWS_VERSION_SERVER2008   0x00060000
#define WINDOWS_VERSION_SERVER2008R2 0x00060001
#define WINDOWS_VERSION_7            0x00060001

unsigned int MTX_DLL_API get_windows_version();

#endif

#endif
