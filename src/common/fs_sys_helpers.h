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

#include "common/os.h"

#include "common/mm_io.h"

namespace mtx {
  namespace mm_io {
    class create_directory_x: public exception {
    protected:
      std::string m_path, m_error;
      int m_error_number;
    public:
      create_directory_x(const std::string &path, const std::string &error, int error_number)
        : m_path(path)
        , m_error(error)
        , m_error_number(error_number)
      {
      }
      virtual ~create_directory_x() throw() { }

      virtual const char *what() const throw() {
        return "create_directory() failed";
      }
      virtual std::string error() const throw() {
        return (boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % m_path % m_error_number % m_error).str();
      }
    };
  }
}

int fs_entry_exists(const char *path);
void create_directory(const char *path);
int64_t get_current_time_millis();
std::string get_application_data_folder();
std::string get_installation_path();

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

#endif
