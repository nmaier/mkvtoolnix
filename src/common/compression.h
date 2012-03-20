/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Helper routines for various compression libs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_COMPRESSION_H
#define MTX_COMMON_COMPRESSION_H

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>

/* compression types */
enum compression_method_e {
  COMPRESSION_UNSPECIFIED = 0,
  COMPRESSION_ZLIB,
  COMPRESSION_BZ2,
  COMPRESSION_LZO,
  COMPRESSION_HEADER_REMOVAL,
  COMPRESSION_MPEG4_P2,
  COMPRESSION_MPEG4_P10,
  COMPRESSION_DIRAC,
  COMPRESSION_DTS,
  COMPRESSION_AC3,
  COMPRESSION_MP3,
  COMPRESSION_ANALYZE_HEADER_REMOVAL,
  COMPRESSION_NONE,
  COMPRESSION_NUM = COMPRESSION_NONE
};

namespace mtx {
  class compression_x: public exception {
  protected:
    std::string m_message;
  public:
    compression_x(const std::string &message)  : m_message(message)       { }
    compression_x(const boost::format &message): m_message(message.str()) { }
    virtual ~compression_x() throw() { }

    virtual const char *what() const throw() {
      return m_message.c_str();
    }
  };
}

class compressor_c;
typedef counted_ptr<compressor_c> compressor_ptr;

class compressor_c {
protected:
  compression_method_e method;
  int64_t raw_size, compressed_size, items;

public:
  compressor_c(compression_method_e n_method):
    method(n_method), raw_size(0), compressed_size(0), items(0) {
  };

  virtual ~compressor_c();

  compression_method_e get_method() {
    return method;
  }


  virtual memory_cptr compress(memory_cptr const &buffer) {
    return do_compress(buffer);
  }
  virtual std::string compress(std::string const &buffer);

  virtual memory_cptr decompress(memory_cptr const &buffer) {
    return do_decompress(buffer);
  }
  virtual std::string decompress(std::string const &buffer);

  virtual void set_track_headers(KaxContentEncoding &c_encoding);

  static compressor_ptr create(compression_method_e method);
  static compressor_ptr create(const char *method);
  static compressor_ptr create_from_file_name(std::string const &file_name);

protected:
  virtual memory_cptr do_compress(memory_cptr const &buffer) {
    return buffer;
  }
  virtual memory_cptr do_decompress(memory_cptr const &buffer) {
    return buffer;
  }
};

#include "common/compression/bzlib.h"
#include "common/compression/header_removal.h"
#include "common/compression/lzo.h"
#include "common/compression/zlib.h"

#endif // MTX_COMMON_COMPRESSION_H
