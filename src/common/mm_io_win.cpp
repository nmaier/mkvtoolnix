/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class implementation (Windows specific parts)

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(SYS_WINDOWS)

#include <direct.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/endian.h"
#include "common/error.h"
#include "common/fs_sys_helpers.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/strings/utf8.h"

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile);

mm_file_io_c::mm_file_io_c(const std::string &path,
                           const open_mode mode)
  : m_file_name(path)
  , m_file(NULL)
  , m_eof(false)
{
  DWORD access_mode, share_mode, disposition;

  switch (mode) {
    case MODE_READ:
      access_mode = GENERIC_READ;
      share_mode  = FILE_SHARE_READ | FILE_SHARE_WRITE;
      disposition = OPEN_EXISTING;
      break;
    case MODE_WRITE:
      access_mode = GENERIC_WRITE | GENERIC_READ;
      share_mode  = FILE_SHARE_READ;
      disposition = OPEN_EXISTING;
      break;
    case MODE_SAFE:
      access_mode = GENERIC_WRITE | GENERIC_READ;
      share_mode  = FILE_SHARE_READ;
      disposition = OPEN_ALWAYS;
      break;
    case MODE_CREATE:
      access_mode = GENERIC_WRITE;
      share_mode  = FILE_SHARE_READ;
      disposition = CREATE_ALWAYS;
      break;
    default:
      throw mm_io_error_c(Y("Unknown open mode"));
  }

  if ((MODE_WRITE == mode) || (MODE_CREATE == mode))
    prepare_path(path);

  m_file = (void *)CreateFileUtf8(path.c_str(), access_mode, share_mode, NULL, disposition, 0, NULL);
  if ((HANDLE)m_file == (HANDLE)0xFFFFFFFF)
    throw mm_io_open_error_c();

  m_dos_style_newlines = true;
}

void
mm_file_io_c::close() {
  if (NULL != m_file) {
    CloseHandle((HANDLE)m_file);
    m_file = NULL;
  }
  m_file_name.clear();
}

uint64
mm_file_io_c::get_real_file_pointer() {
  LONG high = 0;
  DWORD low = SetFilePointer((HANDLE)m_file, 0, &high, FILE_CURRENT);

  if ((low == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    return (uint64)-1;

  return (((uint64)high) << 32) | (uint64)low;
}

void
mm_file_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  DWORD method = seek_beginning == mode ? FILE_BEGIN
               : seek_current   == mode ? FILE_CURRENT
               : seek_end       == mode ? FILE_END
               :                          FILE_BEGIN;
  LONG high    = (LONG)(offset >> 32);
  DWORD low    = SetFilePointer((HANDLE)m_file, (LONG)(offset & 0xffffffff), &high, method);

  if ((INVALID_SET_FILE_POINTER == low) && (GetLastError() != NO_ERROR))
    throw mm_io_seek_error_c();

  m_current_position = (int64_t)low + ((int64_t)high << 32);
}

uint32
mm_file_io_c::_read(void *buffer,
                    size_t size) {
  DWORD bytes_read;

  if (!ReadFile((HANDLE)m_file, buffer, size, &bytes_read, NULL)) {
    m_eof              = true;
    m_current_position = get_real_file_pointer();

    return 0;
  }

  if (size != bytes_read)
    m_eof = true;

  m_current_position += bytes_read;

  return bytes_read;
}

size_t
mm_file_io_c::_write(const void *buffer,
                     size_t size) {
  DWORD bytes_written;

  if (!WriteFile((HANDLE)m_file, buffer, size, &bytes_written, NULL))
    bytes_written = 0;

  if (bytes_written != size) {
    std::string error_msg_utf8;

    DWORD error     = GetLastError();
    char *error_msg = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&error_msg, 0, NULL);

    if (NULL != error_msg) {
      int idx = strlen(error_msg) - 1;

      while ((0 <= idx) && ((error_msg[idx] == '\n') || (error_msg[idx] == '\r'))) {
        error_msg[idx] = 0;
        idx--;
      }

      error_msg_utf8 = g_cc_local_utf8->utf8(error_msg);
    } else
      error_msg_utf8 = Y("unknown");

    mxerror(boost::format(Y("Could not write to the output file: %1% (%2%)\n")) % error % error_msg_utf8);

    if (NULL != error_msg)
      LocalFree(error_msg);
  }

  m_current_position += bytes_written;
  m_cached_size       = -1;

  return bytes_written;
}

bool
mm_file_io_c::eof() {
  return m_eof;
}

int
mm_file_io_c::truncate(int64_t pos) {
  m_cached_size = -1;

  save_pos();
  if (setFilePointer2(pos)) {
    bool result = SetEndOfFile((HANDLE)m_file);
    restore_pos();

    return result ? 0 : -1;
  }

  restore_pos();

  return -1;
}

void
mm_file_io_c::setup() {
}

size_t
mm_stdio_c::_write(const void *buffer,
                   size_t size) {
  HANDLE h_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (INVALID_HANDLE_VALUE == h_stdout)
    return 0;

  DWORD file_type = GetFileType(h_stdout);
  bool is_console = false;
  if ((FILE_TYPE_UNKNOWN != file_type) && ((file_type & ~FILE_TYPE_REMOTE) == FILE_TYPE_CHAR)) {
    DWORD dummy;
    is_console = GetConsoleMode(h_stdout, &dummy);
  }

  if (is_console) {
    const std::wstring &w = to_wide(g_cc_stdio->utf8(std::string(static_cast<const char *>(buffer), size)));
    DWORD bytes_written   = 0;

    WriteConsoleW(h_stdout, w.c_str(), w.length(), &bytes_written, NULL);

    return bytes_written;
  }

  size_t bytes_written = fwrite(buffer, 1, size, stdout);
  fflush(stdout);

  m_cached_size = -1;

  return bytes_written;
}

#endif  // defined(SYS_WINDOWS)
