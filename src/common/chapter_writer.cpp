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
 * XML chapter writer functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <expat.h>
#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "commonebml.h"
#include "mm_io.h"
#include "chapters.h"

using namespace std;
using namespace libmatroska;

// {{{ simple chapter output

class chapter_entry_c {
public:
  string name;
  int64_t start, level;

  chapter_entry_c(string n, int64_t s, int64_t l);

  bool operator < (const chapter_entry_c &cmp) const;
};

chapter_entry_c::chapter_entry_c(string n,
                                 int64_t s,
                                 int64_t l) {
  name = n;
  start = s;
  level = l;
}

bool
chapter_entry_c::operator <(const chapter_entry_c &cmp)
  const {
  return start < cmp.start;
}

static vector<chapter_entry_c> chapter_start_times, chapter_names;
static vector<chapter_entry_c> chapter_entries;

static void
handle_name(int level,
            const char *name) {
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

static void
handle_start_time(int level,
                  int64_t start_time) {
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

static void
write_chapter_display_simple(KaxChapterDisplay *display,
                             int level) {
  int i;
  EbmlElement *e;
  char *s;

  for (i = 0; i < display->ListSize(); i++) {
    e = (*display)[i];
    if (is_id(e, KaxChapterString)) {
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      handle_name(level - 1, s);
      safefree(s);

    } else if (is_id(e, KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

static void
write_chapter_track_simple(KaxChapterTrack *track,
                           int level) {
  int i;
  EbmlElement *e;

  for (i = 0; i < track->ListSize(); i++) {
    e = (*track)[i];

    if (is_id(e, KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

static void
write_chapter_atom_simple(KaxChapterAtom *atom,
                          int level) {
  int i;
  EbmlElement *e;
  uint64_t v;

  for (i = 0; i < atom->ListSize(); i++) {
    e = (*atom)[i];

    if (is_id(e, KaxChapterTimeStart)) {
      v = uint64(*static_cast<EbmlUInteger *>(e)) / 1000000;
      handle_start_time(level, v);

    } else if (is_id(e, KaxChapterTrack))
      write_chapter_track_simple((KaxChapterTrack *)e, level + 1);

    else if (is_id(e, KaxChapterDisplay))
      write_chapter_display_simple((KaxChapterDisplay *)e, level + 1);

    else if (is_id(e, KaxChapterAtom))
      write_chapter_atom_simple((KaxChapterAtom *)e, level + 1);

  }
}

void
write_chapters_simple(int &chapter_num,
                      KaxChapters *chapters,
                      mm_io_c *out) {
  int i, j;
  int64_t v;
  KaxEditionEntry *edition;

  chapter_start_times.clear();
  chapter_names.clear();
  chapter_entries.clear();

  for (i = 0; i < chapters->ListSize(); i++) {
    if (is_id((*chapters)[i], KaxEditionEntry)) {
      edition = (KaxEditionEntry *)(*chapters)[i];
      for (j = 0; j < edition->ListSize(); j++)
        if (is_id((*edition)[j], KaxChapterAtom))
          write_chapter_atom_simple((KaxChapterAtom *)(*edition)[j], 2);
    }
  }

  for (i = 0; i < chapter_entries.size(); i++) {
    v = chapter_entries[i].start;
    out->printf("CHAPTER%02d=%02lld:%02lld:%02lld.%03lld\n", chapter_num,
                (v / 1000 / 60 / 60), (v / 1000 / 60) % 60,
                (v / 1000) % 60, v % 1000);
    out->printf("CHAPTER%02dNAME=%s\n", chapter_num,
                chapter_entries[i].name.c_str());
    chapter_num++;
  }

  chapter_start_times.clear();
  chapter_names.clear();
  chapter_entries.clear();
}

// }}}

// {{{ XML chapter output

static mm_io_c *o;

static void
pt(int level,
   const char *tag) {
  int i;

  for (i = 0; i < level; i++)
    o->printf("  ");
  o->printf("%s", tag);
}

static void write_chapter_atom_xml(KaxChapterAtom *atom, int level);

static void
write_chapter_display_xml(KaxChapterDisplay *display,
                          int level) {
  int i;
  EbmlElement *e;
  char *s;
  bool string_found, language_found;
  string escaped;

  pt(level, "<ChapterDisplay>\n");

  language_found = false;
  string_found = false;
  for (i = 0; i < display->ListSize(); i++) {
    e = (*display)[i];
    if (is_id(e, KaxChapterString)) {
      pt(level + 1, "<ChapterString>");
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      escaped = escape_xml(s);
      o->printf("%s</ChapterString>\n", escaped.c_str());
      safefree(s);
      string_found = true;

    } else if (is_id(e, KaxChapterLanguage)) {
      pt(level + 1, "<ChapterLanguage>");
      o->printf("%s</ChapterLanguage>\n",
                string(*static_cast<EbmlString *>(e)).c_str());
      language_found = true;


    } else if (is_id(e, KaxChapterCountry)) {
      pt(level + 1, "<ChapterCountry>");
      o->printf("%s</ChapterCountry>\n",
                string(*static_cast<EbmlString *>(e)).c_str());


    } else if (is_id(e, KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  if (!string_found)
    pt(level + 1, "<ChapterString></ChapterString>\n");
  if (!language_found)
    pt(level + 1, "<ChapterLanguage>eng</ChapterLanguage>\n");

  pt(level, "</ChapterDisplay>\n");
}

static void
write_chapter_track_xml(KaxChapterTrack *track,
                        int level) {
  int i;
  EbmlElement *e;

  pt(level, "<ChapterTrack>\n");

  for (i = 0; i < track->ListSize(); i++) {
    e = (*track)[i];
    if (is_id(e, KaxChapterTrackNumber)) {
      pt(level + 1, "<ChapterTrackNumber>");
      o->printf("%u</ChapterTrackNumber\n",
                uint32(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(e, KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  pt(level, "</ChapterTrack>\n");
}

static void
write_chapter_atom_xml(KaxChapterAtom *atom,
                       int level) {
  int i;
  EbmlElement *e;
  uint64_t v;
  bool start_time_found;

  pt(level, "<ChapterAtom>\n");

  start_time_found = false;
  for (i = 0; i < atom->ListSize(); i++) {
    e = (*atom)[i];
    if (is_id(e, KaxChapterUID)) {
      pt(level + 1, "<ChapterUID>");
      o->printf("%u</ChapterUID>\n",
                uint32(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(e, KaxChapterTimeStart)) {
      pt(level + 1, "<ChapterTimeStart>");
      v = uint64(*static_cast<EbmlUInteger *>(e));
      o->printf(FMT_TIMECODEN "</ChapterTimeStart>\n",
                ARG_TIMECODEN(v));
      start_time_found = true;

    } else if (is_id(e, KaxChapterTimeEnd)) {
      pt(level + 1, "<ChapterTimeEnd>");
      v = uint64(*static_cast<EbmlUInteger *>(e));
      o->printf(FMT_TIMECODEN "</ChapterTimeEnd>\n",
                ARG_TIMECODEN(v));

    } else if (is_id(e, KaxChapterFlagHidden)) {
      pt(level + 1, "<ChapterFlagHidden>");
      o->printf("%u</ChapterFlagHidden>\n",
                uint8(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(e, KaxChapterFlagEnabled)) {
      pt(level + 1, "<ChapterFlagEnabled>");
      o->printf("%u</ChapterFlagEnabled>\n",
                uint8(*static_cast<EbmlUInteger *>(e)));

    } else if (is_id(e, KaxChapterTrack))
      write_chapter_track_xml((KaxChapterTrack *)e, level + 1);

    else if (is_id(e, KaxChapterDisplay))
      write_chapter_display_xml((KaxChapterDisplay *)e, level + 1);

    else if (is_id(e, KaxChapterAtom))
      write_chapter_atom_xml((KaxChapterAtom *)e, level + 1);

  }

  if (!start_time_found)
    pt(level + 1, "<ChapterTimeStart>00:00:00.000</ChapterTimeStart>\n");

  pt(level, "</ChapterAtom>\n");
}

void
write_chapters_xml(KaxChapters *chapters,
                   mm_io_c *out) {
  int i, j;
  KaxEditionEntry *edition;
  KaxEditionUID *edition_uid;
  EbmlElement *e;

  o = out;

  for (i = 0; i < chapters->ListSize(); i++) {
    if (is_id((*chapters)[i], KaxEditionEntry)) {
      out->printf("  <EditionEntry>\n");
      edition = (KaxEditionEntry *)(*chapters)[i];
      edition_uid = FINDFIRST(edition, KaxEditionUID);
      if (edition != NULL)
        out->printf("    <EditionUID>%llu</EditionUID>\n",
                    uint64(*static_cast<EbmlUInteger *>(edition_uid)));
      for (j = 0; j < edition->ListSize(); j++) {
        e = (*edition)[j];

        if (is_id(e, KaxChapterAtom))
          write_chapter_atom_xml(static_cast<KaxChapterAtom *>(e), 2);

        else if (is_id(e, KaxEditionFlagHidden)) {
          pt(2, "<EditionFlagHidden>");
          o->printf("%u</EditionFlagHidden>\n",
                    uint8(*static_cast<EbmlUInteger *>(e)));

        } else if (is_id(e, KaxEditionManaged)) {
          pt(2, "<EditionManaged>");
          o->printf("%u</EditionManaged>\n",
                    uint8(*static_cast<EbmlUInteger *>(e)));

        } else if (is_id(e, KaxEditionFlagDefault)) {
          pt(2, "<EditionFlagDefault>");
          o->printf("%u</EditionFlagDefault>\n",
                    uint8(*static_cast<EbmlUInteger *>(e)));

        }
      }
 
      out->printf("  </EditionEntry>\n");
    }
  }
}

// }}}
