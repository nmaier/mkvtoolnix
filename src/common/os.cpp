/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   OS dependant helper functions
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

using namespace std;

#if defined(SYS_WINDOWS)

/*
   Utf8ToUtf16 and CreateFileUtf8

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Mike Matsnev <mike@po.cs.msu.su>
   Modifications by Moritz Bunkus <moritz@bunkus.org>
   Additional code by Alexander Noé <alexander.noe@s2001.tu-chemnitz.de>
*/

# include "os_windows.h"

# include <io.h>
# include <windows.h>
# include <winreg.h>
# include <direct.h>
# include <sys/timeb.h>

static unsigned
Utf8ToUtf16(const char *utf8,
            int utf8len,
            wchar_t *utf16,
            unsigned utf16len) {
  const unsigned char *u = (const unsigned char *)utf8;
  const unsigned char *t = u + (utf8len < 0 ? strlen(utf8)+1 : utf8len);
  wchar_t *d = utf16, *w = utf16 + utf16len, c0, c1;
  unsigned ch;

  if (utf16len == 0) {
    d = utf16 = NULL;
    w = d - 1;
  }

  while (u<t && d<w) {
    if (!(*u & 0x80))
      ch = *u++;
    else if ((*u & 0xe0) == 0xc0) {
      ch = (unsigned)(*u++ & 0x1f) << 6;
      if (u<t && (*u & 0xc0) == 0x80)
        ch |= *u++ & 0x3f;
    } else if ((*u & 0xf0) == 0xe0) {
      ch = (unsigned)(*u++ & 0x0f) << 12;
      if (u<t && (*u & 0xc0) == 0x80) {
        ch |= (unsigned)(*u++ & 0x3f) << 6;
        if (u<t && (*u & 0xc0) == 0x80)
          ch |= *u++ & 0x3f;
      }
    } else if ((*u & 0xf8) == 0xf0) {
      ch = (unsigned)(*u++ & 0x07) << 18;
      if (u<t && (*u & 0xc0) == 0x80) {
        ch |= (unsigned)(*u++ & 0x3f) << 12;
        if (u<t && (*u & 0xc0) == 0x80) {
          ch |= (unsigned)(*u++ & 0x3f) << 6;
          if (u<t && (*u & 0xc0) == 0x80)
            ch |= *u++ & 0x3f;
        }
      }
    } else
      continue;

    c0 = c1 = 0x0000;

    if (ch < 0xd800)
      c0 = (wchar_t)ch;
    else if (ch < 0xe000) // invalid
      c0 = 0x0020;
    else if (ch < 0xffff)
      c0 = (wchar_t)ch;
    else if (ch < 0x110000) {
      c0 = 0xd800 | (ch>>10);
      c1 = 0xdc00 | (ch & 0x03ff);
    } else
      c0 = 0x0020;

    if (utf16) {
      if (c1) {
        if (d+1<w) {
          *d++ = c0;
          *d++ = c1;
        } else
          break;
      } else
        *d++ = c0;
    } else {
      d++;
      if (c1)
        d++;
    }
  }

  if (utf16 && d<w)
    *d = 0x0000;

  if (utf8len < 0 && utf16len > 0)
    utf16[utf16len - 1] = L'\0';

  return (unsigned)(d - utf16);
}

static bool
win32_is_unicode_possible() {
  OSVERSIONINFOEX ovi;

  ovi.dwOSVersionInfoSize = sizeof(ovi);
  GetVersionEx((OSVERSIONINFO *)&ovi);
  return ovi.dwPlatformId == VER_PLATFORM_WIN32_NT;
}

static char *
win32_wide_to_multi(const wchar_t *wbuffer) {
  int reqbuf = WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, NULL, 0, NULL,
                                   NULL);
  char *buffer = new char[reqbuf];
  WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, buffer, reqbuf, NULL, NULL);

  return buffer;
}

static wchar_t *
win32_utf8_to_utf16(const char *s) {
  int wreqbuf = Utf8ToUtf16(s, -1, NULL, 0);
  wchar_t *wbuffer = new wchar_t[wreqbuf];

  Utf8ToUtf16(s, -1, wbuffer, wreqbuf);

  return wbuffer;
}

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile) {
  HANDLE ret;
  // convert the name to wide chars
  wchar_t *wbuffer = win32_utf8_to_utf16(lpFileName);

  if (win32_is_unicode_possible())
    ret = CreateFileW(wbuffer, dwDesiredAccess, dwShareMode,
                      lpSecurityAttributes, dwCreationDisposition,
                      dwFlagsAndAttributes, hTemplateFile);
  else {
    char *buffer = win32_wide_to_multi(wbuffer);
    ret = CreateFileA(buffer, dwDesiredAccess, dwShareMode,
                      lpSecurityAttributes, dwCreationDisposition,
                      dwFlagsAndAttributes, hTemplateFile);

    delete []buffer;
  }

  delete []wbuffer;

  return ret;
}

void
create_directory(const char *path) {
  wchar_t *wbuffer = win32_utf8_to_utf16(path);
  int result;

  if (win32_is_unicode_possible())
    result = _wmkdir(wbuffer);

  else {
    char *buffer = win32_wide_to_multi(wbuffer);
    result = _mkdir(buffer);

    delete []buffer;
  }

  delete []wbuffer;

  if (0 != result)
    throw error_c(boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % path % errno % strerror(errno));
}

int
fs_entry_exists(const char *path) {
  wchar_t *wbuffer = win32_utf8_to_utf16(path);
  struct _stat s;
  int result;

  if (win32_is_unicode_possible())
    result = _wstat(wbuffer, &s);

  else {
    char *buffer = win32_wide_to_multi(wbuffer);

    result = _stat(buffer, &s);
    delete []buffer;
  }

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
  string local_path = from_utf8(cc_local_utf8, path);
  if (0 != mkdir(local_path.c_str(), 0777))
    throw error_c(boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % path % errno % strerror(errno));
}

int
fs_entry_exists(const char *path) {
  string local_path = from_utf8(cc_local_utf8, path);
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
