/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions that need libebml/libmatroska

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

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

#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common.h"
#include "commonebml.h"

using namespace libebml;

/*
   UTFstring <-> C string conversion
*/

UTFstring
cstr_to_UTFstring(const string &c) {
  wchar_t *new_string;
  UTFstring u;
  int len;

  len = c.length();
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
#if defined(COMP_MSC) || defined(COMP_MINGW)
  MultiByteToWideChar(CP_ACP, 0, c.c_str(), -1, new_string, len + 1);
  u = new_string;
#else
  char *old_locale;

  memset(new_string, 0, (len + 1) * sizeof(wchar_t));
  new_string[len] = L'\0';
  old_locale = safestrdup(setlocale(LC_CTYPE, NULL));
  setlocale(LC_CTYPE, "");
  mbstowcs(new_string, c.c_str(), len);
  setlocale(LC_CTYPE, old_locale);
  safefree(old_locale);
  u = UTFstring(new_string);
#endif
  safefree(new_string);

  return u;
}

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
cstrutf8_to_UTFstring(const string &c) {
  wchar_t *new_string;
  int slen, dlen, src, dst, clen;
  UTFstring u;

  slen = c.length();
  dlen = 0;
  for (src = 0; src < slen; dlen++) {
    clen = utf8_byte_length(c[src]);
    if (clen < 0)
      mxerror(Y("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please contact moritz@bunkus.org and request that he implements a better UTF-8 parser."));
    src += clen;
  }

  new_string = (wchar_t *)safemalloc((dlen + 1) * sizeof(wchar_t));
  for (src = 0, dst = 0; src < slen; dst++) {
    clen = utf8_byte_length(c[src]);
    if ((src + clen) > slen)
      mxerror(Y("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please contact moritz@bunkus.org and request that he implements a better UTF-8 parser."));

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

string
UTFstring_to_cstr(const UTFstring &u) {
  string retval;
  char *new_string;
  int len;

#if defined(COMP_MSC) || defined(COMP_MINGW)
  BOOL dummy;

  len = u.length();
  new_string = (char *)safemalloc(len + 1);
  WideCharToMultiByte(CP_ACP, 0, u.c_str(), -1, new_string, len + 1, " ",
                      &dummy);
#else
  const wchar_t *sptr;
  char *old_locale;

  len = u.length();
  new_string = (char *)safemalloc(len * 4 + 1);
  memset(new_string, 0, len * 4 + 1);
  sptr = u.c_str();
  old_locale = safestrdup(setlocale(LC_CTYPE, NULL));
  setlocale(LC_CTYPE, "");
  wcstombs(new_string, sptr, len * 4 + 1);
  new_string[len * 4] = 0;
  setlocale(LC_CTYPE, old_locale);
  safefree(old_locale);
#endif
  retval = new_string;
  safefree(new_string);

  return retval;
}

string
UTFstring_to_cstrutf8(const UTFstring &u) {
  int src, dst, dlen, slen, clen;
  unsigned char *new_string;
  uint32_t uc;
  string retval;

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
is_valid_utf8_string(const string &c) {
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
  if (m == NULL)
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
  const EbmlSemanticContext &context = callbacks.Context;
  int i;

//   if (id == parent->Generic().GlobalId)
//     return empty_ebml_master(&parent->Generic().Create());

  for (i = 0; i < context.Size; i++)
    if (id == context.MyTable[i].GetCallbacks.GlobalId)
      return empty_ebml_master(&context.MyTable[i].GetCallbacks.Create());

  for (i = 0; i < context.Size; i++) {
    EbmlElement *e;

    if (!(context != context.MyTable[i].GetCallbacks.Context))
      continue;

    e = create_ebml_element(context.MyTable[i].GetCallbacks, id);
    if (e != NULL)
      return e;
  }

  return NULL;
}

const EbmlCallbacks *
find_ebml_callbacks(const EbmlCallbacks &base,
                    const EbmlId &id) {
  const EbmlSemanticContext &context = base.Context;
  const EbmlCallbacks *result;
  int i;

  if (base.GlobalId == id)
    return &base;

  for (i = 0; i < context.Size; i++)
    if (id == context.MyTable[i].GetCallbacks.GlobalId)
      return &context.MyTable[i].GetCallbacks;

  for (i = 0; i < context.Size; i++) {
    if (!(context != context.MyTable[i].GetCallbacks.Context))
      continue;
    result = find_ebml_callbacks(context.MyTable[i].GetCallbacks, id);
    if (NULL != result)
      return result;
  }

  return NULL;
}

const EbmlCallbacks *
find_ebml_callbacks(const EbmlCallbacks &base,
                    const char *debug_name) {
  const EbmlSemanticContext &context = base.Context;
  const EbmlCallbacks *result;
  int i;

  if (!strcmp(debug_name, base.DebugName))
    return &base;

  for (i = 0; i < context.Size; i++)
    if (!strcmp(debug_name, context.MyTable[i].GetCallbacks.DebugName))
      return &context.MyTable[i].GetCallbacks;

  for (i = 0; i < context.Size; i++) {
    if (!(context != context.MyTable[i].GetCallbacks.Context))
      continue;
    result = find_ebml_callbacks(context.MyTable[i].GetCallbacks, debug_name);
    if (NULL != result)
      return result;
  }

  return NULL;
}

const EbmlCallbacks *
find_ebml_parent_callbacks(const EbmlCallbacks &base,
                           const EbmlId &id) {
  const EbmlSemanticContext &context = base.Context;
  const EbmlCallbacks *result;
  int i;

  for (i = 0; i < context.Size; i++)
    if (id == context.MyTable[i].GetCallbacks.GlobalId)
      return &base;

  for (i = 0; i < context.Size; i++) {
    if (!(context != context.MyTable[i].GetCallbacks.Context))
      continue;
    result = find_ebml_parent_callbacks(context.MyTable[i].GetCallbacks, id);
    if (NULL != result)
      return result;
  }

  return NULL;
}

const EbmlSemantic *
find_ebml_semantic(const EbmlCallbacks &base,
                   const EbmlId &id) {
  const EbmlSemanticContext &context = base.Context;
  const EbmlSemantic *result;
  int i;

  for (i = 0; i < context.Size; i++)
    if (id == context.MyTable[i].GetCallbacks.GlobalId)
      return &context.MyTable[i];

  for (i = 0; i < context.Size; i++) {
    if (!(context != context.MyTable[i].GetCallbacks.Context))
      continue;
    result = find_ebml_semantic(context.MyTable[i].GetCallbacks, id);
    if (NULL != result)
      return result;
  }

  return NULL;
}

EbmlMaster *
sort_ebml_master(EbmlMaster *m) {
  int first_element, first_master, i;
  EbmlElement *e;

  if (m == NULL)
    return m;

  first_element = -1;
  first_master = -1;
  for (i = 0; i < m->ListSize(); i++) {
    if ((dynamic_cast<EbmlMaster *>((*m)[i]) != NULL) &&
        (first_master == -1))
      first_master = i;
    else if ((dynamic_cast<EbmlMaster *>((*m)[i]) == NULL) &&
             (first_master != -1) && (first_element == -1))
      first_element = i;
    if ((first_master != -1) && (first_element != -1))
      break;
  }

  if (first_master == -1)
    return m;

  while (first_element != -1) {
    e = (*m)[first_element];
    m->Remove(first_element);
    m->InsertElement(*e, first_master);
    first_master++;
    for (first_element++; first_element < m->ListSize(); first_element++)
      if (dynamic_cast<EbmlMaster *>((*m)[first_element]) == NULL)
        break;
    if (first_element >= m->ListSize())
      first_element = -1;
  }

  for (i = 0; i < m->ListSize(); i++)
    if (dynamic_cast<EbmlMaster *>((*m)[i]) != NULL)
      sort_ebml_master(dynamic_cast<EbmlMaster *>((*m)[i]));

  return m;
}

// ------------------------------------------------------------------------

int64_t
kt_get_default_duration(KaxTrackEntry &track) {
  KaxTrackDefaultDuration *default_duration;

  default_duration = FINDFIRST(&track, KaxTrackDefaultDuration);
  if (NULL == default_duration)
    return 0;
  return uint64(*default_duration);
}

int64_t
kt_get_number(KaxTrackEntry &track) {
  KaxTrackNumber *number;

  number = FINDFIRST(&track, KaxTrackNumber);
  if (NULL == number)
    return 0;
  return uint64(*number);
}

int64_t
kt_get_uid(KaxTrackEntry &track) {
  KaxTrackUID *uid;

  uid = FINDFIRST(&track, KaxTrackUID);
  if (NULL == uid)
    return 0;
  return uint64(*uid);
}

string
kt_get_codec_id(KaxTrackEntry &track) {
  KaxCodecID *codec_id;

  codec_id = FINDFIRST(&track, KaxCodecID);
  if (NULL == codec_id)
    return "";
  return string(*codec_id);
}

string
kt_get_language(KaxTrackEntry &track) {
  KaxTrackLanguage *language;

  language = FINDFIRST(&track, KaxTrackLanguage);
  if (NULL == language)
    return "";
  return string(*language);
}

int
kt_get_max_blockadd_id(KaxTrackEntry &track) {
  KaxMaxBlockAdditionID *max_blockadd_id;

  max_blockadd_id = FINDFIRST(&track, KaxMaxBlockAdditionID);
  if (NULL == max_blockadd_id)
    return 0;
  return uint32(*max_blockadd_id);
}

int
kt_get_a_channels(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioChannels *channels;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 1;

  channels = FINDFIRST(audio, KaxAudioChannels);
  if (NULL == channels)
    return 1;

  return uint32(*channels);
}

float
kt_get_a_sfreq(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioSamplingFreq *sfreq;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 8000.0;

  sfreq = FINDFIRST(audio, KaxAudioSamplingFreq);
  if (NULL == sfreq)
    return 8000.0;

  return float(*sfreq);
}

float
kt_get_a_osfreq(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioOutputSamplingFreq *osfreq;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 8000.0;

  osfreq = FINDFIRST(audio, KaxAudioOutputSamplingFreq);
  if (NULL == osfreq)
    return 8000.0;

  return float(*osfreq);
}

int
kt_get_a_bps(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioBitDepth *bps;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return -1;

  bps = FINDFIRST(audio, KaxAudioBitDepth);
  if (NULL == bps)
    return -1;

  return uint32(*bps);
}

int
kt_get_v_pixel_width(KaxTrackEntry &track) {
  KaxTrackVideo *video;
  KaxVideoPixelWidth *width;

  video = FINDFIRST(&track, KaxTrackVideo);
  if (NULL == video)
    return 0;

  width = FINDFIRST(video, KaxVideoPixelWidth);
  if (NULL == width)
    return 0;

  return uint32(*width);
}

int
kt_get_v_pixel_height(KaxTrackEntry &track) {
  KaxTrackVideo *video;
  KaxVideoPixelHeight *height;

  video = FINDFIRST(&track, KaxTrackVideo);
  if (NULL == video)
    return 0;

  height = FINDFIRST(video, KaxVideoPixelHeight);
  if (NULL == height)
    return 0;

  return uint32(*height);
}

