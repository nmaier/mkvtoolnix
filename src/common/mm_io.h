/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * IO callback class definitions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
  bool dos_style_newlines;
  stack<int64_t> positions;

public:
  mm_io_c():
    dos_style_newlines(false) {
  }
  virtual ~mm_io_c() { }

  virtual uint64 getFilePointer() = 0;
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning)
    = 0;
  virtual bool setFilePointer2(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size) = 0;
  virtual uint32_t read(string &buffer, size_t size);
  virtual unsigned char read_uint8();
  virtual uint16_t read_uint16();
  virtual uint32_t read_uint24();
  virtual uint32_t read_uint32();
  virtual uint64_t read_uint64();
  virtual uint16_t read_uint16_be();
  virtual uint32_t read_uint24_be();
  virtual uint32_t read_uint32_be();
  virtual uint64_t read_uint64_be();
  virtual int write_uint8(unsigned char value);
  virtual int write_uint16(uint16_t value);
  virtual int write_uint32(uint32_t value);
  virtual int write_uint64(uint64_t value);
  virtual int write_uint16_be(uint16_t value);
  virtual int write_uint32_be(uint32_t value);
  virtual int write_uint64_be(uint64_t value);
  virtual void skip(int64 numbytes);
  virtual size_t write(const void *buffer, size_t size) = 0;
  virtual bool eof() = 0;

  virtual string get_file_name() const = 0;

  virtual string getline();
  virtual bool getline2(string &s);
  virtual size_t puts_unl(const string &s);
  virtual bool write_bom(const string &charset);
  virtual int getch();

  virtual void save_pos(int64_t new_pos = -1);
  virtual bool restore_pos();

  virtual int64_t get_size();

  virtual void close() = 0;

  virtual int printf(const char *fmt, ...);
  virtual void use_dos_style_newlines(bool yes) {
    dos_style_newlines = yes;
  }
};

class MTX_DLL_API mm_file_io_c: public mm_io_c {
protected:
#if defined(SYS_WINDOWS)
  bool _eof;
#endif
  string file_name;
  void *file;

public:
  mm_file_io_c(const string &path, const open_mode mode = MODE_READ);
  virtual ~mm_file_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();

  virtual string get_file_name() const {
    return file_name;
  }

  virtual int truncate(int64_t pos);
};

class MTX_DLL_API mm_proxy_io_c: public mm_io_c {
protected:
  mm_io_c *proxy_io;
  bool proxy_delete_io;

public:
  mm_proxy_io_c(mm_io_c *_proxy_io, bool _proxy_delete_io = true):
    proxy_io(_proxy_io),
    proxy_delete_io(_proxy_delete_io) {
  }
  virtual ~mm_proxy_io_c() {
    close();
  }

  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning) {
    return proxy_io->setFilePointer(offset, mode);
  }
  virtual uint64 getFilePointer() {
    return proxy_io->getFilePointer();
  }
  virtual uint32 read(void *buffer, size_t size) {
    return proxy_io->read(buffer, size);
  }
  virtual size_t write(const void *buffer, size_t size) {
    return proxy_io->write(buffer, size);
  }
  virtual bool eof() {
    return proxy_io->eof();
  }
  virtual void close();
  virtual string get_file_name() const {
    return proxy_io->get_file_name();
  }
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
  int64_t pos, mem_size, allocated, increase;
  unsigned char *mem;
  bool free_mem;
  string file_name;

public:
  mm_mem_io_c(unsigned char *_mem, uint64_t _mem_size, int _increase = 0);
  ~mm_mem_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof();
  virtual string get_file_name() const {
    return file_name;
  }
  virtual void set_file_name(const string &_file_name) {
    file_name = _file_name;
  }
};

enum byte_order_e {BO_UTF8, BO_UTF16_LE, BO_UTF16_BE, BO_UTF32_LE, BO_UTF32_BE,
                   BO_NONE};

class MTX_DLL_API mm_text_io_c: public mm_proxy_io_c {
protected:
  byte_order_e byte_order;
  int bom_len;

public:
  mm_text_io_c(mm_io_c *_in, bool _delete_in = true);

  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual string getline();
  virtual int read_next_char(char *buffer);
  virtual byte_order_e get_byte_order();
};

class MTX_DLL_API mm_stdio_c: public mm_io_c {
public:
  mm_stdio_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void close();
  virtual bool eof() {
    return false;
  }
  virtual string get_file_name() const {
    return "";
  }
};

#endif // __MM_IO_H
