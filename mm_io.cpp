/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mm_io_c.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief IO callback class implementation
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <exception>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(SYS_WINDOWS)
#include <stdarg.h>
#include <w32api/windef.h>
#include <w32api/winbase.h>
#endif // SYS_WINDOWS

#include "mm_io.h"
#include "common.h"

using namespace std;

#if defined(SYS_UNIX)
mm_io_c::mm_io_c(const char *path, const open_mode mode) {
  char *cmode;

  switch (mode) {
    case MODE_READ:
      cmode = "rb";
      break;
    case MODE_WRITE:
      cmode = "wb";
      break;
    case MODE_CREATE:
      cmode = "wb+";
      break;
    default:
      throw 0;
  }

  file = (FILE *)fopen(path, cmode);

  if (file == NULL)
    throw exception();
}

mm_io_c::mm_io_c() {
  file = NULL;
}

mm_io_c::~mm_io_c() {
  close();
}

uint64 mm_io_c::getFilePointer() {
  return ftello((FILE *)file);
}

void mm_io_c::setFilePointer(int64 offset, seek_mode mode) {
  int whence;

  if (mode == seek_beginning)
    whence = SEEK_SET;
  else if (mode == seek_end)
    whence = SEEK_END;
  else
    whence = SEEK_CUR;

  if (fseeko((FILE *)file, offset, whence) != 0)
    throw exception();
}

size_t mm_io_c::write(const void *buffer, size_t size) {
  size_t bwritten;

  bwritten = fwrite(buffer, 1, size, (FILE *)file);
  if (ferror((FILE *)file) != 0) {
    fprintf(stderr, "Error writing to the output file: %d (%s)\n", errno,
            strerror(errno));
    exit(1);
  }

  return bwritten;
}

uint32 mm_io_c::read(void *buffer, size_t size) {
  return fread(buffer, 1, size, (FILE *)file);
}

void mm_io_c::close() {
  if (file != NULL)
    fclose((FILE *)file);
}

bool mm_io_c::eof() {
  return feof((FILE *)file) != 0 ? true : false;
}

char *mm_io_c::gets(char *buffer, size_t max_size) {
  return fgets(buffer, max_size, (FILE *)file);
}

string mm_io_c::getline() {
  char c;
  string s;

  while (!feof((FILE *)file)) {
    if (fread(&c, 1, 1, (FILE *)file) == 1) {
      if (c == '\r')
        continue;
      if (c == '\n')
        return s;
      s += c;
    }
  }

  return s;
}

#else // SYS_UNIX

mm_io_c::mm_io_c(const char *path, const open_mode mode) {
  DWORD access_mode, share_mode, disposition;

  switch (mode) {
    case MODE_READ:
      access_mode = GENERIC_READ;
      share_mode = FILE_SHARE_READ;
      disposition = OPEN_EXISTING;
      break;
    case MODE_WRITE:
      access_mode = GENERIC_WRITE;
      share_mode = 0;
      disposition = OPEN_ALWAYS;
      break;
    case MODE_CREATE:
      access_mode = GENERIC_WRITE;
      share_mode = 0;
      disposition = CREATE_ALWAYS;
      break;
    default:
      throw exception();
  }

  file = (void *)CreateFile(path, access_mode, share_mode, NULL, disposition,
                            0, NULL);
  if ((HANDLE)file == (HANDLE)0xFFFFFFFF)
    throw exception();
}

mm_io_c::mm_io_c() {
  file = NULL;
}

mm_io_c::~mm_io_c() {
  close();
}

void mm_io_c::close() {
  if (file != NULL)
    CloseHandle((HANDLE)file);
}

uint64 mm_io_c::getFilePointer() {
  LONG dummy = 0;

  return SetFilePointer((HANDLE)file, 0, &dummy, FILE_CURRENT);
}

void mm_io_c::setFilePointer(int64 offset, seek_mode mode) {
  DWORD method;
  LONG high;

  switch (mode) {
    case seek_beginning:
      method = FILE_BEGIN;
      break;
    case seek_current:
      method = FILE_CURRENT;
      break;
    case seek_end:
      method = FILE_END;
      break;
  }

  high = offset >> 32;
  SetFilePointer((HANDLE)file, offset & 0xffffffff, &high, method);
}

uint32 mm_io_c::read(void *buffer, size_t size) {
  DWORD bytes_read;

  if (!ReadFile((HANDLE)file, buffer, size, &bytes_read, NULL))
    return 0;

  return bytes_read;
}

size_t mm_io_c::write(const void *buffer,size_t size) {
  DWORD bytes_written;

  if (!WriteFile((HANDLE)file, buffer, size, &bytes_written, NULL))
    return 0;

  return bytes_written;
}

bool mm_io_c::eof() {
  return false;
}

char *mm_io_c::gets(char *buffer, size_t max_size) {
  // This will not be fast... But it shouldn't matter. gets is only
  // used by the text subtitle readers.
  DWORD bytes_read;
  int idx;

  idx = 0;
  do {
    if (!ReadFile((HANDLE)file, &buffer[idx], 1, &bytes_read, NULL)) {
      if (idx == 0)
        return NULL;
      else {
        if (idx <= max_size)
          buffer[idx] = 0;
        return buffer;
      }
    }
    if (buffer[idx] == '\n') {
      if ((idx + 1) < max_size)
        buffer[idx + 1] = 0;
      return buffer;
    }
    idx++;
  } while (idx < max_size);

  return buffer;
}

string mm_io_c::getline() {
  char c;
  string s;
  DWORD bytes_read;

  do {
    ReadFile((HANDLE)file, &c, 1, &bytes_read, NULL);
    if (bytes_read == 1) {
      if (c == '\r')
        continue;
      if (c == '\n')
        return s;
      s += c;
    }
  } while (bytes_read == 1);

  return s;
}

#endif

size_t mm_io_c::puts_unl(const char *s) {
  int i;
  size_t bytes_written;

  bytes_written = 0;
  for (i = 0; i < strlen(s); i++)
    if (s[i] != '\r')
      bytes_written += write(&s[i], 1);

  return bytes_written;
}

unsigned char mm_io_c::read_uint8() {
  unsigned char value;

  if (read(&value, 1) != 1)
    throw exception();

  return value;
}

uint16_t mm_io_c::read_uint16() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw exception();

  return get_uint16(buffer);
}

uint32_t mm_io_c::read_uint32() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw exception();

  return get_uint32(buffer);
}

uint64_t mm_io_c::read_uint64() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw exception();

  return get_uint64(buffer);
}

uint16_t mm_io_c::read_uint16_be() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw exception();

  return get_uint16_be(buffer);
}

uint32_t mm_io_c::read_uint32_be() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw exception();

  return get_uint32_be(buffer);
}

uint64_t mm_io_c::read_uint64_be() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw exception();

  return get_uint64_be(buffer);
}

void mm_io_c::skip(int64 num_bytes) {
  int64_t pos;

  pos = getFilePointer();
  setFilePointer(pos + num_bytes);
  if ((pos + num_bytes) != getFilePointer())
    throw exception();
}

/*
 * Dummy class for output to /dev/null. Needed for two pass stuff.
 */

mm_null_io_c::mm_null_io_c() {
  pos = 0;
}

uint64 mm_null_io_c::getFilePointer() {
  return pos;
}

void mm_null_io_c::setFilePointer(int64 offset, seek_mode mode) {
  if (mode == seek_beginning)
    pos = offset;
  else if (mode == seek_end)
    pos = 0;
  else
    pos += offset;
}

uint32 mm_null_io_c::read(void *buffer, size_t size) {
  memset(buffer, 0, size);
  pos += size;

  return size;
}

size_t mm_null_io_c::write(const void *buffer, size_t size) {
  pos += size;

  return size;
}

void mm_null_io_c::close() {
}
