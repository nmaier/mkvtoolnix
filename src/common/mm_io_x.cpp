/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Exceptios for the I/O callback class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(SYS_WINDOWS)
# include <windows.h>
#else
# include <cerrno>
#endif

#include "common/mm_io_x.h"

namespace mtx { namespace mm_io {

std::error_code
make_error_code() {
#ifdef SYS_WINDOWS
  return std::error_code(::GetLastError(), std::system_category());
#else
  return std::error_code(errno, std::generic_category());
#endif
}

std::string
exception::error()
  const throw() {
  return std::errc::no_such_file_or_directory == code() ? Y("The file or directory was not found")
       : std::errc::no_space_on_device        == code() ? Y("No space left to write to")
       : std::errc::permission_denied         == code() ? Y("No permission to read from, to write to or to create")
       :                                                  code().message();
}

}}
