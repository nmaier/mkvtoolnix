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
    \version $Id$
    \brief IO callback class definitions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MM_IO_H
#define __MM_IO_H

#include "os.h"

#include <stdint.h>

#include <string>

#include <ebml/IOCallback.h>

using namespace std;
using namespace LIBEBML_NAMESPACE;

class mm_io_c: public IOCallback {
protected:
#if !defined(SYS_UNIX)
  bool _eof;
#endif
  void *file;

public:
  mm_io_c();
  mm_io_c(const char *path, const open_mode mode);
  virtual ~mm_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual unsigned char read_uint8();
  virtual uint16_t read_uint16();
  virtual uint32_t read_uint32();
  virtual uint64_t read_uint64();
  virtual uint16_t read_uint16_be();
  virtual uint32_t read_uint32_be();
  virtual uint64_t read_uint64_be();
  virtual void skip(int64 numbytes);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();
  virtual char *gets(char *buffer, size_t max_size);
  virtual string getline();
  virtual size_t puts_unl(const char *s);
};

class mm_null_io_c: public mm_io_c {
protected:
  int64_t pos;

public:
  mm_null_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
};

#endif // __MM_IO_H
