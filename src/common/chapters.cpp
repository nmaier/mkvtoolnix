/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * chapter parser/writer functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "chapters.h"
#include "commonebml.h"
#include "error.h"
#include "mkvmerge.h"

using namespace std;
using namespace libmatroska;

string default_chapter_language;
string default_chapter_country;

// {{{ defines for chapter line recognition

#define isequal(s) (*(s) == '=')
#define iscolon(s) (*(s) == ':')
#define isdot(s) (*(s) == '.')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (istwodigits(s) && isdigit(*(s + 2)))
#define ischapter(s) (!strncmp("CHAPTER", (s), 7))
#define isname(s) (!strncmp("NAME", (s), 4))
#define ischapterline(s) ((strlen(s) == 22) && \
                          ischapter(s) && \
                          istwodigits(s + 7) && \
                          isequal(s + 9) && \
                          istwodigits(s + 10) && \
                          iscolon(s + 12) && \
                          istwodigits(s + 13) && \
                          iscolon(s + 15) && \
                          istwodigits(s + 16) && \
                          isdot(s + 18) && \
                          isthreedigits(s + 19))
#define ischapternameline(s) ((strlen(s) >= 15) && \
                          ischapter(s) && \
                          istwodigits(s + 7) && \
                          isname(s + 9) && \
                          isequal(s + 13) && \
                          !isblanktab(*(s + 14)))

// }}}
// {{{ helper functions

static void
chapter_error(const char *fmt,
              ...) {
  va_list ap;
  string new_fmt;
  char *new_error;
  int len;

  len = strlen("Error: Simple chapter parser: ");
  va_start(ap, fmt);
  fix_format(fmt, new_fmt);
  len += get_varg_len(new_fmt.c_str(), ap);
  new_error = (char *)safemalloc(len + 2);
  strcpy(new_error, "Error: Simple chapter parser: ");
  vsprintf(&new_error[strlen(new_error)], new_fmt.c_str(), ap);
  strcat(new_error, "\n");
  va_end(ap);
  throw error_c(new_error, true);
}

// }}}

// {{{ simple chapter parsing

bool
probe_simple_chapters(mm_text_io_c *in) {
  string line;

  in->setFilePointer(0);
  while (in->getline2(line)) {
    strip(line);
    if (line.length() == 0)
      continue;

    if (!ischapterline(line.c_str()))
      return false;

    while (in->getline2(line)) {
      strip(line);
      if (line.length() == 0)
        continue;

      if (!ischapternameline(line.c_str()))
        return false;

      return true;
    }

    return false;
  }

  return false;
}

//           1         2
// 012345678901234567890123
//
// CHAPTER01=00:00:00.000
// CHAPTER01NAME=Hallo Welt

KaxChapters *
parse_simple_chapters(mm_text_io_c *in,
                      int64_t min_tc,
                      int64_t max_tc,
                      int64_t offset,
                      const char *language,
                      const char *charset,
                      bool exception_on_error) {
  KaxChapters *chaps;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int64_t start, hour, minute, second, msecs;
  string name, line;
  int mode, num, cc_utf8;
  bool do_convert;
  char *recoded_string;
  UTFstring wchar_string;

  in->setFilePointer(0);
  chaps = new KaxChapters;

  mode = 0;
  atom = NULL;
  edition = NULL;
  num = 0;
  start = 0;
  cc_utf8 = 0;

  // The core now uses ns precision timecodes.
  if (min_tc > 0)
    min_tc /= 1000000;
  if (max_tc > 0)
    max_tc /= 1000000;
  if (offset > 0)
    offset /= 1000000;

  if (in->get_byte_order() == BO_NONE) {
    do_convert = true;
    cc_utf8 = utf8_init(charset);

  } else
    do_convert = false;

  if (language == NULL) {
    if (default_chapter_language.length() > 0)
      language = default_chapter_language.c_str();
    else
      language = "eng";
  }

  try {
    while (in->getline2(line)) {
      strip(line);
      if (line.length() == 0)
        continue;

      if (mode == 0) {
        if (!ischapterline(line.c_str()))
          chapter_error("'%s' is not a CHAPTERxx=... line.", line.c_str());
        parse_int(line.substr(10, 2).c_str(), hour);
        parse_int(line.substr(13, 2).c_str(), minute);
        parse_int(line.substr(16, 2).c_str(), second);
        parse_int(line.substr(19, 3).c_str(), msecs);
        if (hour > 23)
          chapter_error("Invalid hour: %d", hour);
        if (minute > 59)
          chapter_error("Invalid minute: %d", minute);
        if (second > 59)
          chapter_error("Invalid second: %d", second);
        start = msecs + second * 1000 + minute * 1000 * 60 +
          hour * 1000 * 60 * 60;
        mode = 1;

      } else {
        if (!ischapternameline(line.c_str()))
          chapter_error("'%s' is not a CHAPTERxxNAME=... line.", line.c_str());
        name = line.substr(14);
        mode = 0;

        if ((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))) {
          if (edition == NULL)
            edition = &GetChild<KaxEditionEntry>(*chaps);
          if (atom == NULL)
            atom = &GetChild<KaxChapterAtom>(*edition);
          else
            atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
            create_unique_uint32();

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
            (start - offset) * 1000000;

          display = &GetChild<KaxChapterDisplay>(*atom);

          if (do_convert) {
            recoded_string = to_utf8(cc_utf8, name.c_str());
            wchar_string = cstrutf8_to_UTFstring(recoded_string);
            safefree(recoded_string);
          } else
            wchar_string = cstrutf8_to_UTFstring(name.c_str());
          *static_cast<EbmlUnicodeString *>
            (&GetChild<KaxChapterString>(*display)) = wchar_string;

          *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
            language;

          if (default_chapter_country.length() > 0)
            *static_cast<EbmlString *>
              (&GetChild<KaxChapterCountry>(*display)) =
              default_chapter_country;

          num++;
        }
      }
    }
  } catch (error_c e) {
    delete in;
    delete chaps;
    throw error_c(e);
  }

  delete in;

  if (num == 0) {
    delete chaps;
    return NULL;
  }

  return chaps;
}

// }}}

KaxChapters *
parse_chapters(const char *file_name,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const char *language,
               const char *charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  mm_text_io_c *in;

  in = NULL;
  try {
    in = new mm_text_io_c(file_name);
  } catch (...) {
    if (exception_on_error)
      throw error_c(string("Could not open '") + string(file_name) +
                    string("' for reading.\n"));
    else
      mxerror("Could not open '%s' for reading.\n", file_name);
  }

  try {
    if (probe_simple_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_simple_chapters(in, min_tc, max_tc, offset, language,
                                   charset, exception_on_error);
    } else if (probe_cue_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_cue_chapters(in, min_tc, max_tc, offset, language,
                                charset, exception_on_error, tags);
    } else if (is_simple_format != NULL)
      *is_simple_format = false;

    if (probe_xml_chapters(in))
      return parse_xml_chapters(in, min_tc, max_tc, offset,
                                exception_on_error);

    delete in;

    throw error_c(string("Unknown file format for '") + string(file_name) +
                  string("'. It does not contain a supported chapter "
                         "format.\n"));
  } catch (error_c e) {
    if (exception_on_error)
      throw e;
    mxerror("%s", e.get_error());
  }

  return NULL;
}

static EbmlMaster *
copy_chapters_recursive(EbmlMaster *src) {
  uint32_t i;
  EbmlMaster *dst, *m;

  dst = static_cast<EbmlMaster *>(&src->Generic().Create());
  while (dst->ListSize() > 0) {
    EbmlElement *e;
    e = (*dst)[0];
    dst->Remove(0);
    delete e;
  }
  for (i = 0; i < src->ListSize(); i++) {
    EbmlElement *e;

    e = (*src)[i];
    if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL)
      dst->PushElement(*copy_chapters_recursive(m));
    else {
      if (is_id(e, KaxChapterUID)) {
        KaxChapterUID *esrc, *edst;
        edst = new KaxChapterUID;
        esrc = static_cast<KaxChapterUID *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterTimeStart)) {
        KaxChapterTimeStart *esrc, *edst;
        edst = new KaxChapterTimeStart;
        esrc = static_cast<KaxChapterTimeStart *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterTimeEnd)) {
        KaxChapterTimeEnd *esrc, *edst;
        edst = new KaxChapterTimeEnd;
        esrc = static_cast<KaxChapterTimeEnd *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterFlagHidden)) {
        KaxChapterFlagHidden *esrc, *edst;
        edst = new KaxChapterFlagHidden;
        esrc = static_cast<KaxChapterFlagHidden *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterFlagEnabled)) {
        KaxChapterFlagEnabled *esrc, *edst;
        edst = new KaxChapterFlagEnabled;
        esrc = static_cast<KaxChapterFlagEnabled *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterTrackNumber)) {
        KaxChapterTrackNumber *esrc, *edst;
        edst = new KaxChapterTrackNumber;
        esrc = static_cast<KaxChapterTrackNumber *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterString)) {
        KaxChapterString *esrc, *edst;
        edst = new KaxChapterString;
        esrc = static_cast<KaxChapterString *>(e);
        *static_cast<EbmlUnicodeString *>(edst) = UTFstring(*esrc);
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterLanguage)) {
        KaxChapterLanguage *esrc, *edst;
        edst = new KaxChapterLanguage;
        esrc = static_cast<KaxChapterLanguage *>(e);
        *static_cast<EbmlString *>(edst) = string(*esrc);
        dst->PushElement(*edst);

      } else if (is_id(e, KaxChapterCountry)) {
        KaxChapterCountry *esrc, *edst;
        edst = new KaxChapterCountry;
        esrc = static_cast<KaxChapterCountry *>(e);
        *static_cast<EbmlString *>(edst) = string(*esrc);
        dst->PushElement(*edst);

      }
    }
  }

  return dst;
}

static void
set_chapter_mandatory_elements(EbmlMaster *m) {
  uint32_t i;
  EbmlMaster *new_m;

  if (EbmlId(*m) == KaxChapterTrack::ClassInfos.GlobalId) {
    KaxChapterTrackNumber *tnumber;
    tnumber = &GetChild<KaxChapterTrackNumber>(*m);
    if (!tnumber->ValueIsSet())
      *static_cast<EbmlUInteger *>(tnumber) = 0;
  } else if (EbmlId(*m) == KaxChapterDisplay::ClassInfos.GlobalId) {
    KaxChapterLanguage *lang;
    GetChild<KaxChapterString>(*m);
    lang = &GetChild<KaxChapterLanguage>(*m);
    if (!lang->ValueIsSet())
      *static_cast<EbmlString *>(lang) = "eng";
  }

  for (i = 0; i < m->ListSize(); i++)
    if ((new_m = dynamic_cast<EbmlMaster *>((*m)[i])) != NULL)
      set_chapter_mandatory_elements(new_m);
}

KaxChapters *
copy_chapters(KaxChapters *source) {
  KaxChapters *dst;
  uint32_t ee;

  if (source == NULL)
    return NULL;

  dst = new KaxChapters;
  for (ee = 0; ee < source->ListSize(); ee++) {
    EbmlMaster *master;
    master = copy_chapters_recursive(static_cast<EbmlMaster *>((*source)[ee]));
    set_chapter_mandatory_elements(master);
    dst->PushElement(*master);
  }

  return dst;
}

// Some helper functions for easy access to libmatroska's chapter structure.

int64_t
get_chapter_start(KaxChapterAtom &atom) {
  KaxChapterTimeStart *start;

  start = FINDFIRST(&atom, KaxChapterTimeStart);
  if (start == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(start));
}

string
get_chapter_name(KaxChapterAtom &atom) {
  KaxChapterDisplay *display;
  KaxChapterString *name;

  display = FINDFIRST(&atom, KaxChapterDisplay);
  if (display == NULL)
    return "";
  name = FINDFIRST(display, KaxChapterString);
  if (name == NULL)
    return "";
  return UTFstring_to_cstrutf8(UTFstring(*name));
}

int64_t
get_chapter_uid(KaxChapterAtom &atom) {
  KaxChapterUID *uid;

  uid = FINDFIRST(&atom, KaxChapterUID);
  if (uid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(uid));
}
