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

static void chapter_error(const char *fmt, ...) {
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

bool probe_simple_chapters(mm_text_io_c *in) {
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

KaxChapters *parse_simple_chapters(mm_text_io_c *in, int64_t min_tc,
                                   int64_t max_tc, int64_t offset,
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

bool probe_cue_chapters(mm_text_io_c *in) {
  string s;

  in->setFilePointer(0);
  if (!in->getline2(s))
    return false;
  if (starts_with_case(s, "performer ") || starts_with_case(s, "title ") ||
      starts_with_case(s, "file "))
    return true;
  return false;
}

char *cue_to_chapter_name_format = NULL;
static void cue_entries_to_chapter_name(string &performer, string &title,
                                        string &global_performer,
                                        string &global_title, string &name,
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

KaxChapters *parse_cue_chapters(mm_text_io_c *in, int64_t min_tc,
                                int64_t max_tc, int64_t offset,
                                const char *language, const char *charset,
                                bool exception_on_error) {
  KaxChapters *chapters;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int line_num, num, index, min, sec, csec, cc_utf8;
  int64_t start;
  string global_title, global_performer, title, performer, line, name;
  UTFstring wchar_string;
  bool do_convert;
  char *recoded_string;
  
  in->setFilePointer(0);
  chapters = new KaxChapters;

  if (in->get_byte_order() == BO_NONE) {
    do_convert = true;
    cc_utf8 = utf8_init(charset);

  } else {
    do_convert = false;
    cc_utf8 = 0;
  }

  if (language == NULL)
    language = "eng";

  atom = NULL;
  edition = NULL;
  num = 0;
  line_num = 0;
  start = -1;
  try {
    while (in->getline2(line)) {
      line_num++;
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
        if (num == 0)
          global_performer = line;
        else
          performer = line;

      } else if (starts_with_case(line, "title ")) {
        line.erase(0, 6);
        strip(line);
        if (line.length() < 3)
          continue;
        if (line[0] == '"')
          line.erase(0, 1);
        if (line[line.length() - 1] == '"')
          line.erase(line.length() - 1);
        if (num == 0)
          global_title = line;
        else
          title = line;

      } else if (starts_with_case(line, "index ")) {
        line.erase(0, 6);
        strip(line);
        if (sscanf(line.c_str(), "%d %d:%d:%d", &index, &min, &sec, &csec) < 4)
          mxerror("Cue sheet parser: Invalid INDEX entry in line %d.\n",
                  line_num);
        if ((start == -1) || (index == 1))
          start = (min * 60 * 100 + sec * 100 + csec) * 10;

      } else if (starts_with_case(line, "track ")) {
        if ((line.length() < 5) || strcasecmp(&line[line.length() - 5],
                                              "audio"))
          continue;
        if (num >= 1) {
          if (start == -1)
            mxerror("Cue sheet parser: No INDEX entry found for the previous "
                    "TRACK entry (current line: %d)\n", line_num);
          if (!((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))))
            continue;

          if (edition == NULL)
            edition = &GetChild<KaxEditionEntry>(*chapters);
          if (atom == NULL)
            atom = &GetChild<KaxChapterAtom>(*edition);
          else
            atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
            create_unique_uint32();

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
            (start - offset) * 1000000;

          display = &GetChild<KaxChapterDisplay>(*atom);

          cue_entries_to_chapter_name(performer, title, global_performer,
                                      global_title, name, num);
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
        }
        num++;

        start = -1;
        performer = "";
        title = "";
      }
    }

    if (num >= 1) {
      if (start == -1)
        mxerror("Cue sheet parser: No INDEX entry found for the previous "
                "TRACK entry (current line: %d)\n", line_num);
      if ((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))) {
        if (edition == NULL)
          edition = &GetChild<KaxEditionEntry>(*chapters);
        if (atom == NULL)
          atom = &GetChild<KaxChapterAtom>(*edition);
        else
          atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

        *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
          create_unique_uint32();

        *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
          (start - offset) * 1000000;

        display = &GetChild<KaxChapterDisplay>(*atom);

        cue_entries_to_chapter_name(performer, title, global_performer,
                                    global_title, name, num);
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
      }
    }
  } catch(error_c e) {
    delete in;
    delete chapters;
    throw error_c(e);
  }

  delete in;

  if (num == 0) {
    delete chapters;
    return NULL;
  }

  return chapters;
}

KaxChapters *parse_chapters(const char *file_name, int64_t min_tc,
                            int64_t max_tc, int64_t offset,
                            const char *language, const char *charset,
                            bool exception_on_error, bool *is_simple_format) {
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
                                charset, exception_on_error);
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

static EbmlMaster *copy_chapters_recursive(EbmlMaster *src) {
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

static void set_chapter_mandatory_elements(EbmlMaster *m) {
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

KaxChapters *copy_chapters(KaxChapters *source) {
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
