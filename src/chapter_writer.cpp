/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapter_writer_xml.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id: chapters.cpp 920 2003-08-28 17:21:23Z mosu $
    \brief XML chapter writer functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <expat.h>
#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "common.h"
#include "mm_io.h"
#include "chapters.h"

using namespace std;
using namespace libmatroska;

// {{{ simple chapter output

#define is_id(ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)
#define is_id2(e, ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)

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

// }}}

// {{{ XML chapter output

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

// }}}
