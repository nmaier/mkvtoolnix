/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   OS dependant helper functions
*/

#include "common.h"
#include "config.h"
#include "os.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

#include <windows.h>

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

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile) {
  OSVERSIONINFOEX ovi;
  bool unicode_possible;
  HANDLE ret;
  // convert the name to wide chars
  int wreqbuf = Utf8ToUtf16(lpFileName, -1, NULL, 0);
  wchar_t *wbuffer = new wchar_t[wreqbuf];
  Utf8ToUtf16(lpFileName, -1, wbuffer, wreqbuf);

  ovi.dwOSVersionInfoSize = sizeof(ovi);
  GetVersionEx((OSVERSIONINFO *)&ovi);
  unicode_possible = ovi.dwPlatformId == VER_PLATFORM_WIN32_NT;

  if (unicode_possible)
    ret = CreateFileW(wbuffer, dwDesiredAccess, dwShareMode,
                      lpSecurityAttributes, dwCreationDisposition,
                      dwFlagsAndAttributes, hTemplateFile);
  else {
    int reqbuf = WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, NULL, 0, NULL,
                                     NULL);
    char *buffer = new char[reqbuf];
    WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, buffer, reqbuf, NULL, NULL);
    ret = CreateFileA(buffer, dwDesiredAccess, dwShareMode,
                      lpSecurityAttributes, dwCreationDisposition,
                      dwFlagsAndAttributes, hTemplateFile);

    delete []buffer;
  }

  delete []wbuffer;

  return ret;
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
