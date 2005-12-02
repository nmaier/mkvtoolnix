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

#include "os.h"

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "commonebml.h"
#include "compression.h"
#include "hacks.h"

using namespace libmatroska;

const char *compression_methods[] = {
  "unspecified", "zlib", "bz2", "lzo", "header_removal", "mpeg4_p2", "none"
};

static const int compression_method_map[] = {
  0,                            // unspecified
  0,                            // zlib
  1,                            // bzlib
  2,                            // lzo1x
  3,                            // header removal
  3,                            // mpeg4_p2 is header removal
  0                             // none
};

// ---------------------------------------------------------------------

#ifdef HAVE_LZO

#ifdef HAVE_LZO_LZO1X_H
# include <lzo/lzoutil.h>
#else
# include <lzoutil.h>
#endif

lzo_compressor_c::lzo_compressor_c():
  compressor_c(COMPRESSION_LZO) {
  int result;

  if ((result = lzo_init()) != LZO_E_OK)
    mxerror("lzo_init() failed. Result: %d\n", result);
  wrkmem = (lzo_bytep)lzo_malloc(LZO1X_999_MEM_COMPRESS);
  if (wrkmem == NULL)
    mxerror("lzo_malloc(LZO1X_999_MEM_COMPRESS) failed.\n");
}

lzo_compressor_c::~lzo_compressor_c() {
  safefree(wrkmem);
}

void
lzo_compressor_c::decompress(memory_cptr &buffer) {
  die("lzo_compressor_c::decompress() not implemented\n");
}

void
lzo_compressor_c::compress(memory_cptr &buffer) {
  unsigned char *dst;
  int result, dstsize, size;

  size = buffer->get_size();
  dst = (unsigned char *)safemalloc(size * 2);

  lzo_uint lzo_dstsize = size * 2;
  if ((result = lzo1x_999_compress(buffer->get(), buffer->get_size(),
                                   dst, &lzo_dstsize, wrkmem)) != LZO_E_OK)
    mxerror("LZO compression failed. Result: %d\n", result);
  dstsize = lzo_dstsize;

  mxverb(3, "lzo_compressor_c: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  buffer = memory_cptr(new memory_c(dst, dstsize, true));
}

#endif // HAVE_LZO

// ---------------------------------------------------------------------

#if defined(HAVE_ZLIB_H)
zlib_compressor_c::zlib_compressor_c():
  compressor_c(COMPRESSION_ZLIB) {
}

zlib_compressor_c::~zlib_compressor_c() {
}

void
zlib_compressor_c::decompress(memory_cptr &buffer) {
  int result, dstsize, n;
  unsigned char *dst;
  z_stream d_stream;

  d_stream.zalloc = (alloc_func)0;
  d_stream.zfree = (free_func)0;
  d_stream.opaque = (voidpf)0;
  result = inflateInit(&d_stream);
  if (result != Z_OK)
    mxerror("inflateInit() failed. Result: %d\n", result);

  d_stream.next_in = (Bytef *)buffer->get();
  d_stream.avail_in = buffer->get_size();
  n = 0;
  dst = NULL;
  do {
    n++;
    dst = (unsigned char *)saferealloc(dst, n * 4000);
    d_stream.next_out = (Bytef *)&dst[(n - 1) * 4000];
    d_stream.avail_out = 4000;
    result = inflate(&d_stream, Z_NO_FLUSH);
    if ((result != Z_OK) && (result != Z_STREAM_END))
      mxerror("Zlib decompression failed. Result: %d\n", result);
  } while ((d_stream.avail_out == 0) && (d_stream.avail_in != 0) &&
           (result != Z_STREAM_END));

  dstsize = d_stream.total_out;
  inflateEnd(&d_stream);

  mxverb(3, "zlib_compressor_c: Decompression from %d to %d, %d%%\n",
         buffer->get_size(), dstsize, dstsize * 100 / buffer->get_size());

  dst = (unsigned char *)saferealloc(dst, dstsize);
  buffer = memory_cptr(new memory_c(dst, dstsize, true));
}

void
zlib_compressor_c::compress(memory_cptr &buffer) {
  int result, dstsize, n;
  unsigned char *dst;
  z_stream c_stream;

  c_stream.zalloc = (alloc_func)0;
  c_stream.zfree = (free_func)0;
  c_stream.opaque = (voidpf)0;
  result = deflateInit(&c_stream, 9);
  if (result != Z_OK)
    mxerror("deflateInit() failed. Result: %d\n", result);

  c_stream.next_in = (Bytef *)buffer->get();
  c_stream.avail_in = buffer->get_size();
  n = 0;
  dst = NULL;
  do {
    n++;
    dst = (unsigned char *)saferealloc(dst, n * 4000);
    c_stream.next_out = (Bytef *)&dst[(n - 1) * 4000];
    c_stream.avail_out = 4000;
    result = deflate(&c_stream, Z_FINISH);
    if ((result != Z_OK) && (result != Z_STREAM_END))
      mxerror("Zlib decompression failed. Result: %d\n", result);
  } while ((c_stream.avail_out == 0) && (result != Z_STREAM_END));
  dstsize = c_stream.total_out;
  deflateEnd(&c_stream);

  mxverb(3, "zlib_compressor_c: Compression from %d to %d, %d%%\n",
         buffer->get_size(), dstsize, dstsize * 100 / buffer->get_size());

  dst = (unsigned char *)saferealloc(dst, dstsize);
  buffer = memory_cptr(new memory_c(dst, dstsize, true));
}

#endif // HAVE_ZLIB_H

// ---------------------------------------------------------------------

#if defined(HAVE_BZLIB_H)
bzlib_compressor_c::bzlib_compressor_c():
  compressor_c(COMPRESSION_BZ2) {
}

bzlib_compressor_c::~bzlib_compressor_c() {
}

void
bzlib_compressor_c::decompress(memory_cptr &buffer) {
  int result;
  bz_stream d_stream;

  die("bzlib_compressor_c::decompress() not implemented\n");

  d_stream.bzalloc = NULL;
  d_stream.bzfree = NULL;
  d_stream.opaque = NULL;

  result = BZ2_bzDecompressInit(&d_stream, 0, 0);
  if (result != BZ_OK)
    mxerror("BZ2_bzCompressInit() failed. Result: %d\n", result);
  BZ2_bzDecompressEnd(&d_stream);
}

void
bzlib_compressor_c::compress(memory_cptr &buffer) {
  unsigned char *dst;
  int result, dstsize, size;
  bz_stream c_stream;

  size = buffer->get_size();
  dst = (unsigned char *)safemalloc(size * 2);

  c_stream.bzalloc = NULL;
  c_stream.bzfree = NULL;
  c_stream.opaque = NULL;

  result = BZ2_bzCompressInit(&c_stream, 9, 0, 30);
  if (result != BZ_OK)
    mxerror("BZ2_bzCompressInit() failed. Result: %d\n", result);

  c_stream.next_in = (char *)buffer->get();
  c_stream.next_out = (char *)dst;
  c_stream.avail_in = size;
  c_stream.avail_out = 2 * size;
  result = BZ2_bzCompress(&c_stream, BZ_FLUSH);
  if (result != BZ_RUN_OK)
    mxerror("bzip2 compression failed. Result: %d\n", result);

  BZ2_bzCompressEnd(&c_stream);

  dstsize = 2 * size - c_stream.avail_out;

  mxverb(3, "bzlib_compressor_c: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  buffer = memory_cptr(new memory_c(dst, dstsize, true));
}

#endif // HAVE_BZLIB_H

// ---------------------------------------------------------------------

header_removal_compressor_c::header_removal_compressor_c():
  compressor_c(COMPRESSION_HEADER_REMOVAL) {
}

void
header_removal_compressor_c::decompress(memory_cptr &buffer) {
  if ((NULL == m_bytes.get()) || (0 == m_bytes->get_size()))
    return;

  memory_cptr new_buffer(new memory_c((unsigned char *)
                                      safemalloc(buffer->get_size() +
                                                 m_bytes->get_size()),
                                      buffer->get_size() + m_bytes->get_size(),
                                      true));
  memcpy(new_buffer->get(), m_bytes->get(), m_bytes->get_size());
  memcpy(new_buffer->get() + m_bytes->get_size(), buffer->get(),
         buffer->get_size());

  buffer = new_buffer;
}

void
header_removal_compressor_c::compress(memory_cptr &buffer) {
  if ((NULL == m_bytes.get()) || (0 == m_bytes->get_size()))
    return;

  if (buffer->get_size() < m_bytes->get_size())
    throw compression_error_c(mxsprintf("Header removal compression not "
                                        "possible because "
                                        "the buffer contained %d bytes which "
                                        "is less than the size of the headers "
                                        "that should be removed, %d.",
                                        buffer->get_size(),
                                        m_bytes->get_size()));

  if (memcmp(buffer->get(), m_bytes->get(), m_bytes->get_size())) {
    string b_buffer, b_bytes;
    int i;

    for (i = 0; m_bytes->get_size() > i; ++i) {
      b_buffer += mxsprintf(" %02x", buffer->get()[i]);
      b_bytes += mxsprintf(" %02x", m_bytes->get()[i]);
    }
    throw compression_error_c(mxsprintf("Header removal compression not "
                                        "possible because the buffer did not "
                                        "start with the bytes that should be "
                                        "removed. Wanted bytes:%s; found:%s.",
                                        b_bytes.c_str(), b_buffer.c_str()));
  }

  memmove(buffer->get(), buffer->get() + m_bytes->get_size(),
          buffer->get_size() - m_bytes->get_size());
  buffer->set_size(buffer->get_size() - m_bytes->get_size());
}

void
header_removal_compressor_c::set_track_headers(KaxContentEncoding
                                               &c_encoding) {
  KaxContentCompression *c_comp;

  compressor_c::set_track_headers(c_encoding);

  c_comp = &GetChild<KaxContentCompression>(c_encoding);
  // Set compression parameters.
  static_cast<EbmlBinary *>(&GetChild<KaxContentCompSettings>(*c_comp))->
    SetBuffer(m_bytes->get(), m_bytes->get_size());
}

mpeg4_p2_compressor_c::mpeg4_p2_compressor_c() {
  if (!hack_engaged(ENGAGE_NATIVE_MPEG4))
    mxerror("The MPEG-4 part 2 compression only works with native MPEG-4. "
            "However, native MPEG-4 mode has not been selected with "
            "'--engage native_mpeg4'.\n");

  memory_cptr bytes(new memory_c((unsigned char *)safemalloc(4), 4, true));
  put_uint32_be(bytes->get(), 0x000001b6);
  set_bytes(bytes);
}

// ---------------------------------------------------------------------

compressor_c::~compressor_c() {
  if (items != 0)
    mxverb(2, "compression: Overall stats: raw size: " LLD ", compressed "
           "size: " LLD ", items: " LLD ", ratio: %.2f%%, avg bytes per item: "
           "" LLD "\n", raw_size, compressed_size, items,
           compressed_size * 100.0 / raw_size, compressed_size / items);
}

void
compressor_c::set_track_headers(KaxContentEncoding &c_encoding) {
  KaxContentCompression *c_comp;

  c_comp = &GetChild<KaxContentCompression>(c_encoding);
  // Set compression method.
  *static_cast<EbmlUInteger *>(&GetChild<KaxContentCompAlgo>(*c_comp)) =
    compression_method_map[method];
}

compressor_ptr
compressor_c::create(compression_method_e method) {
  if ((method <= COMPRESSION_UNSPECIFIED) ||
      (method > COMPRESSION_NUM))
    return compressor_ptr();

  return create(compression_methods[method]);
}

compressor_ptr
compressor_c::create(const char *method) {
#if defined(HAVE_LZO1X_H)
  if (!strcasecmp(method, compression_methods[COMPRESSION_LZO]))
    return compressor_ptr(new lzo_compressor_c());
#endif // HAVE_LZO1X_H

#if defined(HAVE_ZLIB_H)
  if (!strcasecmp(method, compression_methods[COMPRESSION_ZLIB]))
    return compressor_ptr(new zlib_compressor_c());
#endif // HAVE_ZLIB_H

#if defined(HAVE_BZLIB_H)
  if (!strcasecmp(method, compression_methods[COMPRESSION_BZ2]))
    return compressor_ptr(new bzlib_compressor_c());
#endif // HAVE_BZLIB_H

  if (!strcasecmp(method, compression_methods[COMPRESSION_MPEG4_P2]))
    return compressor_ptr(new mpeg4_p2_compressor_c());

  if (!strcasecmp(method, "none"))
    return compressor_ptr(new compressor_c(COMPRESSION_NONE));

  return compressor_ptr();
}

// ------------------------------------------------------------------------

kax_content_encoding_t::kax_content_encoding_t():
  order(0), type(0), scope(CONTENT_ENCODING_SCOPE_BLOCK), comp_algo(0),
  enc_algo(0), sig_algo(0), sig_hash_algo(0) {
}

content_decoder_c::content_decoder_c():
  ok(true) {
}

content_decoder_c::content_decoder_c(KaxTrackEntry &ktentry):
  ok(true) {
  initialize(ktentry);
}

content_decoder_c::~content_decoder_c() {
}

#define EBMLBIN_TO_COUNTEDMEM(bin) \
  memory_cptr(new memory_c((unsigned char *)safememdup(bin->GetBuffer(), \
                                                       bin->GetSize()), \
                           bin->GetSize(), true))

bool
content_decoder_c::initialize(KaxTrackEntry &ktentry) {
  KaxContentEncodings *kcencodings;
  int kcenc_idx, tid;

  encodings.clear();

  kcencodings = FINDFIRST(&ktentry, KaxContentEncodings);
  if (NULL == kcencodings)
    return true;

  tid = kt_get_number(ktentry);

  for (kcenc_idx = 0; kcenc_idx < kcencodings->ListSize(); kcenc_idx++) {
    KaxContentEncoding *kcenc;
    KaxContentEncodingOrder *ce_order;
    KaxContentEncodingType *ce_type;
    KaxContentEncodingScope *ce_scope;
    KaxContentCompression *ce_comp;
    KaxContentEncryption *ce_enc;
    kax_content_encoding_t enc;
    vector<kax_content_encoding_t>::iterator ce_ins_it;

    kcenc = dynamic_cast<KaxContentEncoding *>((*kcencodings)[kcenc_idx]);
    if (NULL == kcenc)
      continue;

    memset(&enc, 0, sizeof(kax_content_encoding_t));

    ce_order = FINDFIRST(kcenc, KaxContentEncodingOrder);
    if (ce_order != NULL)
      enc.order = uint32(*ce_order);

    ce_type = FINDFIRST(kcenc, KaxContentEncodingType);
    if (ce_type != NULL)
      enc.type = uint32(*ce_type);

    ce_scope = FINDFIRST(kcenc, KaxContentEncodingScope);
    if (ce_scope != NULL)
      enc.scope = uint32(*ce_scope);
    else
      enc.scope = 1;

    ce_comp = FINDFIRST(kcenc, KaxContentCompression);
    if (ce_comp != NULL) {
      KaxContentCompAlgo *cc_algo;
      KaxContentCompSettings *cc_settings;

      cc_algo = FINDFIRST(ce_comp, KaxContentCompAlgo);
      if (cc_algo != NULL)
        enc.comp_algo = uint32(*cc_algo);

      cc_settings = FINDFIRST(ce_comp, KaxContentCompSettings);
      if (cc_settings != NULL)
        enc.comp_settings = EBMLBIN_TO_COUNTEDMEM(cc_settings);
    }

    ce_enc = FINDFIRST(kcenc, KaxContentEncryption);
    if (ce_enc != NULL) {
      KaxContentEncAlgo *ce_ealgo;
      KaxContentEncKeyID *ce_ekeyid;
      KaxContentSigAlgo *ce_salgo;
      KaxContentSigHashAlgo *ce_shalgo;
      KaxContentSigKeyID *ce_skeyid;
      KaxContentSignature *ce_signature;

      ce_ealgo = FINDFIRST(ce_enc, KaxContentEncAlgo);
      if (ce_ealgo != NULL)
        enc.enc_algo = uint32(*ce_ealgo);

      ce_ekeyid = FINDFIRST(ce_enc, KaxContentEncKeyID);
      if (ce_ekeyid != NULL)
        enc.enc_keyid = EBMLBIN_TO_COUNTEDMEM(ce_ekeyid);

      ce_salgo = FINDFIRST(ce_enc, KaxContentSigAlgo);
      if (ce_salgo != NULL)
        enc.enc_algo = uint32(*ce_salgo);

      ce_shalgo = FINDFIRST(ce_enc, KaxContentSigHashAlgo);
      if (ce_shalgo != NULL)
        enc.enc_algo = uint32(*ce_shalgo);

      ce_skeyid = FINDFIRST(ce_enc, KaxContentSigKeyID);
      if (ce_skeyid != NULL)
        enc.sig_keyid = EBMLBIN_TO_COUNTEDMEM(ce_skeyid);

      ce_signature = FINDFIRST(ce_enc, KaxContentSignature);
      if (ce_signature != NULL)
        enc.signature = EBMLBIN_TO_COUNTEDMEM(ce_signature);

    }

    if (1 == enc.type) {
      mxwarn("Track number %d has been encrypted and decryption has "
             "not yet been implemented.\n", tid);
      ok = false;
      break;
    }

    if (0 != enc.type) {
      mxerror("Unknown content encoding type %u for track %d.\n", enc.type,
              tid);
      ok = false;
      break;
    }

    if (0 == enc.comp_algo) {
#if !defined(HAVE_ZLIB_H)
      mxwarn("Track %d was compressed with zlib but mkvmerge has not "
             "been compiled with support for zlib compression.\n", tid);
      ok = false;
      break;
#else
      enc.compressor = counted_ptr<compressor_c>(new zlib_compressor_c());
#endif
    } else if (1 == enc.comp_algo) {
#if !defined(HAVE_BZLIB_H)
      mxwarn("Track %d was compressed with bzlib but mkvmerge has not "
             "been compiled with support for bzlib compression.\n", tid);
      ok = false;
      break;
#else
      enc.compressor = counted_ptr<compressor_c>(new bzlib_compressor_c());
#endif
    } else if (enc.comp_algo == 2) {
#if !defined(HAVE_LZO1X_H)
      mxwarn("Track %d was compressed with lzo1x but mkvmerge has not "
             "been compiled with support for lzo1x compression.\n", tid);
      ok = false;
      break;
#else
      enc.compressor = counted_ptr<compressor_c>(new lzo_compressor_c());
#endif
    } else if (enc.comp_algo == 3) {
      header_removal_compressor_c *c = new header_removal_compressor_c();
      c->set_bytes(enc.comp_settings);
      enc.compressor = counted_ptr<compressor_c>(c);

    } else {
      mxwarn("Track %d has been compressed with an unknown/unsupported "
             "compression algorithm (%d).\n", tid,
             enc.comp_algo);
      ok = false;
      break;
    }

    ce_ins_it = encodings.begin();
    while ((ce_ins_it != encodings.end()) && (enc.order <= (*ce_ins_it).order))
      ce_ins_it++;
    encodings.insert(ce_ins_it, enc);
  }

  return ok;
}

void
content_decoder_c::reverse(memory_cptr &memory,
                           content_encoding_scope_e scope) {
  vector<kax_content_encoding_t>::const_iterator ce;

  if (!is_ok() || encodings.empty())
    return;

  foreach(ce, encodings)
    if (0 != (ce->scope & scope))
      ce->compressor->decompress(memory);
}
