/*
   vsscanf for Win32
  
   Written 5/2003 by <mgix@mgix.com>
  
   This code is in the Public Domain
  
   Read http://www.flipcode.org/cgi-bin/fcarticles.cgi?show=4&id=64176 for
   an explanation.
  
*/

#include "config.h"

#if !defined(HAVE_VSSCANF) || (HAVE_VSSCANF != 1)

#include "os.h"

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

int __declspec(naked) vsscanf_impl(const char *,const char *,va_list,int,void *) {
  __asm {
    push    ebx
    mov	    ebx,esp
    mov	    ecx,[ebx+16]
    mov	    edx,[ebx+20]
    lea	    edx,[ecx+edx*4-4]
    jmp	    l3
l2:
    push    dword ptr [edx]
    sub	    edx,4
l3:
    cmp	    edx,ecx
    jae	    l2
    push    dword ptr [ebx+12]
    push    dword ptr [ebx+8]
    call    dword ptr [ebx+24]
    mov	    esp,ebx
    pop	    ebx
    ret
  };
}

int vsscanf(const char *str, const char *format, va_list ap) {
  const char  *p = format;
  int	      narg = 0;

  while (*p)
    if (*p++ == '%') {
      if (*p != '*' && *p != '%')
	++narg;
      ++p;
    }

  return vsscanf_impl(str,format,ap,narg,sscanf);
}

#endif
