/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  commonebml.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief helper functions that need libebml/libmatroska
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "common.h"
#include "commonebml.h"

using namespace libebml;

#if defined(DEBUG)
void __debug_debug_dump_elements(EbmlElement *e, int level) {
  int i;
  EbmlMaster *m;

  for (i = 0; i < level; i++)
    mxprint(stdout, " ");
  mxprint(stdout, "%s", e->Generic().DebugName);

  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    mxprint(stdout, " (size: %u)\n", m->ListSize());
    for (i = 0; i < m->ListSize(); i++)
      debug_dump_elements((*m)[i], level + 1);
  } else
    mxprint(stdout, "\n");
}
#endif

/*
 * UTFstring <-> C string conversion
 */

UTFstring cstr_to_UTFstring(const char *c) {
#if defined(COMP_MSC) || defined(COMP_MINGW)
  wchar_t *new_string;
  int len;
  UTFstring u;

  len = strlen(c);
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
  MultiByteToWideChar(CP_ACP, 0, c, -1, new_string, len + 1);

  u = new_string;
  safefree(new_string);

  return u;
#else
  wchar_t *new_string;
  char *old_locale;
  UTFstring u;
  int len;

  len = strlen(c);
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
  memset(new_string, 0, (len + 1) * sizeof(wchar_t));
  new_string[len] = L'\0';
  old_locale = safestrdup(setlocale(LC_CTYPE, NULL));
  setlocale(LC_CTYPE, "");
  mbstowcs(new_string, c, len);
  setlocale(LC_CTYPE, old_locale);
  safefree(old_locale);
  u = UTFstring(new_string);
  safefree(new_string);

  return u;
#endif
}

static int utf8_byte_length(unsigned char c) {
  if (c < 0x80)                 // 0xxxxxxx
    return 1;
  else if (c < 0xc0)            // 10xxxxxx
    die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
        "contact moritz@bunkus.org and request that he implements a better "
        "UTF-8 parser.");
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
    die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
        "contact moritz@bunkus.org and request that he implements a better "
        "UTF-8 parser.");

  return 0;
}

static int wchar_to_utf8_byte_length(uint32_t w) {
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
    die("UTFstring_to_cstrutf8: Invalid wide character. Please contact "
        "moritz@bunkus.org if you think that this is not true.");

  return 0;
}

UTFstring cstrutf8_to_UTFstring(const char *c) {
  wchar_t *new_string;
  int slen, dlen, src, dst, clen;
  UTFstring u;

  slen = strlen(c);
  dlen = 0;
  for (src = 0; src < slen; dlen++)
    src += utf8_byte_length(c[src]);

  new_string = (wchar_t *)safemalloc((dlen + 1) * sizeof(wchar_t));
  for (src = 0, dst = 0; src < slen; dst++) {
    clen = utf8_byte_length(c[src]);
    if ((src + clen) > slen)
      die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
          "contact moritz@bunkus.org and request that he implements a better "
          "UTF-8 parser.");

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

char *UTFstring_to_cstr(const UTFstring &u) {
#if defined(COMP_MSC) || defined(COMP_MINGW)
  char *new_string;
  int len;
  BOOL dummy;

  len = u.length();
  new_string = (char *)safemalloc(len + 1);
  WideCharToMultiByte(CP_ACP, 0, u.c_str(), -1, new_string, len + 1, " ",
                      &dummy);

  return new_string;
#else
  const wchar_t *sptr;
  char *new_string, *old_locale;
  int len;

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

  return new_string;
#endif
}

char *UTFstring_to_cstrutf8(const UTFstring &u) {
  int src, dst, dlen, slen, clen;
  unsigned char *new_string;
  uint32_t uc;

  dlen = 0;
  slen = u.length();

  for (src = 0, dlen = 0; src < slen; src++)
    dlen += wchar_to_utf8_byte_length((uint32_t)u[src]);

  new_string = (unsigned char *)malloc(dlen + 1);

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

  return (char *)new_string;
}
