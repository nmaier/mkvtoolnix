/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Helper routines for various compression libs

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CONTENT_DECODER_H
#define MTX_COMMON_CONTENT_DECODER_H

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "common/compression.h"

using namespace libmatroska;

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
  bool has_encodings() {
    return !encodings.empty();
  }
  std::string descriptive_algorithm_list();
};

#endif  // MTX_COMMON_CONTENT_DECODER_H
