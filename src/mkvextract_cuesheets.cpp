/*
  mkvextract -- extract tracks from Matroska files into other files

  mkvextract_cuesheets.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief extracts chapters and tags as CUE sheets from Matroska files
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

static string
get_simple_tag(const char *name,
               EbmlMaster &m) {
  int i;
  UTFstring uname;
  string rvalue;

  uname = cstrutf8_to_UTFstring(name);

  for (i = 0; i < m.ListSize(); i++)
    if (EbmlId(*m[i]) == KaxTagSimple::ClassInfos.GlobalId) {
      KaxTagName *tname;
      UTFstring utname;

      tname = FINDFIRST(m[i], KaxTagName);
      if (tname == NULL)
        continue;
      utname = UTFstring(*static_cast<EbmlUnicodeString *>(tname));
      if (utname == uname) {
        KaxTagString *tvalue;
        tvalue = FINDFIRST(m[i], KaxTagString);
        if (tvalue != NULL) {
          char *value;

          value =
            UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                            (tvalue)));
          rvalue = value;
          safefree(value);
          return rvalue;
        } else {
          return "";
        }
      }
    }

  for (i = 0; i < m.ListSize(); i++)
    if ((EbmlId(*m[i]) == KaxTag::ClassInfos.GlobalId) ||
        (EbmlId(*m[i]) == KaxTagSimple::ClassInfos.GlobalId)) {
      rvalue = get_simple_tag(name, *static_cast<EbmlMaster *>(m[i]));
      if (rvalue != "")
        return rvalue;
    }

  return "";
}

static int64_t
get_tag_tuid(KaxTag &tag) {
  KaxTagTargets *targets;
  KaxTagTrackUID *tuid;

  targets = FINDFIRST(&tag, KaxTagTargets);
  if (targets == NULL)
    return -1;
  tuid = FINDFIRST(targets, KaxTagTrackUID);
  if (tuid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(tuid));
}

static int64_t
get_tag_cuid(KaxTag &tag) {
  KaxTagTargets *targets;
  KaxTagChapterUID *cuid;

  targets = FINDFIRST(&tag, KaxTagTargets);
  if (targets == NULL)
    return -1;
  cuid = FINDFIRST(targets, KaxTagChapterUID);
  if (cuid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(cuid));
}

static KaxTag *
find_tag_for_track(int idx,
                   int64_t tuid,
                   int64_t cuid,
                   EbmlMaster &m) {
  string sidx;
  int i;
  int64_t tag_tuid, tag_cuid;

  sidx = mxsprintf("%d", idx);

  for (i = 0; i < m.ListSize(); i++)
    if (EbmlId(*m[i]) == KaxTag::ClassInfos.GlobalId) {
      tag_cuid = get_tag_cuid(*static_cast<KaxTag *>(m[i]));
      if ((cuid == 0) && (tag_cuid != -1) && (tag_cuid != 0))
        continue;
      if ((cuid > 0) && (tag_cuid != cuid))
        continue;
      tag_tuid = get_tag_tuid(*static_cast<KaxTag *>(m[i]));
      if (((tuid == -1) || (tag_tuid == -1) || (tuid == tag_tuid)) &&
          ((get_simple_tag("SET_PART",
                           *static_cast<EbmlMaster *>(m[i])) == sidx) ||
           (idx == -1)))
        return static_cast<KaxTag *>(m[i]);
    }

  return NULL;
}

static string
get_global_tag(const char *name,
               int64_t tuid,
               KaxTags &tags) {
  KaxTag *tag;

  tag = find_tag_for_track(-1, tuid, 0, tags);
  if (tag == NULL)
    return "";

  return get_simple_tag(name, *tag);
}

static int64_t
get_chapter_index(int idx,
                  KaxChapterAtom &atom) {
  int i;
  string sidx;

  sidx = mxsprintf("INDEX 0%d", idx);
  for (i = 0; i < atom.ListSize(); i++)
    if ((EbmlId(*atom[i]) == KaxChapterAtom::ClassInfos.GlobalId) &&
        (get_chapter_name(*static_cast<KaxChapterAtom *>(atom[i])) == sidx))
      return get_chapter_start(*static_cast<KaxChapterAtom *>(atom[i]));

  return -1;
}

static string
get_simple_tag_name(KaxTagSimple &tag) {
  KaxTagName *tname;
  string result;
  char *name;

  tname = FINDFIRST(&tag, KaxTagName);
  if (tname == NULL)
    return "";
  name = UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                         (tname)));
  result = name;
  safefree(name);
  return result;
}

static string
get_simple_tag_value(KaxTagSimple &tag) {
  KaxTagString *tstring;
  string result;
  char *value;

  tstring = FINDFIRST(&tag, KaxTagString);
  if (tstring == NULL)
    return "";
  value = UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>
                                          (tstring)));
  result = value;
  safefree(value);
  return result;
}

#define print_if_global(name, format) \
  _print_if_global(out, name, format, chapters.ListSize(), tuid, tags)
static void
_print_if_global(mm_io_c &out,
                 const char *name,
                 const char *format,
                 int num_entries,
                 int64_t tuid,
                 KaxTags &tags) {
  string global;

  global = get_global_tag(name, tuid, tags);
  if (global != "")
    out.printf(format, global.c_str());
}

#define print_if_available(name, format) \
  _print_if_available(out, name, format, tuid, tags, *tag)
static void
_print_if_available(mm_io_c &out,
                    const char *name,
                    const char *format,
                    int64_t tuid,
                    KaxTags &tags,
                    KaxTag &tag) {
  string value;

  value = get_simple_tag(name, tag);
  if ((value != "") &&
      (value != get_global_tag(name, tuid, tags)))
    out.printf(format, value.c_str());
}

static void
print_comments(const char *prefix,
               KaxTag &tag,
               mm_io_c &out) {
  int i;

  for (i = 0; i < tag.ListSize(); i++)
    if (is_id(tag[i], KaxTagSimple) &&
        (get_simple_tag_name(*static_cast<KaxTagSimple *>(tag[i])) ==
         "COMMENTS"))
      out.printf("%sREM COMMENT \"%s\"\n", prefix,
                 get_simple_tag_value(*static_cast<KaxTagSimple *>(tag[i])).
                 c_str());
}

void
write_cuesheet(const char *file_name,
               KaxChapters &chapters,
               KaxTags &tags,
               int64_t tuid,
               mm_io_c &out) {
  KaxTag *tag;
  string s;
  int i;
  int64_t index_00, index_01;

  if (chapters.ListSize() == 0)
    return;

  print_if_global("DATE", "REM DATE %s\n");
  print_if_global("DISCID", "REM DISCID %s\n");
  print_if_global("ARTIST", "PERFORMER \"%s\"\n");
  print_if_global("ALBUM", "TITLE \"%s\"\n");
  print_if_global("CATALOG", "CATALOG %s\n");
  tag = find_tag_for_track(-1, tuid, 0, tags);
  if (tag != NULL)
    print_comments("", *tag, out);

  out.printf("FILE \"%s\" WAVE\n", file_name);

  for (i = 0; i < chapters.ListSize(); i++) {
    KaxChapterAtom &atom =  *static_cast<KaxChapterAtom *>(chapters[i]);

    out.printf("  TRACK %02d AUDIO\n", i + 1);
    tag = find_tag_for_track(i + 1, tuid, get_chapter_uid(atom), tags);
    if (tag != NULL) {
      print_if_available("TITLE", "    TITLE \"%s\"\n");
      print_if_available("ARTIST", "    PERFORMER \"%s\"\n");
      print_if_available("ISRC", "    ISRC %s\n");
      index_00 = get_chapter_index(0, atom);
      index_01 = get_chapter_index(1, atom);
      if (index_01 == -1) {
        index_01 = get_chapter_start(atom);
        if (index_01 == -1)
          index_01 = 0;
      }
      if (index_00 != -1)
        out.printf("    INDEX 00 %02lld:%02lld:%02lld\n", 
                   index_00 / 1000000 / 1000 / 60,
                   (index_00 / 1000000 / 1000) % 60,
                   irnd((double)(index_00 % 1000000000ll) * 75.0 /
                        1000000000.0));
      out.printf("    INDEX 01 %02lld:%02lld:%02lld\n", 
                 index_01 / 1000000 / 1000 / 60,
                 (index_01 / 1000000 / 1000) % 60,
                 irnd((double)(index_01 % 1000000000ll) * 75.0 /
                      1000000000.0));
      print_if_available("DATE", "    REM DATE %s\n");
      print_if_available("GENRE", "    REM GENRE %s\n");
      print_comments("    ", *tag, out);
    }
  }
}

void
extract_cuesheets(const char *file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  EbmlStream *es;
  mm_io_c *in;
  mm_stdio_c out;
  KaxChapters all_chapters;
  KaxTags all_tags;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error(_("The file '%s' could not be opened for reading (%s)."),
               file_name, strerror(errno));
    return;
  }

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error(_("Error: No EBML head found."));
      delete es;

      return;
    }
      
    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error(_("No segment/level 0 element found."));
        return;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, _("Segment"));
        break;
      }

      show_element(l0, 0, _("Next level 0 element is not a segment but %s"),
                   l0->Generic().DebugName);

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    // We've got our segment, so let's find the chapters
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId) {
        KaxChapters &chapters = *static_cast<KaxChapters *>(l1);
        chapters.Read(*es, KaxChapters::ClassInfos.Context, upper_lvl_el, l2,
                      true);
        if (verbose > 0)
          debug_dump_elements(&chapters, 0);

        while (chapters.ListSize() > 0) {
          if (EbmlId(*chapters[0]) == KaxEditionEntry::ClassInfos.GlobalId) {
            KaxEditionEntry &entry =
              *static_cast<KaxEditionEntry *>(chapters[0]);
            while (entry.ListSize() > 0) {
              if (EbmlId(*entry[0]) == KaxChapterAtom::ClassInfos.GlobalId)
                all_chapters.PushElement(*entry[0]);
              entry.Remove(0);
            }
          }
          chapters.Remove(0);
        }

      } else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId) {
        KaxTags &tags = *static_cast<KaxTags *>(l1);
        tags.Read(*es, KaxTags::ClassInfos.Context, upper_lvl_el, l2, true);
        if (verbose > 0)
          debug_dump_elements(&tags, 0);

        while (tags.ListSize() > 0) {
          all_tags.PushElement(*tags[0]);
          tags.Remove(0);
        }

      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    write_cuesheet(file_name, all_chapters, all_tags, -1, out);

    delete l0;
    delete es;
    delete in;

  } catch (exception &ex) {
    show_error(_("Caught exception: %s"), ex.what());
    delete in;

    return;
  }
}
