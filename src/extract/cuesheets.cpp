/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts chapters and tags as CUE sheets from Matroska files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>

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

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/hacks.h"
#include "common/kax_analyzer.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/tags/tags.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

static KaxTag *
find_tag_for_track(int idx,
                   int64_t tuid,
                   int64_t cuid,
                   EbmlMaster &m) {
  std::string sidx = to_string(idx);

  size_t i;
  for (i = 0; i < m.ListSize(); i++) {
    if (EbmlId(*m[i]) != EBML_ID(KaxTag))
      continue;

    int64_t tag_cuid = get_tag_cuid(*static_cast<KaxTag *>(m[i]));
    if ((0 == cuid) && (-1 != tag_cuid) && (0 != tag_cuid))
      continue;

    if ((0 < cuid) && (tag_cuid != cuid))
      continue;

    int64_t tag_tuid = get_tag_tuid(*static_cast<KaxTag *>(m[i]));
    if (((-1 == tuid) || (-1 == tag_tuid) || (tuid == tag_tuid)) && ((get_simple_tag_value("PART_NUMBER", *static_cast<EbmlMaster *>(m[i])) == sidx) || (-1 == idx)))
      return static_cast<KaxTag *>(m[i]);
  }

  return nullptr;
}

static std::string
get_global_tag(const char *name,
               int64_t tuid,
               KaxTags &tags) {
  KaxTag *tag = find_tag_for_track(-1, tuid, 0, tags);
  if (nullptr == tag)
    return "";

  return get_simple_tag_value(name, *tag);
}

static int64_t
get_chapter_index(int idx,
                  KaxChapterAtom &atom) {
  size_t i;
  std::string sidx = (boost::format("INDEX %|1$02d|") % idx).str();
  for (i = 0; i < atom.ListSize(); i++)
    if ((EbmlId(*atom[i]) == EBML_ID(KaxChapterAtom)) &&
        (get_chapter_name(*static_cast<KaxChapterAtom *>(atom[i])) == sidx))
      return get_chapter_start(*static_cast<KaxChapterAtom *>(atom[i]));

  return -1;
}

#define print_if_global(name, format) _print_if_global(out, name, format, tuid, tags)
static void
_print_if_global(mm_io_c &out,
                 const char *name,
                 const char *format,
                 int64_t tuid,
                 KaxTags &tags) {
  std::string global = get_global_tag(name, tuid, tags);
  if (!global.empty())
    out.puts(boost::format(format) % global);
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
  std::string value = get_simple_tag_value(name, tag);
  if (!value.empty() && (value != get_global_tag(name, tuid, tags)))
    out.puts(boost::format(format) % value);
}

static void
print_comments(const char *prefix,
               KaxTag &tag,
               mm_io_c &out) {
  size_t i;

  for (i = 0; i < tag.ListSize(); i++)
    if (is_id(tag[i], KaxTagSimple)
        && (   (get_simple_tag_name(*static_cast<KaxTagSimple *>(tag[i])) == "COMMENT")
            || (get_simple_tag_name(*static_cast<KaxTagSimple *>(tag[i])) == "COMMENTS")))
      out.puts(boost::format("%1%REM \"%2%\"\n") % prefix % get_simple_tag_value(*static_cast<KaxTagSimple *>(tag[i])));
}

void
write_cuesheet(std::string file_name,
               KaxChapters &chapters,
               KaxTags &tags,
               int64_t tuid,
               mm_io_c &out) {
  if (chapters.ListSize() == 0)
    return;

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    file_name = "no-variable-data";

  out.write_bom("UTF-8");

  print_if_global("CATALOG",        "CATALOG %1%\n"); // until 0.9.6
  print_if_global("CATALOG_NUMBER", "CATALOG %1%\n"); // 0.9.7 and newer
  print_if_global("ARTIST",         "PERFORMER \"%1%\"\n");
  print_if_global("TITLE",          "TITLE \"%1%\"\n");
  print_if_global("DATE",           "REM DATE \"%1%\"\n"); // until 0.9.6
  print_if_global("DATE_RELEASED",  "REM DATE \"%1%\"\n"); // 0.9.7 and newer
  print_if_global("DISCID",         "REM DISCID %1%\n");

  KaxTag *tag = find_tag_for_track(-1, tuid, 0, tags);
  if (nullptr != tag)
    print_comments("", *tag, out);

  out.puts(boost::format("FILE \"%1%\" WAVE\n") % file_name);

  size_t i;
  for (i = 0; i < chapters.ListSize(); i++) {
    KaxChapterAtom &atom =  *static_cast<KaxChapterAtom *>(chapters[i]);

    out.puts(boost::format("  TRACK %|1$02d| AUDIO\n") % (i + 1));
    tag = find_tag_for_track(i + 1, tuid, get_chapter_uid(atom), tags);
    if (nullptr == tag)
      continue;

    print_if_available("TITLE",               "    TITLE \"%1%\"\n");
    print_if_available("ARTIST",              "    PERFORMER \"%1%\"\n");
    print_if_available("ISRC",                "    ISRC %1%\n");
    print_if_available("CDAUDIO_TRACK_FLAGS", "    FLAGS %1%\n");

    int k;
    for (k = 0; 100 > k; ++k) {
      int64_t temp_index = get_chapter_index(k, atom);
      if (-1 == temp_index)
        continue;

      out.puts(boost::format("    INDEX %|1$02d| %|2$02d|:%|3$02d|:%|4$02d|\n")
               % k
               % (temp_index / 1000000 / 1000 / 60)
               % ((temp_index / 1000000 / 1000) % 60)
               % irnd((double)(temp_index % 1000000000ll) * 75.0 / 1000000000.0));
    }

    print_if_available("DATE",          "    REM DATE \"%1%\"\n"); // until 0.9.6
    // 0.9.7 and newer:
    print_if_available("DATE_RELEASED", "    REM DATE \"%1%\"\n");
    print_if_available("GENRE",         "    REM GENRE \"%1%\"\n");
    print_comments("    ", *tag, out);
  }
}

void
extract_cuesheet(const std::string &file_name,
                 kax_analyzer_c::parse_mode_e parse_mode) {
  kax_analyzer_cptr analyzer;

  // open input file
  try {
    analyzer = kax_analyzer_cptr(new kax_analyzer_c(file_name));
    if (!analyzer->process(parse_mode, MODE_READ))
      throw false;
  } catch (...) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading (%2%).")) % file_name % strerror(errno));
    return;
  }

  KaxChapters all_chapters;
  ebml_master_cptr chapters_m(analyzer->read_all(EBML_INFO(KaxChapters)));
  ebml_master_cptr tags_m(    analyzer->read_all(EBML_INFO(KaxTags)));
  KaxChapters *chapters = dynamic_cast<KaxChapters *>(chapters_m.get());
  KaxTags *all_tags     = dynamic_cast<KaxTags *>(    tags_m.get());

  if ((nullptr != chapters) && (nullptr != all_tags)) {
    size_t i;
    for (i = 0; i < chapters->ListSize(); i++) {
      if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == nullptr)
        continue;

      KaxEditionEntry *eentry = dynamic_cast<KaxEditionEntry *>((*chapters)[i]);
      size_t k;
      for (k = 0; k < eentry->ListSize(); k++)
        if (dynamic_cast<KaxChapterAtom *>((*eentry)[k]) != nullptr)
          all_chapters.PushElement(*(*eentry)[k]);
    }

    write_cuesheet(file_name, all_chapters, *all_tags, -1, *g_mm_stdio);

    while (all_chapters.ListSize() > 0)
      all_chapters.Remove(0);
  }
}
