/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  compression.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Helper routines for various compression libs
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __COMPRESSION_H
#define __COMPRESSION_H

#include "common.h"

/* compression types */
#define COMPRESSION_UNSPECIFIED   0
#define COMPRESSION_ZLIB          1
#define COMPRESSION_BZ2           2
#define COMPRESSION_LZO           3
#define COMPRESSION_NONE          4
#define COMPRESSION_NUM           4

extern const char *MTX_DLL_API xcompression_schemes[];

class MTX_DLL_API compression_c {
protected:
  int scheme;
  int64_t raw_size, compressed_size, items;

public:
  compression_c(int nscheme):
    scheme(nscheme), raw_size(0), compressed_size(0), items(0) {
  };

  virtual ~compression_c();

  int get_scheme() {
    return scheme;
  };

  virtual unsigned char *decompress(unsigned char *buffer, int &size) {
    return (unsigned char *)safememdup(buffer, size);
  };

  virtual unsigned char *compress(unsigned char *buffer, int &size) {
    return (unsigned char *)safememdup(buffer, size);
  };

  static compression_c *create(int scheme);
  static compression_c *create(const char *scheme);
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

#endif // __COMPRESSION_H
