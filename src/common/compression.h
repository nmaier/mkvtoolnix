/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Helper routines for various compression libs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_COMPRESSION_H
#define __MTX_COMMON_COMPRESSION_H

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "common/memory.h"
#include "common/error.h"

using namespace libmatroska;

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

extern const char *xcompression_methods[];

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
  };

  virtual void decompress(memory_cptr &/* buffer */) {
  };

  virtual void compress(memory_cptr &/* buffer */) {
  };

  virtual void set_track_headers(KaxContentEncoding &c_encoding);

  static compressor_ptr create(compression_method_e method);
  static compressor_ptr create(const char *method);
};

#ifdef HAVE_LZO

#ifdef HAVE_LZO_LZO1X_H
# include <lzo/lzo1x.h>
#else
# include <lzo1x.h>
#endif

class lzo_compressor_c: public compressor_c {
protected:
  lzo_byte *wrkmem;

public:
  lzo_compressor_c();
  virtual ~lzo_compressor_c();

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);
};
#endif // HAVE_LZO

#include <zlib.h>

class zlib_compressor_c: public compressor_c {
public:
  zlib_compressor_c();
  virtual ~zlib_compressor_c();

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);
};

#if defined(HAVE_BZLIB_H)
#include <bzlib.h>

class bzlib_compressor_c: public compressor_c {
public:
  bzlib_compressor_c();
  virtual ~bzlib_compressor_c();

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);
};
#endif // HAVE_BZLIB_H

class header_removal_compressor_c: public compressor_c {
protected:
  memory_cptr m_bytes;

public:
  header_removal_compressor_c();

  virtual void set_bytes(memory_cptr &bytes) {
    m_bytes = bytes;
    m_bytes->grab();
  }

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);

  virtual void set_track_headers(KaxContentEncoding &c_encoding);
};

class analyze_header_removal_compressor_c: public compressor_c {
protected:
  memory_cptr m_bytes;
  unsigned int m_packet_counter;

public:
  analyze_header_removal_compressor_c();
  virtual ~analyze_header_removal_compressor_c();

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);

  virtual void set_track_headers(KaxContentEncoding &c_encoding);
};

class mpeg4_p2_compressor_c: public header_removal_compressor_c {
public:
  mpeg4_p2_compressor_c();
};

class mpeg4_p10_compressor_c: public header_removal_compressor_c {
public:
  mpeg4_p10_compressor_c();
};

class dirac_compressor_c: public header_removal_compressor_c {
public:
  dirac_compressor_c();
};

class dts_compressor_c: public header_removal_compressor_c {
public:
  dts_compressor_c();
};

class ac3_compressor_c: public header_removal_compressor_c {
public:
  ac3_compressor_c();
};

class mp3_compressor_c: public header_removal_compressor_c {
public:
  mp3_compressor_c();
};

// ------------------------------------------------------------------

enum content_encoding_scope_e {
  CONTENT_ENCODING_SCOPE_BLOCK = 1,
  CONTENT_ENCODING_SCOPE_CODECPRIVATE = 2
};

struct kax_content_encoding_t {
  uint32_t order, type, scope;
  uint32_t comp_algo;
  memory_cptr comp_settings;
  uint32_t enc_algo, sig_algo, sig_hash_algo;
  memory_cptr enc_keyid, sig_keyid, signature;

  counted_ptr<compressor_c> compressor;

  kax_content_encoding_t();
};

class content_decoder_c {
protected:
  std::vector<kax_content_encoding_t> encodings;
  bool ok;

public:
  content_decoder_c();
  content_decoder_c(KaxTrackEntry &ktentry);
  ~content_decoder_c();

  bool initialize(KaxTrackEntry &ktentry);
  void reverse(memory_cptr &data, content_encoding_scope_e scope);
  bool is_ok() {
    return ok;
  }
};

#endif // __MTX_COMMON_COMPRESSION_H
