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
#include "xml_element_mapping.h"

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
  int i, j;
  vector<chapter_entry_c>::iterator itr;

  for (i = 0; i < chapter_start_times.size(); i++) {
    chapter_entry_c &e = chapter_start_times[i];
    if (e.level == level) {
      chapter_entries.push_back(chapter_entry_c(name, e.start, level));
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

typedef struct {
  int level;
  int parent_idx;
  int elt_idx;
  EbmlElement *e;
  mm_io_c *out;
} chapter_writer_cb_t;

static void
pt(chapter_writer_cb_t *cb,
   const char *tag) {
  int i;

  for (i = 0; i < cb->level; i++)
    cb->out->printf("  ");
  cb->out->printf("%s", tag);
}

static void
write_xml_element_rec(int level,
                      int parent_idx,
                      EbmlElement *e,
                      mm_io_c *out) {
  EbmlMaster *m;
  int elt_idx, i;
  bool found;
  string s;

  elt_idx = parent_idx;
  found = false;
  while ((chapter_elements[elt_idx].name != NULL) &&
         (chapter_elements[elt_idx].level >=
          chapter_elements[parent_idx].level)) {
    if (chapter_elements[elt_idx].id == e->Generic().GlobalId) {
      found = true;
      break;
    }
    elt_idx++;
  }

  for (i = 0; i < level; i++)
    out->printf("  ");

  if (!found) {
    out->printf("<!-- Unknown element '%s' -->\n", e->Generic().DebugName);
    return;
  }

  out->printf("<%s>", chapter_elements[elt_idx].name);
  switch (chapter_elements[elt_idx].type) {
    case ebmlt_master:
      out->printf("\n");
      m = dynamic_cast<EbmlMaster *>(e);
      assert(m != NULL);
      for (i = 0; i < m->ListSize(); i++)
        write_xml_element_rec(level + 1, elt_idx, (*m)[i], out);

      if (chapter_elements[elt_idx].end_hook != NULL) {
        chapter_writer_cb_t cb;

        cb.level = level;
        cb.parent_idx = parent_idx;
        cb.elt_idx = elt_idx;
        cb.e = e;
        cb.out = out;

        chapter_elements[elt_idx].end_hook(&cb);
      }

      for (i = 0; i < level; i++)
        out->printf("  ");
      out->printf("</%s>\n", chapter_elements[elt_idx].name);
      break;

    case ebmlt_uint:
    case ebmlt_bool:
      out->printf("%llu</%s>\n", uint64(*dynamic_cast<EbmlUInteger *>(e)),
                  chapter_elements[elt_idx].name);
      break;

    case ebmlt_string:
      s = escape_xml(string(*dynamic_cast<EbmlString *>(e)));
      out->printf("%s</%s>\n", s.c_str(), chapter_elements[elt_idx].name);
      break;

    case ebmlt_ustring:
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      s = escape_xml(s);
      out->printf("%s</%s>\n", s.c_str(), chapter_elements[elt_idx].name);
      break;

    case ebmlt_time:
      out->printf(FMT_TIMECODEN "</%s>\n", 
                  ARG_TIMECODEN(uint64(*dynamic_cast<EbmlUInteger *>(e))),
                  chapter_elements[elt_idx].name);
      break;

    default:
      assert(false);
  }
}

static int
cet_index(const char *name) {
  int i;

  for (i = 0; chapter_elements[i].name != NULL; i++)
    if (!strcmp(name, chapter_elements[i].name))
      return i;

  assert(false);
  return -1;
}

static void
end_write_chapter_atom(void *data) {
  KaxChapterAtom *atom;
  chapter_writer_cb_t *cb;

  cb = (chapter_writer_cb_t *)data;
  atom = dynamic_cast<KaxChapterAtom *>(cb->e);
  assert(atom != NULL);
  if (FINDFIRST(atom, KaxChapterTimeStart) == NULL)
    pt(cb, "<ChapterTimeStart>00:00:00.000</ChapterTimeStart>\n");
}

static void
end_write_chapter_display(void *data) {
  KaxChapterDisplay *display;
  chapter_writer_cb_t *cb;

  cb = (chapter_writer_cb_t *)data;
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
    write_xml_element_rec(1, 0, (*chapters)[i], out);
}

// }}}
