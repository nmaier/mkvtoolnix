/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions that need libebml/libmatroska

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
#endif
#if defined(SYS_WINDOWS)
#include <windows.h>
#endif

#include <ebml/EbmlFloat.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlVoid.h>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/memory.h"
#include "common/segmentinfo.h"
#include "common/segment_tracks.h"
#include "common/tags/tags.h"

using namespace libebml;

/*
   UTFstring <-> C string conversion
*/

static int
utf8_byte_length(unsigned char c) {
  if (c < 0x80)                 // 0xxxxxxx
    return 1;
  else if (c < 0xc0)            // 10xxxxxx
    return -1;
  else if (c < 0xe0)            // 110xxxxx
    return 2;
  else if (c < 0xf0)            // 1110xxxx
    return 3;
  else if (c < 0xf8)            // 11110xxx
    return 4;
  else if (c < 0xfc)            // 111110xx
    return 5;
  else if (c < 0xfe)            // 1111110x
    return 6;
  else
    return -1;
}

static int
wchar_to_utf8_byte_length(uint32_t w) {
  if (w < 0x00000080)
    return 1;
  else if (w < 0x00000800)
    return 2;
  else if (w < 0x00010000)
    return 3;
  else if (w < 0x00200000)
    return 4;
  else if (w < 0x04000000)
    return 5;
  else if (w < 0x80000000)
    return 6;
  else
    mxerror(Y("UTFstring_to_cstrutf8: Invalid wide character. Please contact moritz@bunkus.org if you think that this is not true."));

  return 0;
}

UTFstring
cstrutf8_to_UTFstring(const std::string &c) {
  wchar_t *new_string;
  int slen, dlen, src, dst, clen;
  UTFstring u;

  slen = c.length();
  dlen = 0;
  for (src = 0; src < slen; dlen++) {
    clen = utf8_byte_length(c[src]);
    if (clen < 0)
      break;
    src += clen;
  }

  new_string = (wchar_t *)safemalloc((dlen + 1) * sizeof(wchar_t));
  for (src = 0, dst = 0; src < slen; dst++) {
    clen = utf8_byte_length(c[src]);
    if ((clen < 0) || ((src + clen) > slen))
      break;

    if (clen == 1)
      new_string[dst] = c[src];

    else if (clen == 2)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x1f) << 6) |
        (((uint32_t)c[src + 1]) & 0x3f);
    else if (clen == 3)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x0f) << 12) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 6) |
        (((uint32_t)c[src + 2]) & 0x3f);
    else if (clen == 4)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 18) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 6) |
        (((uint32_t)c[src + 3]) & 0x3f);
    else if (clen == 5)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 24) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 18) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 3]) & 0x3f) << 6) |
        (((uint32_t)c[src + 4]) & 0x3f);
    else if (clen == 6)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 30) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 24) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 18) |
        ((((uint32_t)c[src + 3]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 4]) & 0x3f) << 6) |
        (((uint32_t)c[src + 5]) & 0x3f);

    src += clen;
  }
  new_string[dst] = 0;

  u = UTFstring(new_string);
  safefree(new_string);

  return u;
}

std::string
UTFstring_to_cstrutf8(const UTFstring &u) {
  int src, dst, dlen, slen, clen;
  unsigned char *new_string;
  uint32_t uc;
  std::string retval;

  dlen = 0;
  slen = u.length();

  for (src = 0, dlen = 0; src < slen; src++)
    dlen += wchar_to_utf8_byte_length((uint32_t)u[src]);

  new_string = (unsigned char *)safemalloc(dlen + 1);

  for (src = 0, dst = 0; src < slen; src++) {
    uc = (uint32_t)u[src];
    clen = wchar_to_utf8_byte_length(uc);

    if (clen == 1)
      new_string[dst] = (unsigned char)uc;

    else if (clen == 2) {
      new_string[dst]     = 0xc0 | ((uc >> 6) & 0x0000001f);
      new_string[dst + 1] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 3) {
      new_string[dst]     = 0xe0 | ((uc >> 12) & 0x0000000f);
      new_string[dst + 1] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 2] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 4) {
      new_string[dst]     = 0xf0 | ((uc >> 18) & 0x00000007);
      new_string[dst + 1] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 3] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 5) {
      new_string[dst]     = 0xf8 | ((uc >> 24) & 0x00000003);
      new_string[dst + 1] = 0x80 | ((uc >> 18) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 3] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 4] = 0x80 | (uc & 0x0000003f);

    } else {
      new_string[dst]     = 0xfc | ((uc >> 30) & 0x00000001);
      new_string[dst + 1] = 0x80 | ((uc >> 24) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 18) & 0x0000003f);
      new_string[dst + 3] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 4] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 5] = 0x80 | (uc & 0x0000003f);

    }

    dst += clen;
  }

  new_string[dst] = 0;
  retval = (char *)new_string;
  safefree(new_string);

  return retval;
}

bool
is_valid_utf8_string(const std::string &c) {
  int slen, dlen, src, clen;
  UTFstring u;

  slen = c.length();
  dlen = 0;
  for (src = 0; src < slen; dlen++) {
    clen = utf8_byte_length(c[src]);
    if ((clen < 0) || ((src + clen) > slen))
      return false;
    src += clen;
  }

  return true;
}

EbmlElement *
empty_ebml_master(EbmlElement *e) {
  EbmlMaster *m;

  m = dynamic_cast<EbmlMaster *>(e);
  if (!m)
    return e;

  while (m->ListSize() > 0) {
    delete (*m)[0];
    m->Remove(0);
  }

  return m;
}

EbmlElement *
create_ebml_element(const EbmlCallbacks &callbacks,
                    const EbmlId &id) {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(callbacks);
  size_t i;

//   if (id == EbmlId(*parent))
//     return empty_ebml_master(&parent->Generic().Create());

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (id == EBML_CTX_IDX_ID(context,i))
      return empty_ebml_master(&EBML_SEM_CREATE(EBML_CTX_IDX(context,i)));

  for (i = 0; i < EBML_CTX_SIZE(context); i++) {
    EbmlElement *e;

    if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context,i))))
      continue;

    e = create_ebml_element(EBML_CTX_IDX_INFO(context, i), id);
    if (e)
      return e;
  }

  return nullptr;
}

const EbmlCallbacks *
find_ebml_callbacks(const EbmlCallbacks &base,
                    const EbmlId &id) {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(base);
  const EbmlCallbacks *result;
  size_t i;

  if (EBML_INFO_ID(base) == id)
    return &base;

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (id == EBML_CTX_IDX_ID(context,i))
      return &EBML_CTX_IDX_INFO(context, i);

  for (i = 0; i < EBML_CTX_SIZE(context); i++) {
    if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context,i))))
      continue;
    result = find_ebml_callbacks(EBML_CTX_IDX_INFO(context, i), id);
    if (result)
      return result;
  }

  return nullptr;
}

const EbmlCallbacks *
find_ebml_callbacks(const EbmlCallbacks &base,
                    const char *debug_name) {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(base);
  const EbmlCallbacks *result;
  size_t i;

  if (!strcmp(debug_name, EBML_INFO_NAME(base)))
    return &base;

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (!strcmp(debug_name, EBML_INFO_NAME(EBML_CTX_IDX_INFO(context, i))))
      return &EBML_CTX_IDX_INFO(context, i);

  for (i = 0; i < EBML_CTX_SIZE(context); i++) {
    if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context,i))))
      continue;
    result = find_ebml_callbacks(EBML_CTX_IDX_INFO(context, i), debug_name);
    if (result)
      return result;
  }

  return nullptr;
}

const EbmlCallbacks *
find_ebml_parent_callbacks(const EbmlCallbacks &base,
                           const EbmlId &id) {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(base);
  const EbmlCallbacks *result;
  size_t i;

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (id == EBML_CTX_IDX_ID(context,i))
      return &base;

  for (i = 0; i < EBML_CTX_SIZE(context); i++) {
    if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context,i))))
      continue;
    result = find_ebml_parent_callbacks(EBML_CTX_IDX_INFO(context, i), id);
    if (result)
      return result;
  }

  return nullptr;
}

const EbmlSemantic *
find_ebml_semantic(const EbmlCallbacks &base,
                   const EbmlId &id) {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(base);
  const EbmlSemantic *result;
  size_t i;

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (id == EBML_CTX_IDX_ID(context,i))
      return &EBML_CTX_IDX(context,i);

  for (i = 0; i < EBML_CTX_SIZE(context); i++) {
    if (!(context != EBML_SEM_CONTEXT(EBML_CTX_IDX(context,i))))
      continue;
    result = find_ebml_semantic(EBML_CTX_IDX_INFO(context, i), id);
    if (result)
      return result;
  }

  return nullptr;
}

EbmlMaster *
sort_ebml_master(EbmlMaster *m) {
  if (!m)
    return m;

  int first_element = -1;
  int first_master  = -1;
  size_t i;
  for (i = 0; i < m->ListSize(); i++) {
    if (dynamic_cast<EbmlMaster *>((*m)[i]) && (-1 == first_master))
      first_master = i;
    else if (!dynamic_cast<EbmlMaster *>((*m)[i]) && (-1 != first_master) && (-1 == first_element))
      first_element = i;
    if ((first_master != -1) && (first_element != -1))
      break;
  }

  if (first_master == -1)
    return m;

  while (first_element != -1) {
    EbmlElement *e = (*m)[first_element];
    m->Remove(first_element);
    m->InsertElement(*e, first_master);
    first_master++;
    for (first_element++; first_element < static_cast<int>(m->ListSize()); first_element++)
      if (!dynamic_cast<EbmlMaster *>((*m)[first_element]))
        break;
    if (first_element >= static_cast<int>(m->ListSize()))
      first_element = -1;
  }

  for (i = 0; i < m->ListSize(); i++)
    if (dynamic_cast<EbmlMaster *>((*m)[i]))
      sort_ebml_master(dynamic_cast<EbmlMaster *>((*m)[i]));

  return m;
}

void
move_children(EbmlMaster &source,
              EbmlMaster &destination) {
  for (auto child : source)
    destination.PushElement(*child);
}

// ------------------------------------------------------------------------

int64_t
kt_get_default_duration(KaxTrackEntry &track) {
  return FindChildValue<KaxTrackDefaultDuration>(track);
}

int64_t
kt_get_number(KaxTrackEntry &track) {
  return FindChildValue<KaxTrackNumber>(track);
}

int64_t
kt_get_uid(KaxTrackEntry &track) {
  return FindChildValue<KaxTrackUID>(track);
}

std::string
kt_get_codec_id(KaxTrackEntry &track) {
  return FindChildValue<KaxCodecID>(track);
}

std::string
kt_get_language(KaxTrackEntry &track) {
  return FindChildValue<KaxTrackLanguage>(track);
}

int
kt_get_max_blockadd_id(KaxTrackEntry &track) {
  return FindChildValue<KaxMaxBlockAdditionID>(track);
}

int
kt_get_a_channels(KaxTrackEntry &track) {
  auto audio = FindChild<KaxTrackAudio>(track);
  return audio ? FindChildValue<KaxAudioChannels>(audio, 1u) : 1;
}

double
kt_get_a_sfreq(KaxTrackEntry &track) {
  auto audio = FindChild<KaxTrackAudio>(track);
  return audio ? FindChildValue<KaxAudioSamplingFreq>(audio, 8000.0) : 8000.0;
}

double
kt_get_a_osfreq(KaxTrackEntry &track) {
  auto audio = FindChild<KaxTrackAudio>(track);
  return audio ? FindChildValue<KaxAudioOutputSamplingFreq>(audio, 8000.0) : 8000.0;
}

int
kt_get_a_bps(KaxTrackEntry &track) {
  auto audio = FindChild<KaxTrackAudio>(track);
  return audio ? FindChildValue<KaxAudioBitDepth, int>(audio, -1) : -1;
}

int
kt_get_v_pixel_width(KaxTrackEntry &track) {
  auto video = FindChild<KaxTrackVideo>(track);
  return video ? FindChildValue<KaxVideoPixelWidth>(video) : 0;
}

int
kt_get_v_pixel_height(KaxTrackEntry &track) {
  auto video = FindChild<KaxTrackVideo>(track);
  return video ? FindChildValue<KaxVideoPixelHeight>(video) : 0;
}

EbmlElement *
find_ebml_element_by_id(EbmlMaster *master,
                        const EbmlId &id) {
  for (auto child : master->GetElementList())
    if (EbmlId(*child) == id)
      return child;

  return nullptr;
}

void
fix_mandatory_elements(EbmlElement *master) {
  if (dynamic_cast<KaxInfo *>(master))
    fix_mandatory_segmentinfo_elements(master);

  else if (dynamic_cast<KaxTracks *>(master))
    fix_mandatory_segment_tracks_elements(master);

  else if (dynamic_cast<KaxTags *>(master))
    fix_mandatory_tag_elements(master);

  else if (dynamic_cast<KaxChapters *>(master))
    fix_mandatory_chapter_elements(master);
}

void
remove_voids_from_master(EbmlElement *element) {
  auto master = dynamic_cast<EbmlMaster *>(element);
  if (master)
    DeleteChildren<EbmlVoid>(master);
}
