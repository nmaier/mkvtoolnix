/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts chapters and tags as CUE sheets from Matroska files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

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
#include "quickparser.h"
#include "tag_common.h"

using namespace libmatroska;
using namespace std;

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
          ((get_simple_tag_value("PART_NUMBER",
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

  return get_simple_tag_value(name, *tag);
}

static int64_t
get_chapter_index(int idx,
                  KaxChapterAtom &atom) {
  int i;
  string sidx;

  sidx = mxsprintf("INDEX %02d", idx);
  for (i = 0; i < atom.ListSize(); i++)
    if ((EbmlId(*atom[i]) == KaxChapterAtom::ClassInfos.GlobalId) &&
        (get_chapter_name(*static_cast<KaxChapterAtom *>(atom[i])) == sidx))
      return get_chapter_start(*static_cast<KaxChapterAtom *>(atom[i]));

  return -1;
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

  value = get_simple_tag_value(name, tag);
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
        ((get_simple_tag_name(*static_cast<KaxTagSimple *>(tag[i])) ==
          "COMMENT") ||
         (get_simple_tag_name(*static_cast<KaxTagSimple *>(tag[i])) ==
          "COMMENTS")))
      out.printf("%sREM \"%s\"\n", prefix,
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
  int i, j;
  int64_t temp_index;

  if (chapters.ListSize() == 0)
    return;

  if (no_variable_data)
    file_name = "no-variable-data";

  out.write_bom("UTF-8");

  print_if_global("CATALOG", "CATALOG %s\n"); // until 0.9.6
  print_if_global("CATALOG_NUMBER", "CATALOG %s\n"); // 0.9.7 and newer
  print_if_global("ARTIST", "PERFORMER \"%s\"\n");
  print_if_global("TITLE", "TITLE \"%s\"\n");
  print_if_global("DATE", "REM DATE \"%s\"\n"); // until 0.9.6
  print_if_global("DATE_RELEASED", "REM DATE \"%s\"\n"); // 0.9.7 and newer
  print_if_global("DISCID", "REM DISCID %s\n");

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
      print_if_available("CDAUDIO_TRACK_FLAGS", "    FLAGS %s\n");

      for (j = 0; j < 100; j++) {
        temp_index = get_chapter_index(j, atom);
        if (temp_index == -1)
          continue;
        out.printf("    INDEX %02d %02lld:%02lld:%02lld\n",
                   j,
                   temp_index / 1000000 / 1000 / 60,
                   (temp_index / 1000000 / 1000) % 60,
                   irnd((double)(temp_index % 1000000000ll) * 75.0 /
                        1000000000.0));
      }

      print_if_available("DATE", "    REM DATE \"%s\"\n"); // until 0.9.6
      // 0.9.7 and newer:
      print_if_available("DATE_RELEASED", "    REM DATE \"%s\"\n");
      print_if_available("GENRE", "    REM GENRE \"%s\"\n");
      print_comments("    ", *tag, out);
    }
  }
}

void
extract_cuesheet(const char *file_name,
                 bool parse_fully) {
  mm_io_c *in;
  mm_stdio_c out;
  kax_quickparser_c *qp;
  KaxChapters all_chapters, *chapters;
  KaxEditionEntry *eentry;
  KaxTags *all_tags;
  int i, k;

  // open input file
  try {
    in = new mm_file_io_c(file_name);
    qp = new kax_quickparser_c(*in, parse_fully);
  } catch (std::exception &ex) {
    show_error(_("The file '%s' could not be opened for reading (%s)."),
               file_name, strerror(errno));
    return;
  }

  chapters =
    dynamic_cast<KaxChapters *>(qp->read_all(KaxChapters::ClassInfos));
  all_tags = dynamic_cast<KaxTags *>(qp->read_all(KaxTags::ClassInfos));
  if ((chapters != NULL) && (all_tags != NULL)) {
    for (i = 0; i < chapters->ListSize(); i++) {
      if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL)
        continue;
      eentry = dynamic_cast<KaxEditionEntry *>((*chapters)[i]);
      for (k = 0; k < eentry->ListSize(); k++)
        if (dynamic_cast<KaxChapterAtom *>((*eentry)[k]) != NULL)
          all_chapters.PushElement(*(*eentry)[k]);
    }
    if (verbose > 0) {
      debug_dump_elements(&all_chapters, 0);
      debug_dump_elements(all_tags, 0);
    }

    write_cuesheet(file_name, all_chapters, *all_tags, -1, out);

    while (all_chapters.ListSize() > 0)
      all_chapters.Remove(0);
  }

  delete all_tags;
  delete chapters;

  delete in;
  delete qp;
}
