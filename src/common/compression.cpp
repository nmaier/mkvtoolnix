/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Helper routines for various compression libs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "common/compression.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/strings/formatting.h"

using namespace libmatroska;

static const char *compression_methods[] = {
  "unspecified", "zlib", "header_removal", "mpeg4_p2", "mpeg4_p10", "dirac", "dts", "ac3", "mp3", "analyze_header_removal", "none"
};

static const int compression_method_map[] = {
  0,                            // unspecified
  0,                            // zlib
  3,                            // header removal
  3,                            // mpeg4_p2 is header removal
  3,                            // mpeg4_p10 is header removal
  3,                            // dirac is header removal
  3,                            // dts is header removal
  3,                            // ac3 is header removal
  3,                            // mp3 is header removal
  999999999,                    // analyze_header_removal
  0                             // none
};

compressor_c::~compressor_c() {
  if (0 == items)
    return;

  mxverb(2,
         boost::format("compression: Overall stats: raw size: %1%, compressed size: %2%, items: %3%, ratio: %|4$.2f|%%, avg bytes per item: %5%\n")
         % raw_size % compressed_size % items % (compressed_size * 100.0 / raw_size) % (compressed_size / items));
}

void
compressor_c::set_track_headers(KaxContentEncoding &c_encoding) {
  // Set compression method.
  GetChild<KaxContentCompAlgo>(GetChild<KaxContentCompression>(c_encoding)).SetValue(compression_method_map[method]);
}

compressor_ptr
compressor_c::create(compression_method_e method) {
  if ((COMPRESSION_UNSPECIFIED >= method) || (COMPRESSION_NUM < method))
    return compressor_ptr();

  return create(compression_methods[method]);
}

compressor_ptr
compressor_c::create(const char *method) {
  if (!strcasecmp(method, compression_methods[COMPRESSION_ZLIB]))
    return compressor_ptr(new zlib_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_MPEG4_P2]))
    return compressor_ptr(new mpeg4_p2_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_MPEG4_P10]))
    return compressor_ptr(new mpeg4_p10_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_DIRAC]))
    return compressor_ptr(new dirac_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_DTS]))
    return compressor_ptr(new dts_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_AC3]))
    return compressor_ptr(new ac3_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_MP3]))
    return compressor_ptr(new mp3_compressor_c());

  if (!strcasecmp(method, compression_methods[COMPRESSION_ANALYZE_HEADER_REMOVAL]))
    return compressor_ptr(new analyze_header_removal_compressor_c());

  if (!strcasecmp(method, "none"))
    return compressor_ptr(new compressor_c(COMPRESSION_NONE));

  return compressor_ptr();
}

compressor_ptr
compressor_c::create_from_file_name(std::string const &file_name) {
  auto pos = file_name.rfind(".");
  auto ext = balg::to_lower_copy(pos == std::string::npos ? file_name : file_name.substr(pos + 1));

  if (ext == "gz")
    return compressor_ptr(new zlib_compressor_c());

  return compressor_ptr(new compressor_c(COMPRESSION_NONE));
}

std::string
compressor_c::compress(std::string const &buffer) {
  auto new_buffer = compress(memory_cptr(new memory_c(const_cast<char *>(&buffer[0]), buffer.length(), false)));
  return std::string(reinterpret_cast<char const *>(new_buffer->get_buffer()), new_buffer->get_size());
}

std::string
compressor_c::decompress(std::string const &buffer) {
  auto new_buffer = decompress(memory_cptr(new memory_c(const_cast<char *>(&buffer[0]), buffer.length(), false)));
  return std::string(reinterpret_cast<char const *>(new_buffer->get_buffer()), new_buffer->get_size());
}
