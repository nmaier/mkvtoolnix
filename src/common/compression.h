/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Helper routines for various compression libs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMPRESSION_H
#define __COMPRESSION_H

#include "os.h"

#include <matroska/KaxTracks.h>

#include "common.h"

using namespace libmatroska;

/* compression types */
enum compression_method_e {
  COMPRESSION_UNSPECIFIED = 0,
  COMPRESSION_ZLIB,
  COMPRESSION_BZ2,
  COMPRESSION_LZO,
  COMPRESSION_NONE,
  COMPRESSION_NUM = COMPRESSION_NONE
};

extern const char *MTX_DLL_API xcompression_methods[];

class MTX_DLL_API compression_c {
protected:
  compression_method_e method;
  int64_t raw_size, compressed_size, items;

public:
  compression_c(compression_method_e _method):
    method(_method), raw_size(0), compressed_size(0), items(0) {
  };

  virtual ~compression_c();

  compression_method_e get_method() {
    return method;
  };

  virtual unsigned char *decompress(unsigned char *buffer, int &size) {
    return (unsigned char *)safememdup(buffer, size);
  };

  virtual unsigned char *compress(unsigned char *buffer, int &size) {
    return (unsigned char *)safememdup(buffer, size);
  };

  static compression_c *create(compression_method_e method);
  static compression_c *create(const char *method);
};

#if defined(HAVE_LZO1X_H)
#include <lzo1x.h>

class MTX_DLL_API lzo_compression_c: public compression_c {
protected:
  lzo_byte *wrkmem;

public:
  lzo_compression_c();
  virtual ~lzo_compression_c();

  virtual unsigned char *decompress(unsigned char *buffer, int &size);
  virtual unsigned char *compress(unsigned char *buffer, int &size);
};
#endif // HAVE_LZO1X_H

#if defined(HAVE_ZLIB_H)
#include <zlib.h>

class MTX_DLL_API zlib_compression_c: public compression_c {
public:
  zlib_compression_c();
  virtual ~zlib_compression_c();

  virtual unsigned char *decompress(unsigned char *buffer, int &size);
  virtual unsigned char *compress(unsigned char *buffer, int &size);
};
#endif // HAVE_ZLIB_H

#if defined(HAVE_BZLIB_H)
#include <bzlib.h>

class MTX_DLL_API bzlib_compression_c: public compression_c {
public:
  bzlib_compression_c();
  virtual ~bzlib_compression_c();

  virtual unsigned char *decompress(unsigned char *buffer, int &size);
  virtual unsigned char *compress(unsigned char *buffer, int &size);
};
#endif // HAVE_BZLIB_H

struct kax_content_encoding_t {
  uint32_t order, type, scope;
  uint32_t comp_algo;
  unsigned char *comp_settings;
  uint32_t comp_settings_len;
  uint32_t enc_algo, sig_algo, sig_hash_algo;
  unsigned char *enc_keyid, *sig_keyid, *signature;
  uint32_t enc_keyid_len, sig_keyid_len, signature_len;
};

enum content_encoding_scope_e {
  CONTENT_ENCODING_SCOPE_BLOCK = 1,
  CONTENT_ENCODING_SCOPE_CODECPRIVATE = 2
};

class MTX_DLL_API content_decoder_c {
protected:
  vector<kax_content_encoding_t> encodings;
  auto_ptr<compression_c> zlib_compressor, bzlib_compressor, lzo1x_compressor;
  bool ok;

public:
  content_decoder_c();
  content_decoder_c(KaxTrackEntry &ktentry);
  ~content_decoder_c();

  bool initialize(KaxTrackEntry &ktentry);
  bool reverse(unsigned char *&data, uint32_t &size,
               content_encoding_scope_e scope);
  bool is_ok() {
    return ok;
  }
};

#endif // __COMPRESSION_H
