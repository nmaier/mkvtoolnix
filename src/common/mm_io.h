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

#include <string>
#include <stack>

#include <ebml/IOCallback.h>

using namespace std;
using namespace libebml;

class MTX_DLL_API mm_io_c: public IOCallback {
protected:
#if !defined(SYS_UNIX)
  bool _eof;
#endif
  char *file_name;
  void *file;
  stack<int64_t> positions;

public:
  mm_io_c();
  mm_io_c(const char *path, const open_mode mode);
  virtual ~mm_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual bool setFilePointer2(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual unsigned char read_uint8();
  virtual uint16_t read_uint16();
  virtual uint32_t read_uint24();
  virtual uint32_t read_uint32();
  virtual uint64_t read_uint64();
  virtual uint16_t read_uint16_be();
  virtual uint32_t read_uint24_be();
  virtual uint32_t read_uint32_be();
  virtual uint64_t read_uint64_be();
  virtual void skip(int64 numbytes);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();
  virtual string getline();
  virtual bool getline2(string &s);
  virtual size_t puts_unl(const char *s);
  virtual bool write_bom(const char *charset);
  virtual int getch();

  virtual void save_pos(int64_t new_pos = -1);
  virtual bool restore_pos();

  virtual int64_t get_size();

  virtual const char *get_file_name();
};

class MTX_DLL_API mm_null_io_c: public mm_io_c {
protected:
  int64_t pos;

public:
  mm_null_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
};

class MTX_DLL_API mm_mem_io_c: public mm_io_c {
protected:
  int64_t pos, mem_size;
  unsigned char *mem;

public:
  mm_mem_io_c(unsigned char *nmem, uint64_t nsize);

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();
};

enum byte_order_e {BO_UTF8, BO_UTF16_LE, BO_UTF16_BE, BO_UTF32_LE, BO_UTF32_BE,
                   BO_NONE};

class MTX_DLL_API mm_text_io_c: public mm_io_c {
protected:
  byte_order_e byte_order;
  int bom_len;

public:
  mm_text_io_c(const char *path);

  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual string getline();
  virtual int read_next_char(char *buffer);
  virtual byte_order_e get_byte_order();
};

#endif // __MM_IO_H
