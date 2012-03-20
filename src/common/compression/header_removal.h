/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header removal compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_COMPRESSION_HEADER_REMOVAL_H
#define MTX_COMMON_COMPRESSION_HEADER_REMOVAL_H

#include "common/common_pch.h"

#include "common/compression.h"

class header_removal_compressor_c: public compressor_c {
protected:
  memory_cptr m_bytes;

public:
  header_removal_compressor_c();

  virtual void set_bytes(memory_cptr &bytes) {
    m_bytes = bytes;
    m_bytes->grab();
  }

  virtual memory_cptr do_decompress(memory_cptr const &buffer);
  virtual memory_cptr do_compress(memory_cptr const &buffer);

  virtual void set_track_headers(KaxContentEncoding &c_encoding);
};

class analyze_header_removal_compressor_c: public compressor_c {
protected:
  memory_cptr m_bytes;
  unsigned int m_packet_counter;

public:
  analyze_header_removal_compressor_c();
  virtual ~analyze_header_removal_compressor_c();

  virtual memory_cptr do_decompress(memory_cptr const &buffer);
  virtual memory_cptr do_compress(memory_cptr const &buffer);

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

#endif // MTX_COMMON_COMPRESSION_HEADER_REMOVAL_H
