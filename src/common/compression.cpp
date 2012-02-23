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

#include <boost/range/algorithm/for_each.hpp>

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>

#include "common/ac3.h"
#include "common/compression.h"
#include "common/dirac.h"
#include "common/dts.h"
#include "common/ebml.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/strings/formatting.h"

using namespace libmatroska;

const char *compression_methods[] = {
  "unspecified", "zlib", "bz2", "lzo", "header_removal", "mpeg4_p2", "mpeg4_p10", "dirac", "dts", "ac3", "mp3", "analyze_header_removal", "none"
};

static const int compression_method_map[] = {
  0,                            // unspecified
  0,                            // zlib
  1,                            // bzlib
  2,                            // lzo1x
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

// ---------------------------------------------------------------------

#ifdef HAVE_LZO

#ifdef HAVE_LZO_LZO1X_H
# include <lzo/lzoutil.h>
#else
# include <lzoutil.h>
#endif

lzo_compressor_c::lzo_compressor_c()
  : compressor_c(COMPRESSION_LZO)
{
  int result;
  if ((result = lzo_init()) != LZO_E_OK)
    mxerror(boost::format(Y("lzo_init() failed. Result: %1%\n")) % result);

  wrkmem = (lzo_bytep)lzo_malloc(LZO1X_999_MEM_COMPRESS);
  if (NULL == wrkmem)
    mxerror(Y("lzo_malloc(LZO1X_999_MEM_COMPRESS) failed.\n"));
}

lzo_compressor_c::~lzo_compressor_c() {
  safefree(wrkmem);
}

void
lzo_compressor_c::decompress(memory_cptr &) {
  mxerror(Y("lzo_compressor_c::decompress() not implemented\n"));
}

void
lzo_compressor_c::compress(memory_cptr &buffer) {
  int size             = buffer->get_size();
  unsigned char *dst   = (unsigned char *)safemalloc(size * 2);
  lzo_uint lzo_dstsize = size * 2;

  int result;
  if ((result = lzo1x_999_compress(buffer->get_buffer(), buffer->get_size(), dst, &lzo_dstsize, wrkmem)) != LZO_E_OK)
    mxerror(boost::format(Y("LZO compression failed. Result: %1%\n")) % result);

  int dstsize = lzo_dstsize;

  mxverb(3, boost::format("lzo_compressor_c: Compression from %1% to %2%, %3%%%\n") % size % dstsize % (dstsize * 100 / size));

  raw_size        += size;
  compressed_size += dstsize;
  items++;

  buffer = memory_cptr(new memory_c((unsigned char *)saferealloc(dst, dstsize), dstsize, true));
}

#endif // HAVE_LZO

// ---------------------------------------------------------------------

zlib_compressor_c::zlib_compressor_c()
  : compressor_c(COMPRESSION_ZLIB)
{
}

zlib_compressor_c::~zlib_compressor_c() {
}

void
zlib_compressor_c::decompress(memory_cptr &buffer) {
  z_stream d_stream;

  d_stream.zalloc = (alloc_func)0;
  d_stream.zfree  = (free_func)0;
  d_stream.opaque = (voidpf)0;
  int result      = inflateInit(&d_stream);

  if (Z_OK != result)
    mxerror(boost::format(Y("inflateInit() failed. Result: %1%\n")) % result);

  d_stream.next_in   = (Bytef *)buffer->get_buffer();
  d_stream.avail_in  = buffer->get_size();
  int n              = 0;
  unsigned char *dst = NULL;

  do {
    n++;
    dst                = (unsigned char *)saferealloc(dst, n * 4000);
    d_stream.next_out  = (Bytef *)&dst[(n - 1) * 4000];
    d_stream.avail_out = 4000;
    result             = inflate(&d_stream, Z_NO_FLUSH);

    if ((Z_OK != result) && (Z_STREAM_END != result))
      mxerror(boost::format(Y("Zlib decompression failed. Result: %1%\n")) % result);

  } while ((0 == d_stream.avail_out) && (0 != d_stream.avail_in) && (Z_STREAM_END != result));

  int dstsize = d_stream.total_out;
  inflateEnd(&d_stream);

  mxverb(3, boost::format("zlib_compressor_c: Decompression from %1% to %2%, %3%%%\n") % buffer->get_size() % dstsize % (dstsize * 100 / buffer->get_size()));

  buffer = memory_cptr(new memory_c((unsigned char *)saferealloc(dst, dstsize), dstsize, true));
}

void
zlib_compressor_c::compress(memory_cptr &buffer) {
  z_stream c_stream;

  c_stream.zalloc = (alloc_func)0;
  c_stream.zfree  = (free_func)0;
  c_stream.opaque = (voidpf)0;
  int result      = deflateInit(&c_stream, 9);

  if (Z_OK != result)
    mxerror(boost::format(Y("deflateInit() failed. Result: %1%\n")) % result);

  c_stream.next_in   = (Bytef *)buffer->get_buffer();
  c_stream.avail_in  = buffer->get_size();
  int n              = 0;
  unsigned char *dst = NULL;

  do {
    n++;
    dst                = (unsigned char *)saferealloc(dst, n * 4000);
    c_stream.next_out  = (Bytef *)&dst[(n - 1) * 4000];
    c_stream.avail_out = 4000;
    result             = deflate(&c_stream, Z_FINISH);

    if ((Z_OK != result) && (Z_STREAM_END != result))
      mxerror(boost::format(Y("Zlib decompression failed. Result: %1%\n")) % result);

  } while ((c_stream.avail_out == 0) && (result != Z_STREAM_END));

  int dstsize = c_stream.total_out;
  deflateEnd(&c_stream);

  mxverb(3, boost::format("zlib_compressor_c: Compression from %1% to %2%, %3%%%\n") % buffer->get_size() % dstsize % (dstsize * 100 / buffer->get_size()));

  buffer = memory_cptr(new memory_c((unsigned char *)saferealloc(dst, dstsize), dstsize, true));
}

// ---------------------------------------------------------------------

#if defined(HAVE_BZLIB_H)
bzlib_compressor_c::bzlib_compressor_c()
  : compressor_c(COMPRESSION_BZ2)
{
}

bzlib_compressor_c::~bzlib_compressor_c() {
}

void
bzlib_compressor_c::decompress(memory_cptr &) {
  mxerror(Y("bzlib_compressor_c::decompress() not implemented\n"));

  bz_stream d_stream;

  d_stream.bzalloc = NULL;
  d_stream.bzfree  = NULL;
  d_stream.opaque  = NULL;
  int result       = BZ2_bzDecompressInit(&d_stream, 0, 0);

  if (BZ_OK != result)
    mxerror(boost::format(Y("BZ2_bzCompressInit() failed. Result: %1%\n")) % result);

  BZ2_bzDecompressEnd(&d_stream);
}

void
bzlib_compressor_c::compress(memory_cptr &buffer) {
  bz_stream c_stream;

  int size           = buffer->get_size();
  unsigned char *dst = (unsigned char *)safemalloc(size * 2);

  c_stream.bzalloc   = NULL;
  c_stream.bzfree    = NULL;
  c_stream.opaque    = NULL;

  int result         = BZ2_bzCompressInit(&c_stream, 9, 0, 30);
  if (BZ_OK != result)
    mxerror(boost::format(Y("BZ2_bzCompressInit() failed. Result: %1%\n")) % result);

  c_stream.next_in   = (char *)buffer->get_buffer();
  c_stream.next_out  = (char *)dst;
  c_stream.avail_in  = size;
  c_stream.avail_out = 2 * size;
  result             = BZ2_bzCompress(&c_stream, BZ_FINISH);

  if (BZ_STREAM_END != result)
    mxerror(boost::format(Y("bzip2 compression failed. Result: %1%\n")) % result);

  BZ2_bzCompressEnd(&c_stream);

  int dstsize = 2 * size - c_stream.avail_out;

  mxverb(3, boost::format("bzlib_compressor_c: Compression from %1% to %2%, %3%%%\n") % size % dstsize % (dstsize * 100 / size));

  raw_size        += size;
  compressed_size += dstsize;
  items++;

  buffer = memory_cptr(new memory_c((unsigned char *)saferealloc(dst, dstsize), dstsize, true));
}

#endif // HAVE_BZLIB_H

// ---------------------------------------------------------------------

header_removal_compressor_c::header_removal_compressor_c()
  : compressor_c(COMPRESSION_HEADER_REMOVAL)
{
}

void
header_removal_compressor_c::decompress(memory_cptr &buffer) {
  if (!m_bytes.is_set() || (0 == m_bytes->get_size()))
    return;

  memory_cptr new_buffer = memory_c::alloc(buffer->get_size() + m_bytes->get_size());

  memcpy(new_buffer->get_buffer(),                       m_bytes->get_buffer(), m_bytes->get_size());
  memcpy(new_buffer->get_buffer() + m_bytes->get_size(), buffer->get_buffer(),  buffer->get_size());

  buffer = new_buffer;
}

void
header_removal_compressor_c::compress(memory_cptr &buffer) {
  if (!m_bytes.is_set() || (0 == m_bytes->get_size()))
    return;

  size_t size = m_bytes->get_size();
  if (buffer->get_size() < size)
    throw mtx::compression_x(boost::format(Y("Header removal compression not possible because the buffer contained %1% bytes "
                                             "which is less than the size of the headers that should be removed, %2%.")) % buffer->get_size() % size);

  unsigned char *buffer_ptr = buffer->get_buffer();
  unsigned char *bytes_ptr  = m_bytes->get_buffer();

  if (memcmp(buffer_ptr, bytes_ptr, size)) {
    std::string b_buffer, b_bytes;
    size_t i;

    for (i = 0; size > i; ++i) {
      b_buffer += (boost::format(" %|1$02x|") % static_cast<unsigned int>(buffer_ptr[i])).str();
      b_bytes  += (boost::format(" %|1$02x|") % static_cast<unsigned int>(bytes_ptr[i])).str();
    }
    throw mtx::compression_x(boost::format(Y("Header removal compression not possible because the buffer did not start with the bytes that should be removed. "
                                             "Wanted bytes:%1%; found:%2%.")) % b_bytes % b_buffer);
  }

  size_t new_size = buffer->get_size() - size;

  if (buffer->is_free()) {
    memmove(buffer_ptr, buffer_ptr + size, new_size);
    buffer->set_size(new_size);
  } else
    buffer = memory_c::clone(buffer_ptr + size, new_size);
}

void
header_removal_compressor_c::set_track_headers(KaxContentEncoding &c_encoding) {
  compressor_c::set_track_headers(c_encoding);

  // Set compression parameters.
  GetChild<KaxContentCompSettings>(GetChild<KaxContentCompression>(c_encoding)).CopyBuffer(m_bytes->get_buffer(), m_bytes->get_size());
}

// ------------------------------------------------------------

analyze_header_removal_compressor_c::analyze_header_removal_compressor_c()
  : compressor_c(COMPRESSION_ANALYZE_HEADER_REMOVAL)
  , m_packet_counter(0)
{
}

analyze_header_removal_compressor_c::~analyze_header_removal_compressor_c() {
  if (!m_bytes.is_set())
    mxinfo("Analysis failed: no packet encountered\n");

  else if (m_bytes->get_size() == 0)
    mxinfo("Analysis complete but no similarities found.\n");

  else {
    mxinfo(boost::format("Analysis complete. %1% identical byte(s) at the start of each of the %2% packet(s). Hex dump of the content:\n") % m_bytes->get_size() % m_packet_counter);
    mxhexdump(0, m_bytes->get_buffer(), m_bytes->get_size());
  }
}

void
analyze_header_removal_compressor_c::decompress(memory_cptr &) {
  mxerror("analyze_header_removal_compressor_c::decompress(): not supported\n");
}

void
analyze_header_removal_compressor_c::compress(memory_cptr &buffer) {
  ++m_packet_counter;

  if (!m_bytes.is_set()) {
    m_bytes = memory_cptr(buffer->clone());
    return;
  }

  unsigned char *current = buffer->get_buffer();
  unsigned char *saved   = m_bytes->get_buffer();
  size_t i, new_size     = 0;

  for (i = 0; i < std::min(buffer->get_size(), m_bytes->get_size()); ++i, ++new_size)
    if (current[i] != saved[i])
      break;

  m_bytes->set_size(new_size);
}

void
analyze_header_removal_compressor_c::set_track_headers(KaxContentEncoding &) {
}

// ------------------------------------------------------------

mpeg4_p2_compressor_c::mpeg4_p2_compressor_c() {
  memory_cptr bytes = memory_c::alloc(3);
  put_uint24_be(bytes->get_buffer(), 0x000001);
  set_bytes(bytes);
}

mpeg4_p10_compressor_c::mpeg4_p10_compressor_c() {
  memory_cptr bytes      = memory_c::alloc(1);
  bytes->get_buffer()[0] = 0;
  set_bytes(bytes);
}

dirac_compressor_c::dirac_compressor_c() {
  memory_cptr bytes = memory_c::alloc(4);
  put_uint32_be(bytes->get_buffer(), DIRAC_SYNC_WORD);
  set_bytes(bytes);
}

dts_compressor_c::dts_compressor_c() {
  memory_cptr bytes = memory_c::alloc(4);
  put_uint32_be(bytes->get_buffer(), DTS_HEADER_MAGIC);
  set_bytes(bytes);
}

ac3_compressor_c::ac3_compressor_c() {
  memory_cptr bytes = memory_c::alloc(2);
  put_uint16_be(bytes->get_buffer(), AC3_SYNC_WORD);
  set_bytes(bytes);
}

mp3_compressor_c::mp3_compressor_c() {
  memory_cptr bytes = memory_c::alloc(1);
  bytes->get_buffer()[0] = 0xff;
  set_bytes(bytes);
}

// ---------------------------------------------------------------------

compressor_c::~compressor_c() {
  if (0 == items)
    return;

  mxverb(2,
         boost::format("compression: Overall stats: raw size: %1%, compressed size: %2%, items: %3%, ratio: %|4$.2f|%%, avg bytes per item: %5%\n")
         % raw_size % compressed_size % items % (compressed_size * 100.0 / raw_size) % (compressed_size / items));
}

void
compressor_c::set_track_headers(KaxContentEncoding &c_encoding) {
  KaxContentCompression *c_comp = &GetChild<KaxContentCompression>(c_encoding);
  // Set compression method.
  GetChildAs<KaxContentCompAlgo, EbmlUInteger>(*c_comp) = compression_method_map[method];
}

compressor_ptr
compressor_c::create(compression_method_e method) {
  if ((COMPRESSION_UNSPECIFIED >= method) || (COMPRESSION_NUM < method))
    return compressor_ptr();

  return create(compression_methods[method]);
}

compressor_ptr
compressor_c::create(const char *method) {
#if defined(HAVE_LZO)
  if (!strcasecmp(method, compression_methods[COMPRESSION_LZO]))
    return compressor_ptr(new lzo_compressor_c());
#endif // HAVE_LZO1X_H

  if (!strcasecmp(method, compression_methods[COMPRESSION_ZLIB]))
    return compressor_ptr(new zlib_compressor_c());

#if defined(HAVE_BZLIB_H)
  if (!strcasecmp(method, compression_methods[COMPRESSION_BZ2]))
    return compressor_ptr(new bzlib_compressor_c());
#endif // HAVE_BZLIB_H

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

// ------------------------------------------------------------------------

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

  KaxContentEncodings *kcencodings = FINDFIRST(&ktentry, KaxContentEncodings);
  if (NULL == kcencodings)
    return true;

  int tid = kt_get_number(ktentry);

  size_t kcenc_idx;
  for (kcenc_idx = 0; kcenc_idx < kcencodings->ListSize(); kcenc_idx++) {
    KaxContentEncoding *kcenc = dynamic_cast<KaxContentEncoding *>((*kcencodings)[kcenc_idx]);
    if (NULL == kcenc)
      continue;

    kax_content_encoding_t enc;
    memset(&enc, 0, sizeof(kax_content_encoding_t));

    KaxContentEncodingOrder *ce_order = FINDFIRST(kcenc, KaxContentEncodingOrder);
    if (NULL != ce_order)
      enc.order = uint32(*ce_order);

    KaxContentEncodingType *ce_type = FINDFIRST(kcenc, KaxContentEncodingType);
    if (NULL != ce_type)
      enc.type = uint32(*ce_type);

    KaxContentEncodingScope *ce_scope = FINDFIRST(kcenc, KaxContentEncodingScope);
    enc.scope = NULL != ce_scope ? uint32(*ce_scope) : 1;

    KaxContentCompression *ce_comp = FINDFIRST(kcenc, KaxContentCompression);
    if (NULL != ce_comp) {
      KaxContentCompAlgo *cc_algo = FINDFIRST(ce_comp, KaxContentCompAlgo);
      if (NULL != cc_algo)
        enc.comp_algo = uint32(*cc_algo);

      KaxContentCompSettings *cc_settings = FINDFIRST(ce_comp, KaxContentCompSettings);
      if (NULL != cc_settings)
        enc.comp_settings = EBMLBIN_TO_COUNTEDMEM(cc_settings);
    }

    KaxContentEncryption *ce_enc = FINDFIRST(kcenc, KaxContentEncryption);
    if (NULL != ce_enc) {
      KaxContentEncAlgo *ce_ealgo = FINDFIRST(ce_enc, KaxContentEncAlgo);
      if (NULL != ce_ealgo)
        enc.enc_algo = uint32(*ce_ealgo);

      KaxContentEncKeyID *ce_ekeyid = FINDFIRST(ce_enc, KaxContentEncKeyID);
      if (NULL != ce_ekeyid)
        enc.enc_keyid = EBMLBIN_TO_COUNTEDMEM(ce_ekeyid);

      KaxContentSigAlgo *ce_salgo = FINDFIRST(ce_enc, KaxContentSigAlgo);
      if (NULL != ce_salgo)
        enc.enc_algo = uint32(*ce_salgo);

      KaxContentSigHashAlgo *ce_shalgo = FINDFIRST(ce_enc, KaxContentSigHashAlgo);
      if (NULL != ce_shalgo)
        enc.enc_algo = uint32(*ce_shalgo);

      KaxContentSigKeyID *ce_skeyid = FINDFIRST(ce_enc, KaxContentSigKeyID);
      if (NULL != ce_skeyid)
        enc.sig_keyid = EBMLBIN_TO_COUNTEDMEM(ce_skeyid);

      KaxContentSignature *ce_signature = FINDFIRST(ce_enc, KaxContentSignature);
      if (NULL != ce_signature)
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
      enc.compressor = counted_ptr<compressor_c>(new zlib_compressor_c());

    else if (1 == enc.comp_algo) {
#if !defined(HAVE_BZLIB_H)
      mxwarn(boost::format(Y("Track %1% was compressed with bzlib but mkvmerge has not been compiled with support for bzlib compression.\n")) % tid);
      ok = false;
      break;
#else
      enc.compressor = counted_ptr<compressor_c>(new bzlib_compressor_c());
#endif
    } else if (enc.comp_algo == 2) {
#if !defined(HAVE_LZO1X_H)
      mxwarn(boost::format(Y("Track %1% was compressed with lzo1x but mkvmerge has not been compiled with support for lzo1x compression.\n")) % tid);
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
      ce.compressor->decompress(memory);
}

std::string
content_decoder_c::descriptive_algorithm_list() {
  std::string list;
  std::vector<std::string> algorithms;

  boost::for_each(encodings, [&](const kax_content_encoding_t &enc) { algorithms.push_back(to_string(enc.comp_algo)); });

  return join(",", algorithms);
}
