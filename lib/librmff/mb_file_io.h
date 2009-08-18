/*
 *  mb_file_io.h
 *
 *  Copyright (C) Moritz Bunkus - March 2004
 *
 *  mb_file_io is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  mb_file_io is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __MB_FILE_IO_H
#define __MB_FILE_IO_H

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif

#define MB_OPEN_MODE_READING 0
#define MB_OPEN_MODE_WRITING 1

typedef void *(*mb_file_open_t)(const char *path, int mode);
typedef int (*mb_file_close_t)(void *file);
typedef int64_t (*mb_file_read_t)(void *file, void *buffer, int64_t bytes);
typedef int64_t (*mb_file_write_t)(void *file, const void *buffer,
                                   int64_t bytes);
typedef int64_t (*mb_file_tell_t)(void *file);
typedef int64_t (*mb_file_seek_t)(void *file, int64_t offset, int whence);

typedef struct {
  mb_file_open_t open;
  mb_file_close_t close;
  mb_file_read_t read;
  mb_file_write_t write;
  mb_file_tell_t tell;
  mb_file_seek_t seek;
} mb_file_io_t;

extern mb_file_io_t std_mb_file_io;

#endif /* __MB_FILE_IO */
