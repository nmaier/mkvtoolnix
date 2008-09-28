/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   XML chapter writer functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <expat.h>
#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "chapters.h"
#include "commonebml.h"
#include "mm_io.h"
#include "xml_element_writer.h"

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
            const string &name) {
  int i;

  for (i = 0; i < chapter_start_times.size(); i++) {
    chapter_entry_c &e = chapter_start_times[i];
    if (e.level == level) {
      chapter_entries.push_back(chapter_entry_c(name, e.start, level));
      chapter_start_times.erase(chapter_start_times.begin() + i);
      return;
    }
  }

  chapter_names.push_back(chapter_entry_c(name, 0, level));
}

static void
handle_start_time(int level,
                  int64_t start_time) {
  int i;

  for (i = 0; i < chapter_names.size(); i++) {
    chapter_entry_c &e = chapter_names[i];
    if (e.level == level) {
      chapter_entries.push_back(chapter_entry_c(e.name, start_time, level));
      chapter_names.erase(chapter_names.begin() + i);
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
  string s;

  for (i = 0; i < display->ListSize(); i++) {
    e = (*display)[i];
    if (is_id(e, KaxChapterString)) {
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      handle_name(level - 1, s);

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
    out->puts(boost::format("CHAPTER%|1$02d|=%|2$02d|:%|3$02d|:%|4$02d|.%|5$03d|\n")
              % chapter_num % (v / 1000 / 60 / 60) % ((v / 1000 / 60) % 60) % ((v / 1000) % 60) % (v % 1000));
    out->puts(boost::format("CHAPTER%|1$02d|NAME=%2%\n") % chapter_num % chapter_entries[i].name);
    chapter_num++;
  }

  chapter_start_times.clear();
  chapter_names.clear();
  chapter_entries.clear();
}

// }}}

// {{{ XML chapter output

static void
pt(xml_writer_cb_t *cb,
   const char *tag) {
  int i;

  for (i = 0; i < cb->level; i++)
    cb->out->puts("  ");
  cb->out->puts(tag);
}

static int
cet_index(const char *name) {
  int i;

  for (i = 0; chapter_elements[i].name != NULL; i++)
    if (!strcmp(name, chapter_elements[i].name))
      return i;

  mxerror(boost::format(Y("cet_index: '%1%' not found\n")) % name);
  return -1;
}

static void
end_write_chapter_atom(void *data) {
  KaxChapterAtom *atom;
  xml_writer_cb_t *cb;

  cb = (xml_writer_cb_t *)data;
  atom = dynamic_cast<KaxChapterAtom *>(cb->e);
  assert(atom != NULL);
  if (FINDFIRST(atom, KaxChapterTimeStart) == NULL)
    pt(cb, "<ChapterTimeStart>00:00:00.000</ChapterTimeStart>\n");
}

static void
end_write_chapter_display(void *data) {
  KaxChapterDisplay *display;
  xml_writer_cb_t *cb;

  cb = (xml_writer_cb_t *)data;
  display = dynamic_cast<KaxChapterDisplay *>(cb->e);
  assert(display != NULL);
  if (FINDFIRST(display, KaxChapterString) == NULL)
    pt(cb, "<ChapterString></ChapterString>\n");
  if (FINDFIRST(display, KaxChapterLanguage) == NULL)
    pt(cb, "<ChapterLanguage>eng</ChapterLanguage>\n");
}

void
write_chapters_xml(KaxChapters *chapters,
                   mm_io_c *out) {
  int i;

  for (i = 0; chapter_elements[i].name != NULL; i++) {
    chapter_elements[i].start_hook = NULL;
    chapter_elements[i].end_hook = NULL;
  }
  chapter_elements[cet_index("ChapterAtom")].end_hook =
    end_write_chapter_atom;
  chapter_elements[cet_index("ChapterDisplay")].end_hook =
    end_write_chapter_display;

  for (i = 0; i < chapters->ListSize(); i++)
    write_xml_element_rec(1, 0, (*chapters)[i], out, chapter_elements);
}

// }}}
