/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_MM_IO_H
#define __MTX_COMMON_MM_IO_H

#include "common/common_pch.h"

#include <stack>
#if HAVE_POSIX_FADVISE
# include <unistd.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif

#include <ebml/IOCallback.h>

#include "common/error.h"
#include "common/memory.h"
#include "common/smart_pointers.h"

using namespace libebml;

namespace mtx {
  namespace mm_io {
    class exception: public mtx::exception {
    public:
      virtual const char *what() const throw() {
        return "unspecified I/O error";
      }
    };

    class end_of_file_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "end of file error";
      }
    };

    class seek_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "seek in file error";
      }
    };

    class open_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "open file error";
      }
    };

    class wrong_read_write_access_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "write operation to read-only file or vice versa";
      }
    };

    class insufficient_space_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "insufficient space for write operation";
      }
    };

    class create_directory_x: public exception {
    protected:
      std::string m_path, m_error;
      int m_error_number;
    public:
      create_directory_x(const std::string &path, const std::string &error, int error_number)
        : m_path(path)
        , m_error(error)
        , m_error_number(error_number)
      {
      }
      virtual ~create_directory_x() throw() { }

      virtual const char *what() const throw() {
        return "create_directory() failed";
      }
      virtual std::string error() const throw() {
        return (boost::format(Y("mkdir(%1%) failed; errno = %2% (%3%)")) % m_path % m_error_number % m_error).str();
      }
    };
  }
}

class mm_io_c;
typedef counted_ptr<mm_io_c> mm_io_cptr;

class mm_io_c: public IOCallback {
protected:
  bool m_dos_style_newlines;
  std::stack<int64_t> m_positions;
  int64_t m_current_position, m_cached_size;

public:
  mm_io_c()
    : m_dos_style_newlines(false)
    , m_current_position(0)
    , m_cached_size(-1)
  {
  }
  virtual ~mm_io_c() { }

  virtual uint64 getFilePointer() = 0;
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning) = 0;
  virtual bool setFilePointer2(int64 offset, seek_mode mode = seek_beginning);
  virtual uint32 read(void *buffer, size_t size);
  virtual uint32_t read(std::string &buffer, size_t size, size_t offset = 0);
  virtual uint32_t read(memory_cptr &buffer, size_t size, int offset = 0);
  virtual unsigned char read_uint8();
  virtual uint16_t read_uint16_le();
  virtual uint32_t read_uint24_le();
  virtual uint32_t read_uint32_le();
  virtual uint64_t read_uint64_le();
  virtual uint16_t read_uint16_be();
  virtual uint32_t read_uint24_be();
  virtual uint32_t read_uint32_be();
  virtual uint64_t read_uint64_be();
  virtual int write_uint8(unsigned char value);
  virtual int write_uint16_le(uint16_t value);
  virtual int write_uint32_le(uint32_t value);
  virtual int write_uint64_le(uint64_t value);
  virtual int write_uint16_be(uint16_t value);
  virtual int write_uint32_be(uint32_t value);
  virtual int write_uint64_be(uint64_t value);
  virtual void skip(int64 numbytes);
  virtual size_t write(const void *buffer, size_t size);
  virtual size_t write(const memory_cptr &buffer, size_t size = UINT_MAX, size_t offset = 0);
  virtual bool eof() = 0;
  virtual void flush() {
  }
  virtual int truncate(int64_t) {
    return 0;
  }

  virtual std::string get_file_name() const = 0;

  virtual std::string getline();
  virtual bool getline2(std::string &s);
  virtual size_t puts(const std::string &s);
  inline size_t puts(const boost::format &format) {
    return puts(format.str());
  }
  virtual bool write_bom(const std::string &charset);
  virtual int getch();

  virtual void save_pos(int64_t new_pos = -1);
  virtual bool restore_pos();

  virtual int64_t get_size();

  virtual void close() = 0;

  virtual void use_dos_style_newlines(bool yes) {
    m_dos_style_newlines = yes;
  }

protected:
  virtual uint32 _read(void *buffer, size_t size) = 0;
  virtual size_t _write(const void *buffer, size_t size) = 0;
};

#if HAVE_POSIX_FADVISE
struct file_id_t {
  bool m_initialized;
  dev_t m_dev;
  ino_t m_ino;

  file_id_t()
    : m_initialized(false)
    , m_dev(0)
    , m_ino(0)
  {
  }

  void initialize(const struct stat &st) {
    m_dev         = st.st_dev;
    m_ino         = st.st_ino;
    m_initialized = true;
  }
};
#endif

class mm_file_io_c: public mm_io_c {
protected:
  std::string m_file_name;
  void *m_file;

#if defined(SYS_WINDOWS)
  bool m_eof;
#endif
#if HAVE_POSIX_FADVISE
  file_id_t m_file_id;
  unsigned long m_read_count, m_write_count;
  static bool ms_use_posix_fadvise;
  bool m_use_posix_fadvise_here;
  std::string m_canonicalized_file_name;
#endif

public:
  mm_file_io_c(const std::string &path, const open_mode mode = MODE_READ);
  virtual ~mm_file_io_c();

  static void prepare_path(const std::string &path);

  virtual uint64 getFilePointer();
#if defined(SYS_WINDOWS)
  virtual uint64 get_real_file_pointer();
#endif
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual void close();
  virtual bool eof();

  virtual std::string get_file_name() const {
    return m_file_name;
  }

  virtual int truncate(int64_t pos);

#if HAVE_POSIX_FADVISE
  void setup_fadvise(const std::string &local_path);
#endif

  static void setup();
  static void cleanup();
  static mm_io_cptr open(const std::string &path, const open_mode mode = MODE_READ);

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
};

typedef counted_ptr<mm_file_io_c> mm_file_io_cptr;

class mm_proxy_io_c: public mm_io_c {
protected:
  mm_io_c *m_proxy_io;
  bool m_proxy_delete_io;

public:
  mm_proxy_io_c(mm_io_c *proxy_io, bool proxy_delete_io = true)
    : m_proxy_io(proxy_io)
    , m_proxy_delete_io(proxy_delete_io)
  {
  }
  virtual ~mm_proxy_io_c() {
    close();
  }

  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning) {
    return m_proxy_io->setFilePointer(offset, mode);
  }
  virtual uint64 getFilePointer() {
    return m_proxy_io->getFilePointer();
  }
  virtual bool eof() {
    return m_proxy_io->eof();
  }
  virtual void close();
  virtual std::string get_file_name() const {
    return m_proxy_io->get_file_name();
  }

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
};

typedef counted_ptr<mm_proxy_io_c> mm_proxy_io_cptr;

class mm_null_io_c: public mm_io_c {
protected:
  int64_t m_pos;

public:
  mm_null_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual void close();

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
};

typedef counted_ptr<mm_null_io_c> mm_null_io_cptr;

class mm_mem_io_c: public mm_io_c {
protected:
  size_t m_pos, m_mem_size, m_allocated, m_increase;
  unsigned char *m_mem;
  const unsigned char *m_ro_mem;
  bool m_free_mem, m_read_only;
  std::string m_file_name;

public:
  mm_mem_io_c(unsigned char *mem, uint64_t mem_size, int increase);
  mm_mem_io_c(const unsigned char *mem, uint64_t mem_size);
  ~mm_mem_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual void close();
  virtual bool eof();
  virtual std::string get_file_name() const {
    return m_file_name;
  }
  virtual void set_file_name(const std::string &file_name) {
    m_file_name = file_name;
  }

  virtual unsigned char *get_and_lock_buffer();

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
};

typedef counted_ptr<mm_mem_io_c> mm_mem_io_cptr;

enum byte_order_e {BO_UTF8, BO_UTF16_LE, BO_UTF16_BE, BO_UTF32_LE, BO_UTF32_BE, BO_NONE};

class mm_text_io_c: public mm_proxy_io_c {
protected:
  byte_order_e m_byte_order;
  unsigned int m_bom_len;
  bool m_uses_carriage_returns, m_uses_newlines, m_eol_style_detected;

public:
  mm_text_io_c(mm_io_c *in, bool delete_in = true);

  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual std::string getline();
  virtual int read_next_char(char *buffer);
  virtual byte_order_e get_byte_order();
  virtual void set_byte_order(byte_order_e byte_order) {
    m_byte_order = byte_order;
  }

protected:
  virtual void detect_eol_style();

public:
  static bool has_byte_order_marker(const std::string &string);
  static bool detect_byte_order_marker(const unsigned char *buffer, unsigned int size, byte_order_e &byte_order, unsigned int &bom_length);
};

typedef counted_ptr<mm_text_io_c> mm_text_io_cptr;

class mm_stdio_c: public mm_io_c {
public:
  mm_stdio_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode=seek_beginning);
  virtual void close();
  virtual bool eof() {
    return false;
  }
  virtual std::string get_file_name() const {
    return "";
  }
  virtual void flush();

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
};

typedef counted_ptr<mm_stdio_c> mm_stdio_cptr;

#endif // __MTX_COMMON_MM_IO_H
