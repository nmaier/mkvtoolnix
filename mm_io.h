/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mm_io_c.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mm_io.h,v 1.2 2003/05/23 06:34:57 mosu Exp $
    \brief IO callback class definitions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MM_IO_H
#define __MM_IO_H

#include <stdio.h>
#include <stdint.h>

#include "IOCallback.h"

using namespace LIBEBML_NAMESPACE;

class mm_io_c: public IOCallback {
protected:
  FILE *file;

public:
  mm_io_c(const char *path, const open_mode mode);
  virtual ~mm_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();
  virtual char *gets(char *buffer, size_t max_size);
};

class mm_devnull_io_callback: public IOCallback {
protected:
  int64_t pos;

public:
  mm_devnull_io_callback();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
};

#endif // __MM_IO_H
