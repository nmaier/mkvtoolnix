/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapters.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief chapter parser/writer functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTags.h>

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

// PERFORMER "Blackmore's Night"
// TITLE "Fires At Midnight"
// FILE "Range.wav" WAVE
//   TRACK 01 AUDIO
//     TITLE "Written In The Stars"
//     PERFORMER "Blackmore's Night"
//     INDEX 01 00:00:00
//   TRACK 02 AUDIO
//     TITLE "The Times They Are A Changin'"
//     PERFORMER "Blackmore's Night"
//     INDEX 00 04:46:62
//     INDEX 01 04:49:64

bool
probe_cue_chapters(mm_text_io_c *in) {
  string s;

  in->setFilePointer(0);
  if (!in->getline2(s))
    return false;
  if (starts_with_case(s, "performer ") || starts_with_case(s, "title ") ||
      starts_with_case(s, "file ") || starts_with_case(s, "catalog ") ||
      starts_with_case(s, "rem date") || starts_with_case(s, "rem genre") ||
      starts_with_case(s, "rem discid"))
    return true;
  return false;
}

char *cue_to_chapter_name_format = NULL;

static void
cue_entries_to_chapter_name(string &performer,
                            string &title,
                            string &global_performer,
                            string &global_title,
                            string &name,
                            int num) {
  const char *this_char, *next_char;

  name = "";
  if (title.length() == 0)
    title = global_title;
  if (performer.length() == 0)
    performer = global_performer;

  if (cue_to_chapter_name_format == NULL)
    this_char = "%p - %t";
  else
    this_char = cue_to_chapter_name_format;
  next_char = this_char + 1;
  while (*this_char != 0) {
    if (*this_char == '%') {
      if (*next_char == 'p')
        name += performer;
      else if (*next_char == 't')
        name += title;
      else if (*next_char == 'n')
        name += to_string(num);
      else if (*next_char == 'N') {
        if (num < 10)
          name += '0';
        name += to_string(num);
      } else {
        name += *this_char;
        this_char--;
      }
      this_char++;
    } else
      name += *this_char;
    this_char++;
    next_char = this_char + 1;
  }
}

typedef struct {
  int num;
  int64_t start;
  int64_t end;
  int64_t min_tc;
  int64_t max_tc;
  int64_t offset;
  KaxChapters *chapters;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  bool do_convert;
  string performer;
  string title;
  string global_performer;
  string global_title;
  string name;
  string date;
  string genre;
  string disc_id;
  string isrc;
  const char *language;
  int line_num;
  int cc_utf8;
} cue_parser_args_t;

static UTFstring
cue_str_internal_to_utf(cue_parser_args_t &a,
                        const string &s) {
  if (a.do_convert) {
    UTFstring wchar_string;
    char *recoded_string;

    recoded_string = to_utf8(a.cc_utf8, s.c_str());
    wchar_string = cstrutf8_to_UTFstring(recoded_string);
    safefree(recoded_string);

    return wchar_string;
  } else
    return cstrutf8_to_UTFstring(s.c_str());
}

static KaxTagSimple *
create_simple_tag(cue_parser_args_t &a,
                  const string &name,
                  const string &value) {
  KaxTagSimple *simple;

  simple = new KaxTagSimple;
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxTagName>(*simple)) =
    cue_str_internal_to_utf(a, name);
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxTagString>(*simple)) =
    cue_str_internal_to_utf(a, value);

  return simple;
}

static bool
set_string2(string &dst,
            const string &s1,
            const string &s2) {
  if (s1.length() != 0) {
    dst = s1;
    return true;
  } else if (s2.length() != 0) {
    dst = s2;
    return true;
  } else
    return false;
}

static void
add_tag_for_cue_entry(cue_parser_args_t &a,
                      KaxTags **tags,
                      uint32_t cuid) {
  KaxTag *tag;
  KaxTagTargets *targets;
  string s;

  if (tags == NULL)
    return;

  if (*tags == NULL)
    *tags = new KaxTags;

  tag = new KaxTag;
  targets = &GetChild<KaxTagTargets>(*tag);
  *static_cast<EbmlUInteger *>(&GetChild<KaxTagChapterUID>(*targets)) = cuid;

  tag->PushElement(*create_simple_tag(a, "TITLE", a.title));
  tag->PushElement(*create_simple_tag(a, "TRACKNUMBER",
                                      mxsprintf("%d", a.num + 1)));
  if (set_string2(s, a.performer, a.global_performer))
    tag->PushElement(*create_simple_tag(a, "ARTIST", s));
  if (set_string2(s, a.title, a.global_title))
    tag->PushElement(*create_simple_tag(a, "ALBUM", s));
  if (a.date.length() > 0)
    tag->PushElement(*create_simple_tag(a, "DATE", a.date));
  if (a.genre.length() > 0)
    tag->PushElement(*create_simple_tag(a, "GENRE", a.genre));
  if (a.disc_id.length() > 0)
    tag->PushElement(*create_simple_tag(a, "DISCID", a.disc_id));
  if (a.isrc.length() > 0)
    tag->PushElement(*create_simple_tag(a, "ISRC", a.isrc));

  (*tags)->PushElement(*tag);
}

static void
add_elements_for_cue_entry(cue_parser_args_t &a,
                           KaxTags **tags) {
  KaxChapterDisplay *display;
  UTFstring wchar_string;
  uint32_t cuid;

  if (a.start == -1)
    mxerror("Cue sheet parser: No INDEX entry found for the previous "
            "TRACK entry (current line: %d)\n", a.line_num);
  if (!((a.start >= a.min_tc) && ((a.start <= a.max_tc) || (a.max_tc == -1))))
    return;

  if (a.edition == NULL)
    a.edition = &GetChild<KaxEditionEntry>(*a.chapters);
  if (a.atom == NULL)
    a.atom = &GetChild<KaxChapterAtom>(*a.edition);
  else
    a.atom = &GetNextChild<KaxChapterAtom>(*a.edition, *a.atom);

  cuid = create_unique_uint32();
  *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*a.atom)) = cuid;

  *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*a.atom)) =
    (a.start - a.offset) * 1000000;

  display = &GetChild<KaxChapterDisplay>(*a.atom);

  cue_entries_to_chapter_name(a.performer, a.title, a.global_performer,
                              a.global_title, a.name, a.num);
  *static_cast<EbmlUnicodeString *> (&GetChild<KaxChapterString>(*display)) =
    cue_str_internal_to_utf(a, a.name);

  *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
    a.language;

  add_tag_for_cue_entry(a, tags, cuid);
}

KaxChapters *
parse_cue_chapters(mm_text_io_c *in,
                   int64_t min_tc,
                   int64_t max_tc,
                   int64_t offset,
                   const char *language,
                   const char *charset,
                   bool exception_on_error,
                   KaxTags **tags) {
  int index, min, sec, csec;
  cue_parser_args_t a;
  string line;
  
  in->setFilePointer(0);
  a.chapters = new KaxChapters;

  if (in->get_byte_order() == BO_NONE) {
    a.do_convert = true;
    a.cc_utf8 = utf8_init(charset);

  } else {
    a.do_convert = false;
    a.cc_utf8 = 0;
  }

  if (language == NULL)
    a.language = "eng";
  else
    a.language = language;

  // The core now uses ns precision timecodes.
  if (min_tc > 0)
    min_tc /= 1000000;
  if (max_tc > 0)
    max_tc /= 1000000;
  if (offset > 0)
    offset /= 1000000;
  a.min_tc = min_tc;
  a.max_tc = max_tc;
  a.offset = offset;

  a.atom = NULL;
  a.edition = NULL;
  a.num = 0;
  a.line_num = 0;
  a.start = -1;
  try {
    while (in->getline2(line)) {
      a.line_num++;
      strip(line);
      if ((line.length() == 0) || starts_with_case(line, "file "))
        continue;

      if (starts_with_case(line, "performer ")) {
        line.erase(0, 10);
        strip(line);
        if (line.length() < 3)
          continue;
        if (line[0] == '"')
          line.erase(0, 1);
        if (line[line.length() - 1] == '"')
          line.erase(line.length() - 1);
        if (a.num == 0)
          a.global_performer = line;
        else
          a.performer = line;

      } else if (starts_with_case(line, "title ")) {
        line.erase(0, 6);
        strip(line);
        if (line.length() < 3)
          continue;
        if (line[0] == '"')
          line.erase(0, 1);
        if (line[line.length() - 1] == '"')
          line.erase(line.length() - 1);
        if (a.num == 0)
          a.global_title = line;
        else
          a.title = line;

      } else if (starts_with_case(line, "index ")) {
        line.erase(0, 6);
        strip(line);
        if (sscanf(line.c_str(), "%d %d:%d:%d", &index, &min, &sec, &csec) < 4)
          mxerror("Cue sheet parser: Invalid INDEX entry in line %d.\n",
                  a.line_num);
        if ((a.start == -1) || (index == 1))
          a.start = (min * 60 * 100 + sec * 100 + csec) * 10;

      } else if (starts_with_case(line, "track ")) {
        if ((line.length() < 5) || strcasecmp(&line[line.length() - 5],
                                              "audio"))
          continue;

        if (a.num >= 1)
          add_elements_for_cue_entry(a, tags);

        a.num++;

        a.start = -1;
        a.performer = "";
        a.title = "";
        a.isrc = "";

      } else if (starts_with_case(line, "rem date ")) {
        line.erase(0, 9);
        strip(line);
        if ((line.length() > 0) && (line[0] == '"'))
          line.erase(0, 1);
        if ((line.length() > 0) && (line[line.length() - 1] == '"'))
          line.erase(line.length() - 1);
        a.date = line;

      } else if (starts_with_case(line, "rem genre ")) {
        line.erase(0, 10);
        strip(line);
        if ((line.length() > 0) && (line[0] == '"'))
          line.erase(0, 1);
        if ((line.length() > 0) && (line[line.length() - 1] == '"'))
          line.erase(line.length() - 1);
        a.genre = line;

      } else if (starts_with_case(line, "rem discid ")) {
        line.erase(0, 11);
        strip(line);
        if ((line.length() > 0) && (line[0] == '"'))
          line.erase(0, 1);
        if ((line.length() > 0) && (line[line.length() - 1] == '"'))
          line.erase(line.length() - 1);
        a.disc_id = line;

      }
    }

    if (a.num >= 1)
      add_elements_for_cue_entry(a, tags);

  } catch(error_c e) {
    delete in;
    delete a.chapters;
    throw error_c(e);
  }

  delete in;

  if (a.num == 0) {
    delete a.chapters;
    return NULL;
  }

  return a.chapters;
}

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

#define is_id(c) (EbmlId(*e) == c::ClassInfos.GlobalId)

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
      if (is_id(KaxChapterUID)) {
        KaxChapterUID *esrc, *edst;
        edst = new KaxChapterUID;
        esrc = static_cast<KaxChapterUID *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterTimeStart)) {
        KaxChapterTimeStart *esrc, *edst;
        edst = new KaxChapterTimeStart;
        esrc = static_cast<KaxChapterTimeStart *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterTimeEnd)) {
        KaxChapterTimeEnd *esrc, *edst;
        edst = new KaxChapterTimeEnd;
        esrc = static_cast<KaxChapterTimeEnd *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterFlagHidden)) {
        KaxChapterFlagHidden *esrc, *edst;
        edst = new KaxChapterFlagHidden;
        esrc = static_cast<KaxChapterFlagHidden *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterFlagEnabled)) {
        KaxChapterFlagEnabled *esrc, *edst;
        edst = new KaxChapterFlagEnabled;
        esrc = static_cast<KaxChapterFlagEnabled *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterTrackNumber)) {
        KaxChapterTrackNumber *esrc, *edst;
        edst = new KaxChapterTrackNumber;
        esrc = static_cast<KaxChapterTrackNumber *>(e);
        *edst = *esrc;
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterString)) {
        KaxChapterString *esrc, *edst;
        edst = new KaxChapterString;
        esrc = static_cast<KaxChapterString *>(e);
        *static_cast<EbmlUnicodeString *>(edst) = UTFstring(*esrc);
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterLanguage)) {
        KaxChapterLanguage *esrc, *edst;
        edst = new KaxChapterLanguage;
        esrc = static_cast<KaxChapterLanguage *>(e);
        *static_cast<EbmlString *>(edst) = string(*esrc);
        dst->PushElement(*edst);

      } else if (is_id(KaxChapterCountry)) {
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
