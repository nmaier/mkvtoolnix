/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapters.h

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

#include "common.h"
#include "mm_io.h"

using namespace std;
using namespace libmatroska;

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
                          !isspace(*(s + 14)))

static void chapter_error(const char *fmt, ...) {
  va_list ap;

  mxprint(stderr, "Error parsing chapters: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  mxprint(stderr, "\n");
  exit(1);
}

static bool probe_simple_chapters(mm_text_io_c *in) {
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

static KaxChapters *parse_simple_chapters(mm_text_io_c *in, int64_t min_tc,
                                          int64_t max_tc, int64_t offset) {
  KaxChapters *chaps;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int64_t start, hour, minute, second, msecs;
  string name, line;
  int mode;

  in->setFilePointer(0);
  chaps = new KaxChapters;

  mode = 0;
  atom = NULL;
  edition = NULL;

  printf("off: %lld\n", offset);

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
        *static_cast<EbmlUnicodeString *>
          (&GetChild<KaxChapterString>(*display)) =
          cstr_to_UTFstring(name.c_str());
        *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
          "eng";
      }
    }
  }

  delete in;

  return chaps;
}

static bool probe_xml_chapters(mm_text_io_c *) {
  return false;
}

static KaxChapters *parse_xml_chapters(mm_text_io_c *, int64_t, int64_t,
                                       int64_t) {
  return NULL;
}

KaxChapters *parse_chapters(const char *file_name, int64_t min_tc,
                            int64_t max_tc, int64_t offset) {
  mm_text_io_c *in;

  try {
    in = new mm_text_io_c(file_name);
  } catch (...) {
    mxprint(stderr, "Error: Could not open '%s' for reading.\n", file_name);
    exit(1);
  }

  if (probe_simple_chapters(in))
    return parse_simple_chapters(in, min_tc, max_tc, offset);

  if (probe_xml_chapters(in))
    return parse_xml_chapters(in, min_tc, max_tc, offset);

  mxprint(stderr, "Error: Unknown file format for '%s'. It does not contain "
          "a supported chapter format.\n", file_name);
  delete in;
  exit(1);

  return NULL;
}

#define is_id(ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)
#define is_id2(e, ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)

static FILE *o;

static void pt(int level, const char *tag) {
  int i;

  for (i = 0; i < level; i++)
    mxprint(o, "  ");
  mxprint(o, "%s", tag);
}

static void write_chapter_atom_xml(KaxChapterAtom *atom, int level);

static void write_chapter_display_xml(KaxChapterDisplay *display, int level) {
  int i;
  EbmlElement *e;
  char *s;

  pt(level, "<Display>\n");

  for (i = 0; i < display->ListSize(); i++) {
    e = (*display)[i];
    if (is_id(KaxChapterString)) {
      pt(level + 1, "<String>");
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      mxprint(o, "%s</String>\n", s);
      safefree(s);

    } else if (is_id(KaxChapterLanguage)) {
      pt(level + 1, "<Langauge>");
      mxprint(o, "%s</Language>\n", string(*static_cast
                                           <EbmlString *>(e)).c_str());


    } else if (is_id(KaxChapterCountry)) {
      pt(level + 1, "<Country>");
      mxprint(o, "%s</Country>\n", string(*static_cast
                                          <EbmlString *>(e)).c_str());


    } else if (is_id(KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  pt(level, "</Display>\n");
}

static void write_chapter_track_xml(KaxChapterTrack *track, int level) {
  int i;
  EbmlElement *e;

  pt(level, "<Track>\n");

  for (i = 0; i < track->ListSize(); i++) {
    e = (*track)[i];
    if (is_id(KaxChapterTrackNumber)) {
      pt(level + 1, "<TrackNumber>");
      mxprint(o, "%u</TrackNumber\n", uint32(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  pt(level, "</Track>\n");
}

static void write_chapter_atom_xml(KaxChapterAtom *atom, int level) {
  int i;
  EbmlElement *e;
  uint64_t v;

  pt(level, "<Atom>\n");

  for (i = 0; i < atom->ListSize(); i++) {
    e = (*atom)[i];
    if (is_id(KaxChapterUID)) {
      pt(level + 1, "<UID>");
      mxprint(o, "%u</UID\n", uint32(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(KaxChapterTimeStart)) {
      pt(level + 1, "<TimeStart>");
      v = uint64(*static_cast<EbmlUInteger *>(e)) / 1000000;
      mxprint(o, "%02llu:%02llu:%02llu.%03llu</TimeStart>\n",
              v / 1000 / 60 / 60, (v / 1000 / 60) % 60, (v / 1000) % 60,
              v % 1000);

    } else if (is_id(KaxChapterTimeEnd)) {
      pt(level + 1, "<TimeEnd>");
      v = uint64(*static_cast<EbmlUInteger *>(e)) / 1000000;
      mxprint(o, "%02llu:%02llu:%02llu.%03llu</TimeEnd>\n",
              v / 1000 / 60 / 60, (v / 1000 / 60) % 60, (v / 1000) % 60,
              v % 1000);

    } else if (is_id(KaxChapterTrack))
      write_chapter_track_xml((KaxChapterTrack *)e, level + 1);

    else if (is_id(KaxChapterDisplay))
      write_chapter_display_xml((KaxChapterDisplay *)e, level + 1);

    else if (is_id(KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  pt(level, "</Atom>\n");
}

void write_chapters_xml(KaxChapters *chapters, FILE *out) {
  int i, j;
  KaxEditionEntry *edition;

  o = out;

  for (i = 0; i < chapters->ListSize(); i++) {
    if (is_id2((*chapters)[i], KaxEditionEntry)) {
      mxprint(out, "  <EditionEntry>\n");
      edition = (KaxEditionEntry *)(*chapters)[i];
      for (j = 0; j < edition->ListSize(); j++)
        if (is_id2((*edition)[j], KaxChapterAtom))
          write_chapter_atom_xml((KaxChapterAtom *)(*edition)[j], 2);
      mxprint(out, "  </EditionEntry>\n");
    }
  }
}

class chapter_entry_c {
public:
  string name;
  int64_t start, level;

  chapter_entry_c(string n, int64_t s, int64_t l);

  bool operator < (const chapter_entry_c &cmp) const;
};

chapter_entry_c::chapter_entry_c(string n, int64_t s, int64_t l) {
  name = n;
  start = s;
  level = l;
}

bool chapter_entry_c::operator <(const chapter_entry_c &cmp) const {
  return start < cmp.start;
}

static vector<chapter_entry_c> chapter_start_times, chapter_names,
  chapter_entries;

static void handle_name(int level, const char *name) {
  int i, j;
  vector<chapter_entry_c>::iterator itr;

  for (i = 0; i < chapter_start_times.size(); i++) {
    chapter_entry_c &e = chapter_start_times[i];
    if (e.level == level) {
      chapter_entries.push_back(chapter_entry_c(string(name), e.start, level));
      itr = chapter_start_times.begin();
      for (j = 0; j < i; j++)
        itr++;
      chapter_start_times.erase(itr);
      return;
    }
  }

  chapter_names.push_back(chapter_entry_c(name, 0, level));
}

static void handle_start_time(int level, int64_t start_time) {
  int i, j;
  vector<chapter_entry_c>::iterator itr;

  for (i = 0; i < chapter_names.size(); i++) {
    chapter_entry_c &e = chapter_names[i];
    if (e.level == level) {
      chapter_entries.push_back(chapter_entry_c(e.name, start_time, level));
      itr = chapter_names.begin();
      for (j = 0; j < i; j++)
        itr++;
      chapter_names.erase(itr);
      return;
    }
  }

  chapter_start_times.push_back(chapter_entry_c("", start_time, level));
}

static void write_chapter_atom_simple(KaxChapterAtom *atom, int level);

static void write_chapter_display_simple(KaxChapterDisplay *display,
                                         int level) {
  int i;
  EbmlElement *e;
  char *s;

  for (i = 0; i < display->ListSize(); i++) {
    e = (*display)[i];
    if (is_id(KaxChapterString)) {
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      handle_name(level - 1, s);
      safefree(s);

    } else if (is_id(KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

static void write_chapter_track_simple(KaxChapterTrack *track, int level) {
  int i;
  EbmlElement *e;

  for (i = 0; i < track->ListSize(); i++) {
    e = (*track)[i];

    if (is_id(KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

static void write_chapter_atom_simple(KaxChapterAtom *atom, int level) {
  int i;
  EbmlElement *e;
  uint64_t v;

  for (i = 0; i < atom->ListSize(); i++) {
    e = (*atom)[i];

    if (is_id(KaxChapterTimeStart)) {
      v = uint64(*static_cast<EbmlUInteger *>(e)) / 1000000;
      handle_start_time(level, v);

    } else if (is_id(KaxChapterTrack))
      write_chapter_track_simple((KaxChapterTrack *)e, level + 1);

    else if (is_id(KaxChapterDisplay))
      write_chapter_display_simple((KaxChapterDisplay *)e, level + 1);

    else if (is_id(KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

void write_chapters_simple(int &chapter_num, KaxChapters *chapters,
                           FILE *out) {
  int i, j;
  int64_t v;
  KaxEditionEntry *edition;

  chapter_start_times.clear();
  chapter_names.clear();
  chapter_entries.clear();

  for (i = 0; i < chapters->ListSize(); i++) {
    if (is_id2((*chapters)[i], KaxEditionEntry)) {
      edition = (KaxEditionEntry *)(*chapters)[i];
      for (j = 0; j < edition->ListSize(); j++)
        if (is_id2((*edition)[j], KaxChapterAtom))
          write_chapter_atom_simple((KaxChapterAtom *)(*edition)[j], 2);
    }
  }

  for (i = 0; i < chapter_entries.size(); i++) {
    v = chapter_entries[i].start;
    mxprint(out, "CHAPTER%02d=%02lld:%02lld:%02lld.%03lld\n", chapter_num,
            (v / 1000 / 60 / 60), (v / 1000 / 60) % 60,
            (v / 1000) % 60, v % 1000);
    mxprint(out, "CHAPTER%02dNAME=%s\n", chapter_num,
            chapter_entries[i].name.c_str());
    chapter_num++;
  }

  chapter_start_times.clear();
  chapter_names.clear();
  chapter_entries.clear();
}
