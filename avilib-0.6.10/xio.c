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

#include "os.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>

int
xio_open(const char *pathname, int flags, ...)
{
	int mode=0;

	if(flags & O_CREAT) {
 		va_list arg;
	       	va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	return open(pathname, flags, mode);
}

ssize_t
xio_read(int fd, void *buf, size_t count)
{
  return read(fd, buf, count);
}

ssize_t
xio_write(int fd, const void *buf, size_t count)
{
  return write(fd, buf, count);
}

int
xio_ftruncate(int fd, off_t length)
{
#if defined(SYS_WINDOWS)
  _chsize(fd, length);
#else
  return ftruncate(fd, length);
#endif
}

off_t
xio_lseek(int fd, off_t offset, int whence)
{
  return lseek(fd, offset, whence);
}

int
xio_close(int fd)
{
  return close(fd);
}

int
xio_stat(const char *file_name, struct stat *buf)
{
	return stat(file_name, buf);
}

int
xio_lstat(const char *file_name, struct stat *buf)
{
#if defined(SYS_WINDOWS)
  fprintf(stderr, "xio_lstat not implemented on Windows.\n");
  exit(1);
  return -1;
#else
  return lstat(file_name, buf);
#endif
}

int
xio_rename(const char *oldpath, const char *newpath)
{
	return rename(oldpath, newpath);
}

int
xio_fstat(int fd, struct stat *buf)
{
  return fstat(fd, buf);
}
