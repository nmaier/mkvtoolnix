/*
 *  libxio.c
 *
 *  Copyright (C) Lukas Hejtmanek - January 2004
 *
 *  This file is part of transcode, a linux video stream processing tool
 *
 *  transcode is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  transcode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "common/common_pch.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "common/mm_io.h"

#include "xio.h"

#define MAX_INSTANCES 4000

static mm_io_c *instances[MAX_INSTANCES];
static bool instances_initialized = false;

int
xio_open(void *pathname,
         int,
         ...) {
  int i, idx;

  if (!instances_initialized) {
    memset(instances, 0, MAX_INSTANCES * sizeof(mm_io_c *));
    instances_initialized = true;
  }

  idx = -1;
  for (i = 0; i < MAX_INSTANCES; i++)
    if (instances[i] == NULL) {
      idx = i;
      break;
    }

  if (idx == -1)
    return -1;

  instances[idx] = static_cast<mm_io_c *>(pathname);

  return idx;
}

ssize_t
xio_read(int fd,
         void *buf,
         size_t count) {
  if ((fd < 0) || (fd >= MAX_INSTANCES) || (instances[fd] == NULL))
    return -1;
  return instances[fd]->read(buf, count);
}

ssize_t
xio_write(int fd,
          const void *buf,
          size_t count) {
  if ((fd < 0) || (fd >= MAX_INSTANCES) || (instances[fd] == NULL))
    return -1;
  return instances[fd]->write(buf, count);
}

int
xio_ftruncate(int fd,
              int64_t length) {
  if ((fd < 0) || (fd >= MAX_INSTANCES) || (instances[fd] == NULL))
    return -1;
  return instances[fd]->truncate(length);
}

int64_t
xio_lseek(int fd,
          int64_t offset,
          int whence) {
  uint64_t expected_pos;
  seek_mode smode;

  if ((fd < 0) || (fd >= MAX_INSTANCES) || (instances[fd] == NULL))
    return (int64_t)-1;
  if (whence == SEEK_SET) {
    smode = seek_beginning;
    expected_pos = offset;
  } else if (whence == SEEK_END) {
    smode = seek_end;
    expected_pos = instances[fd]->get_size() - offset;
  } else {
    smode = seek_current;
    expected_pos = instances[fd]->getFilePointer() + offset;
  }
  instances[fd]->setFilePointer(offset, smode);
  if (instances[fd]->getFilePointer() != expected_pos)
    return (int64_t)-1;
  return expected_pos;
}

int
xio_close(int fd) {
  if ((fd < 0) || (fd >= MAX_INSTANCES) || (instances[fd] == NULL))
    return -1;
  instances[fd] = NULL;
  return 0;
}

int
xio_stat(const char *,
         struct stat *) {
  // Not implemented for mkvtoolnix.
  return -1;
}

int
xio_lstat(const char *,
          struct stat *) {
  // Not implemented for mkvtoolnix.
  return -1;
}

int
xio_rename(const char *oldpath,
           const char *newpath) {
	return rename(oldpath, newpath);
}

int
xio_fstat(int,
          struct stat *) {
  // Not implemented for mkvtoolnix.
  return -1;
}
