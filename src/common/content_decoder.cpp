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

#include "common/content_decoder.h"
#include "common/ebml.h"
#include "common/strings/formatting.h"

kax_content_encoding_t::kax_content_encoding_t()
  : order(0)
  , type(0)
  , scope(CONTENT_ENCODING_SCOPE_BLOCK)
  , comp_algo(0)
  , enc_algo(0)
  , sig_algo(0)
  , sig_hash_algo(0)
{
}

content_decoder_c::content_decoder_c()
  : ok(true)
{
}

content_decoder_c::content_decoder_c(KaxTrackEntry &ktentry)
  : ok(true)
{
  initialize(ktentry);
}

content_decoder_c::~content_decoder_c() {
}

#define EBMLBIN_TO_COUNTEDMEM(bin) memory_c::clone(bin->GetBuffer(), bin->GetSize())

bool
content_decoder_c::initialize(KaxTrackEntry &ktentry) {
  encodings.clear();

  KaxContentEncodings *kcencodings = FindChild<KaxContentEncodings>(&ktentry);
  if (!kcencodings)
    return true;

  int tid = kt_get_number(ktentry);

  size_t kcenc_idx;
  for (kcenc_idx = 0; kcenc_idx < kcencodings->ListSize(); kcenc_idx++) {
    KaxContentEncoding *kcenc = dynamic_cast<KaxContentEncoding *>((*kcencodings)[kcenc_idx]);
    if (!kcenc)
      continue;

    kax_content_encoding_t enc;
    memset(&enc, 0, sizeof(kax_content_encoding_t));

    KaxContentEncodingOrder *ce_order = FindChild<KaxContentEncodingOrder>(kcenc);
    if (ce_order)
      enc.order = uint32(*ce_order);

    KaxContentEncodingType *ce_type = FindChild<KaxContentEncodingType>(kcenc);
    if (ce_type)
      enc.type = uint32(*ce_type);

    KaxContentEncodingScope *ce_scope = FindChild<KaxContentEncodingScope>(kcenc);
    enc.scope = ce_scope ? uint32(*ce_scope) : 1;

    KaxContentCompression *ce_comp = FindChild<KaxContentCompression>(kcenc);
    if (ce_comp) {
      KaxContentCompAlgo *cc_algo = FindChild<KaxContentCompAlgo>(ce_comp);
      if (cc_algo)
        enc.comp_algo = uint32(*cc_algo);

      KaxContentCompSettings *cc_settings = FindChild<KaxContentCompSettings>(ce_comp);
      if (cc_settings)
        enc.comp_settings = EBMLBIN_TO_COUNTEDMEM(cc_settings);
    }

    KaxContentEncryption *ce_enc = FindChild<KaxContentEncryption>(kcenc);
    if (ce_enc) {
      KaxContentEncAlgo *ce_ealgo = FindChild<KaxContentEncAlgo>(ce_enc);
      if (ce_ealgo)
        enc.enc_algo = uint32(*ce_ealgo);

      KaxContentEncKeyID *ce_ekeyid = FindChild<KaxContentEncKeyID>(ce_enc);
      if (ce_ekeyid)
        enc.enc_keyid = EBMLBIN_TO_COUNTEDMEM(ce_ekeyid);

      KaxContentSigAlgo *ce_salgo = FindChild<KaxContentSigAlgo>(ce_enc);
      if (ce_salgo)
        enc.enc_algo = uint32(*ce_salgo);

      KaxContentSigHashAlgo *ce_shalgo = FindChild<KaxContentSigHashAlgo>(ce_enc);
      if (ce_shalgo)
        enc.enc_algo = uint32(*ce_shalgo);

      KaxContentSigKeyID *ce_skeyid = FindChild<KaxContentSigKeyID>(ce_enc);
      if (ce_skeyid)
        enc.sig_keyid = EBMLBIN_TO_COUNTEDMEM(ce_skeyid);

      KaxContentSignature *ce_signature = FindChild<KaxContentSignature>(ce_enc);
      if (ce_signature)
        enc.signature = EBMLBIN_TO_COUNTEDMEM(ce_signature);

    }

    if (1 == enc.type) {
      mxwarn(boost::format(Y("Track number %1% has been encrypted and decryption has not yet been implemented.\n")) % tid);
      ok = false;
      break;
    }

    if (0 != enc.type) {
      mxerror(boost::format(Y("Unknown content encoding type %1% for track %2%.\n")) % enc.type % tid);
      ok = false;
      break;
    }

    if (0 == enc.comp_algo)
      enc.compressor = std::shared_ptr<compressor_c>(new zlib_compressor_c());

    else if (1 == enc.comp_algo) {
#if !defined(HAVE_BZLIB_H)
      mxwarn(boost::format(Y("Track %1% was compressed with bzlib but mkvmerge has not been compiled with support for bzlib compression.\n")) % tid);
      ok = false;
      break;
#else
      enc.compressor = std::shared_ptr<compressor_c>(new bzlib_compressor_c());
#endif
    } else if (enc.comp_algo == 2) {
#if !defined(HAVE_LZO1X_H)
      mxwarn(boost::format(Y("Track %1% was compressed with lzo1x but mkvmerge has not been compiled with support for lzo1x compression.\n")) % tid);
      ok = false;
      break;
#else
      enc.compressor = std::shared_ptr<compressor_c>(new lzo_compressor_c());
#endif
    } else if (enc.comp_algo == 3) {
      header_removal_compressor_c *c = new header_removal_compressor_c();
      c->set_bytes(enc.comp_settings);
      enc.compressor = std::shared_ptr<compressor_c>(c);

    } else {
      mxwarn(boost::format(Y("Track %1% has been compressed with an unknown/unsupported compression algorithm (%2%).\n")) % tid % enc.comp_algo);
      ok = false;
      break;
    }

    std::vector<kax_content_encoding_t>::iterator ce_ins_it = encodings.begin();
    while ((ce_ins_it != encodings.end()) && (enc.order <= (*ce_ins_it).order))
      ce_ins_it++;
    encodings.insert(ce_ins_it, enc);
  }

  return ok;
}

void
content_decoder_c::reverse(memory_cptr &memory,
                           content_encoding_scope_e scope) {
  if (!is_ok() || encodings.empty())
    return;

  for (auto &ce : encodings)
    if (0 != (ce.scope & scope))
      memory = ce.compressor->decompress(memory);
}

std::string
content_decoder_c::descriptive_algorithm_list() {
  std::string list;
  std::vector<std::string> algorithms;

  boost::for_each(encodings, [&](const kax_content_encoding_t &enc) { algorithms.push_back(to_string(enc.comp_algo)); });

  return join(",", algorithms);
}
