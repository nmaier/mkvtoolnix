/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mm_io_callback.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mm_io.cpp,v 1.1 2003/05/22 17:54:43 mosu Exp $
    \brief IO callback class implementation
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mm_io_callback.h"

mm_io_callback::mm_io_callback(const char *path, const open_mode mode) {
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

  file = fopen(path, cmode);

  if (file == NULL) {
    fprintf(stderr, "Error: Could not open the file: %d (%s)\n", errno,
            strerror(errno));
    exit(1);
  }
}

mm_io_callback::~mm_io_callback() {
  close();
}

uint64 mm_io_callback::getFilePointer() {
  return ftello(file);
}

void mm_io_callback::setFilePointer(int64 offset, seek_mode mode) {
  int whence;

  if (mode == seek_beginning)
    whence = SEEK_SET;
  else if (mode == seek_end)
    whence = SEEK_END;
  else
    whence = SEEK_CUR;

  fseeko(file, offset, whence);
}

size_t mm_io_callback::write(const void *buffer, size_t size) {
  size_t bwritten;

  bwritten = fwrite(buffer, 1, size, file);
  if (ferror(file) != 0) {
    fprintf(stderr, "Error writing to the output file: %d (%s)\n", errno,
            strerror(errno));
    exit(1);
  }

  return bwritten;
}

uint32 mm_io_callback::read(void *buffer, size_t size) {
  return fread(buffer, 1, size, file);
}

void mm_io_callback::close() {
  fclose(file);
}

/*
 * Dummy class for output to /dev/null. Needed for two pass stuff.
 */

mm_devnull_io_callback::mm_devnull_io_callback() {
  pos = 0;
}

uint64 mm_devnull_io_callback::getFilePointer() {
  return pos;
}

void mm_devnull_io_callback::setFilePointer(int64 offset, seek_mode mode) {
  if (mode == seek_beginning)
    pos = offset;
  else if (mode == seek_end)
    pos = 0;
  else
    pos += offset;
}

uint32 mm_devnull_io_callback::read(void *buffer, size_t size) {
  memset(buffer, 0, size);
  pos += size;

  return size;
}

size_t mm_devnull_io_callback::write(const void *buffer, size_t size) {
  pos += size;

  return size;
}

void mm_devnull_io_callback::close() {
}
