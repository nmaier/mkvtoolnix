/*
 * vsscanf for Win32
 *
 * Written 5/2003 by <mgix@mgix.com>
 *
 * This code is in the Public Domain
 *
 * Read http://www.flipcode.org/cgi-bin/fcarticles.cgi?show=4&id=64176 for
 * an explanation.
 *
 */

#include "config.h"

#if !defined(HAVE_VSSCANF) || (HAVE_VSSCANF != 1)

#include "os.h"

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

int
vsscanf(const char *buffer,
        const char *format,
        va_list argPtr) {
  // Get an upper bound for the # of args
  size_t count = 0;
  const char *p = format;
  while (1) {
    char c = *(p++);
    if (c == 0)
      break;
    if ((c == '%') && ((p[0] != '*') && (p[0] != '%')))
      ++count;
  }

  // Make a local stack
  size_t stackSize = (2 + count) * sizeof(void *);
  void **newStack = (void **)alloca(stackSize);

  // Fill local stack the way sscanf likes it
  newStack[0] = (void*)buffer;
  newStack[1] = (void*)format;
  memcpy(newStack + 2, argPtr, count * sizeof(void *));

  // Warp into system sscanf with new stack
  int result;
  void *savedESP;
  _asm {
    mov     savedESP, esp;
    mov     esp, newStack;
    call    sscanf;
    mov     esp, savedESP;
    mov     result, eax;
  }

  return result;
}

#endif
