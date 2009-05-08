/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   OS dependant helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>

   CreateFileUtf8
     Written by Mike Matsnev <mike@po.cs.msu.su>
     Modifications by Moritz Bunkus <moritz@bunkus.org>
     Additional code by Alexander Noé <alexander.noe@s2001.tu-chemnitz.de>
*/

#include "common/os.h"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "common/common.h"
#include "common/locale.h"
#include "common/mm_io.h"
#ifdef SYS_WINDOWS
# include "common/string_editing.h"
#endif

#if defined(SYS_WINDOWS)

# include "os_windows.h"

# include <io.h>
# include <windows.h>
# include <winreg.h>
# include <direct.h>
# include <sys/timeb.h>

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile) {
  // convert the name to wide chars
  wchar_t *wbuffer = win32_utf8_to_utf16(lpFileName);
  HANDLE ret       = CreateFileW(wbuffer, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
  delete []wbuffer;

  return ret;
}

void
create_directory(const char *path) {
  wchar_t *wbuffer = win32_utf8_to_utf16(path);
  int result       = _wmkdir(wbuffer);
  delete []wbuffer;

  if (0 != result)
    throw error_c(boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % path % errno % strerror(errno));
}

int
fs_entry_exists(const char *path) {
  struct _stat s;

  wchar_t *wbuffer = win32_utf8_to_utf16(path);
  int result       = _wstat(wbuffer, &s);
  delete []wbuffer;

  return 0 == result;
}

int64_t
get_current_time_millis() {
  struct _timeb tb;
  _ftime(&tb);

  return (int64_t)tb.time * 1000 + tb.millitm;
}

bool
get_registry_key_value(const std::string &key,
                       const std::string &value_name,
                       std::string &value) {
  std::vector<std::string> key_parts = split(key, "\\", 2);
  HKEY hkey;
  HKEY hkey_base = key_parts[0] == "HKEY_CURRENT_USER" ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  DWORD error    = RegOpenKeyEx(hkey_base, key_parts[1].c_str(), 0, KEY_READ, &hkey);

  if (ERROR_SUCCESS != error)
    return false;

  bool ok        = false;
  DWORD data_len = 0;
  DWORD dwDisp;
  if (ERROR_SUCCESS == RegQueryValueEx(hkey, value_name.c_str(), NULL, &dwDisp, NULL, &data_len)) {
    char *data = new char[data_len + 1];
    memset(data, 0, data_len + 1);

    if (ERROR_SUCCESS == RegQueryValueEx(hkey, value_name.c_str(), NULL, &dwDisp, (BYTE *)data, &data_len)) {
      value = data;
      ok    = true;
    }

    delete []data;
  }

  RegCloseKey(hkey);

  return ok;
}

std::string
get_installation_path() {
  std::string path;

  if (get_registry_key_value("HKEY_LOCAL_MACHINE\\Software\\mkvmergeGUI\\GUI", "installation_path", path) && !path.empty())
    return path;

  if (get_registry_key_value("HKEY_CURRENT_USER\\Software\\mkvmergeGUI\\GUI", "installation_path", path) && !path.empty())
    return path;

  return "";
}

void
set_environment_variable(const std::string &key,
                         const std::string &value) {
  SetEnvironmentVariable(key.c_str(), value.c_str());
  std::string env_buf = (boost::format("%1%=%2%") % key % value).str();
  _putenv(env_buf.c_str());
}

std::string
get_environment_variable(const std::string &key) {
  char buffer[100];
  memset(buffer, 0, 100);

  if (0 == GetEnvironmentVariable(key.c_str(), buffer, 99))
    return "";

  return buffer;
}

#else // SYS_WINDOWS

# include <sys/types.h>
# include <unistd.h>

void
create_directory(const char *path) {
  std::string local_path = from_utf8(cc_local_utf8, path);
  if (0 != mkdir(local_path.c_str(), 0777))
    throw error_c(boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % path % errno % strerror(errno));
}

int
fs_entry_exists(const char *path) {
  std::string local_path = from_utf8(cc_local_utf8, path);
  struct stat s;
  return 0 == stat(local_path.c_str(), &s);
}

int64_t
get_current_time_millis() {
  struct timeval tv;
  if (0 != gettimeofday(&tv, NULL))
    return -1;

  return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
}

#endif // SYS_WINDOWS

// -----------------------------------------------------------------

#if !defined(HAVE_VSSCANF) || (HAVE_VSSCANF != 1)
/*
   vsscanf for Win32

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Mike Matsnev <mike@po.cs.msu.su>
*/

int __declspec(naked)
vsscanf_impl(const char *,
             const char *,
             va_list,
             int,
             void *) {
  __asm {
    push    ebx
    mov     ebx,esp
    mov     ecx,[ebx+16]
    mov     edx,[ebx+20]
    lea     edx,[ecx+edx*4-4]
    jmp     l3
l2:
    push    dword ptr [edx]
    sub     edx,4
l3:
    cmp     edx,ecx
    jae     l2
    push    dword ptr [ebx+12]
    push    dword ptr [ebx+8]
    call    dword ptr [ebx+24]
    mov     esp,ebx
    pop     ebx
    ret
  };
}

int
vsscanf(const char *str,
        const char *format,
        va_list ap) {
  const char  *p = format;
  int narg = 0;

  while (*p)
    if (*p++ == '%') {
      if (*p != '*' && *p != '%')
        ++narg;
      ++p;
    }

  return vsscanf_impl(str,format,ap,narg,sscanf);
}

#endif // !defined(HAVE_VSSCANF) || (HAVE_VSSCANF != 1)
