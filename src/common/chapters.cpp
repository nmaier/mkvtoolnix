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
            create_unique_uint32(UNIQUE_CHAPTER_IDS);

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
    in = new mm_text_io_c(new mm_file_io_c(file_name));
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

void
fix_mandatory_chapter_elements(EbmlElement *e) {
  if (e == NULL)
    return;

  if (dynamic_cast<KaxEditionEntry *>(e) != NULL) {
    KaxEditionEntry &ee = *static_cast<KaxEditionEntry *>(e);
    GetChild<KaxEditionFlagDefault>(ee);
    GetChild<KaxEditionFlagHidden>(ee);
    if (FINDFIRST(&ee, KaxEditionUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxEditionUID>(ee)) =
        create_unique_uint32(UNIQUE_EDITION_IDS);

  } else if (dynamic_cast<KaxChapterAtom *>(e) != NULL) {
    KaxChapterAtom &a = *static_cast<KaxChapterAtom *>(e);

    GetChild<KaxChapterFlagHidden>(a);
    GetChild<KaxChapterFlagEnabled>(a);
    if (FINDFIRST(&a, KaxChapterUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(a)) =
        create_unique_uint32(UNIQUE_CHAPTER_IDS);
    if (FINDFIRST(&a, KaxChapterTimeStart) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(a)) = 0;

  } else if (dynamic_cast<KaxChapterTrack *>(e) != NULL) {
    KaxChapterTrack &t = *static_cast<KaxChapterTrack *>(e);

    if (FINDFIRST(&t, KaxChapterTrackNumber) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTrackNumber>(t)) = 0;

  } else if (dynamic_cast<KaxChapterDisplay *>(e) != NULL) {
    KaxChapterDisplay &d = *static_cast<KaxChapterDisplay *>(e);

    if (FINDFIRST(&d, KaxChapterString) == NULL)
      *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(d)) = L"";
    if (FINDFIRST(&d, KaxChapterLanguage) == NULL)
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(d)) = "und";

  } else if (dynamic_cast<KaxChapterProcess *>(e) != NULL) {
    KaxChapterProcess &p = *static_cast<KaxChapterProcess *>(e);

    GetChild<KaxChapterProcessCodecID>(p);

  } else if (dynamic_cast<KaxChapterProcessCommand *>(e) != NULL) {
    KaxChapterProcessCommand &c = *static_cast<KaxChapterProcessCommand *>(e);

    GetChild<KaxChapterProcessTime>(c);
    GetChild<KaxChapterProcessData>(c);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_chapter_elements((*m)[i]);
  }
}

static void
remove_entries(int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               EbmlMaster &m) {
  int i;
  bool remove;
  KaxChapterAtom *atom;
  KaxChapterTimeStart *cts;
  KaxChapterTimeEnd *cte;
  EbmlMaster *m2;
  int64_t start_tc, end_tc;

  i = 0;
  while (i < m.ListSize()) {
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (atom != NULL) {
      cts = static_cast<KaxChapterTimeStart *>
        (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));

      remove = false;

      start_tc = uint64(*static_cast<EbmlUInteger *>(cts));
      if (start_tc < min_tc)
        remove = true;
      else if ((max_tc >= 0) && (start_tc > max_tc))
        remove = true;

      if (remove) {
        m.Remove(i);
        delete atom;
      } else
        i++;

    } else
      i++;
  }

  for (i = 0; i < m.ListSize(); i++) {
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (atom != NULL) {
      cts = static_cast<KaxChapterTimeStart *>
        (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));
      cte = static_cast<KaxChapterTimeEnd *>
        (atom->FindFirstElt(KaxChapterTimeEnd::ClassInfos, false));

      *static_cast<EbmlUInteger *>(cts) =
        uint64(*static_cast<EbmlUInteger *>(cts)) - offset;
      if (cte != NULL) {
        end_tc =  uint64(*static_cast<EbmlUInteger *>(cte));
        if ((max_tc >= 0) && (end_tc > max_tc))
          end_tc = max_tc;
        end_tc -= offset;
        *static_cast<EbmlUInteger *>(cte) = end_tc;
      }
    }

    m2 = dynamic_cast<EbmlMaster *>(m[i]);
    if (m2 != NULL)
      remove_entries(min_tc, max_tc, offset, *m2);
  }    
}

KaxChapters *
select_chapters_in_timeframe(KaxChapters *chapters,
                             int64_t min_tc,
                             int64_t max_tc,
                             int64_t offset) {
  uint32_t i, k, num_atoms;
  KaxEditionEntry *eentry;

  for (i = 0; i < chapters->ListSize(); i++) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL)
      continue;
    remove_entries(min_tc, max_tc, offset,
                   *static_cast<EbmlMaster *>((*chapters)[i]));
  }

  i = 0;
  while (i < chapters->ListSize()) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL) {
      i++;
      continue;
    }
    eentry = static_cast<KaxEditionEntry *>((*chapters)[i]);
    num_atoms = 0;
    for (k = 0; k < eentry->ListSize(); k++)
      if (dynamic_cast<KaxChapterAtom *>((*eentry)[k]) != NULL)
        num_atoms++;

    if (num_atoms == 0) {
      chapters->Remove(i);
      delete eentry;

    } else
      i++;
  }

  if (chapters->ListSize() == 0) {
    delete chapters;
    chapters = NULL;
  }

  return chapters;
}

KaxEditionEntry *
find_edition_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxEditionUID *euid;
  int eentry_idx;

  if (uid == 0)
    return FINDFIRST(&chapters, KaxEditionEntry);

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    euid = FINDFIRST(eentry, KaxEditionUID);
    if ((euid != NULL) && (uint64(*euid) == uid))
      return eentry;
  }

  return NULL;
}

KaxChapterAtom *
find_chapter_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxChapterAtom *atom;
  KaxChapterUID *cuid;
  int eentry_idx, atom_idx;

  if (uid == 0) {
    eentry = FINDFIRST(&chapters, KaxEditionEntry);
    if (eentry == NULL)
      return NULL;
    return FINDFIRST(eentry, KaxChapterAtom);
  }

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    for (atom_idx = 0; atom_idx < eentry->ListSize(); atom_idx++) {
      atom = dynamic_cast<KaxChapterAtom *>((*eentry)[atom_idx]);
      if (atom == NULL)
        continue;
      cuid = FINDFIRST(atom, KaxChapterUID);
      if ((cuid != NULL) && (uint64(*cuid) == uid))
        return atom;
    }
  }

  return NULL;
}
