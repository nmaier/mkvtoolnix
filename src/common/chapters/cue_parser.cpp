/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   chapter parser for CUE sheets

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Patches by Nicolas Le Guen <nleguen@pepper-prod.com> and
   Vegard Pettersen <vegard_p@broadpark.no>
*/

#include "common/common_pch.h"

#include <ctype.h>
#include <stdarg.h>

#include <stdio.h>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/locale.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/unique_numbers.h"

using namespace libmatroska;

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

bool
probe_cue_chapters(mm_text_io_c *in) {
  std::string s;

  in->setFilePointer(0);
  if (!in->getline2(s))
    return false;

  return (ba::istarts_with(s, "performer ") || ba::istarts_with(s, "title ") || ba::istarts_with(s, "file ") || ba::istarts_with(s, "catalog ") || ba::istarts_with(s, "rem "));
}

std::string g_cue_to_chapter_name_format;

static void
cue_entries_to_chapter_name(std::string &performer,
                            std::string &title,
                            std::string &global_performer,
                            std::string &global_title,
                            std::string &name,
                            int num) {
  name = "";

  if (title.empty())
    title = global_title;

  if (performer.empty())
    performer = global_performer;

  const char *this_char = g_cue_to_chapter_name_format.empty() ? "%p - %t" : g_cue_to_chapter_name_format.c_str();
  const char *next_char = this_char + 1;

  while (0 != *this_char) {
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

struct cue_parser_args_t {
  int num;
  int64_t start_of_track;
  std::vector<int64_t> start_indices;
  bool index00_missing;
  int64_t end;
  int64_t min_tc;
  int64_t max_tc;
  int64_t offset;
  KaxChapters *chapters;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  bool do_convert;
  std::string global_catalog;
  std::string global_performer;
  std::string performer;
  std::string global_title;
  std::string title;
  std::string name;
  std::string global_date;
  std::string date;
  std::string global_genre;
  std::string genre;
  std::string global_disc_id;
  std::string isrc;
  std::string flags;
  std::vector<std::string> global_rem;
  std::vector<std::string> global_comment;
  std::vector<std::string> comment;
  std::string language;
  int line_num;
  charset_converter_cptr cc_utf8;

  cue_parser_args_t()
    : num(0)
    , start_of_track(-1)
    , index00_missing(false)
    , end(0)
    , min_tc(0)
    , max_tc(0)
    , offset(0)
    , chapters(nullptr)
    , edition(nullptr)
    , atom(nullptr)
    , do_convert(false)
    , line_num(0)
  {
  }
};

static UTFstring
cue_str_internal_to_utf(cue_parser_args_t &a,
                        const std::string &s) {
  if (a.do_convert)
    return cstrutf8_to_UTFstring(a.cc_utf8->utf8(s));
  else
    return cstrutf8_to_UTFstring(s);
}

static KaxTagSimple *
create_simple_tag(cue_parser_args_t &a,
                  const std::string &name,
                  const std::string &value) {
  KaxTagSimple *simple = new KaxTagSimple;

  GetChildAs<KaxTagName,   EbmlUnicodeString>(*simple) = cue_str_internal_to_utf(a, name);
  GetChildAs<KaxTagString, EbmlUnicodeString>(*simple) = cue_str_internal_to_utf(a, value);

  return simple;
}

#define create_tag1(v1, text) \
  if (v1 != "") \
    tag->PushElement(*create_simple_tag(a, text, v1));
#define create_tag2(v1, v2, text) \
  if (((v1) != "" ? (v1) : (v2)) != "") \
    tag->PushElement(*create_simple_tag(a, text, ((v1) != "" ? (v1) : (v2))));

static void
add_tag_for_cue_entry(cue_parser_args_t &a,
                      KaxTags **tags,
                      uint32_t cuid) {
  if (nullptr == tags)
    return;

  if (nullptr == *tags)
    *tags = new KaxTags;

  KaxTag *tag            = new KaxTag;
  KaxTagTargets *targets = &GetChild<KaxTagTargets>(*tag);
  GetChildAs<KaxTagChapterUID,      EbmlUInteger>(*targets) = cuid;
  GetChildAs<KaxTagTargetTypeValue, EbmlUInteger>(*targets) = TAG_TARGETTYPE_TRACK;
  GetChildAs<KaxTagTargetType,      EbmlString>(*targets)   = "track";

  create_tag1(a.title, "TITLE");
  tag->PushElement(*create_simple_tag(a, "PART_NUMBER", to_string(a.num)));
  create_tag2(a.performer, a.global_performer, "ARTIST");
  create_tag2(a.date, a.global_date, "DATE_RELEASED");
  create_tag2(a.genre, a.global_genre, "GENRE");
  create_tag1(a.isrc, "ISRC");
  create_tag1(a.flags, "CDAUDIO_TRACK_FLAGS");

  size_t i;
  for (i = 0; i < a.global_comment.size(); i++)
    create_tag1(a.global_comment[i], "COMMENT");

  for (i = 0; i < a.comment.size(); i++)
    create_tag1(a.comment[i], "COMMENT");

  if (FINDFIRST(tag, KaxTagSimple) != nullptr)
    (*tags)->PushElement(*tag);
  else
    delete tag;
}

static void
add_tag_for_global_cue_settings(cue_parser_args_t &a,
                                KaxTags **tags) {
  if (nullptr == tags)
    return;

  if (nullptr == *tags)
    *tags = new KaxTags;

  KaxTag *tag            = new KaxTag;
  KaxTagTargets *targets = &GetChild<KaxTagTargets>(*tag);

  GetChildAs<KaxTagTargetTypeValue, EbmlUInteger>(*targets) = TAG_TARGETTYPE_ALBUM;
  GetChildAs<KaxTagTargetType, EbmlString>(*targets)        = "album";

  create_tag1(a.global_performer, "ARTIST");
  create_tag1(a.global_title,     "TITLE");
  create_tag1(a.global_date,      "DATE_RELEASED");
  create_tag1(a.global_disc_id,   "DISCID");
  create_tag1(a.global_catalog,   "CATALOG_NUMBER");

  size_t i;
  for (i = 0; i < a.global_rem.size(); i++)
    create_tag1(a.global_rem[i], "COMMENT");

  if (FINDFIRST(tag, KaxTagSimple) != nullptr)
    (*tags)->PushElement(*tag);
  else
    delete tag;
}

static void
add_subchapters_for_index_entries(cue_parser_args_t &a) {
  if (a.start_indices.empty())
    return;

  KaxChapterAtom *atom = nullptr;
  size_t offset        = a.index00_missing ? 1 : 0;
  size_t i;
  for (i = 0; i < a.start_indices.size(); i++) {
    atom                                                             = &GetFirstOrNextChild<KaxChapterAtom>(a.atom, atom);

    GetChildAs<KaxChapterUID,           EbmlUInteger>(*atom)         = create_unique_uint32(UNIQUE_CHAPTER_IDS);
    GetChildAs<KaxChapterTimeStart,     EbmlUInteger>(*atom)         = a.start_indices[i] - a.offset;
    GetChildAs<KaxChapterFlagHidden,    EbmlUInteger>(*atom)         = 1;
    GetChildAs<KaxChapterPhysicalEquiv, EbmlUInteger>(*atom)         = CHAPTER_PHYSEQUIV_INDEX;

    KaxChapterDisplay *display                                       = &GetChild<KaxChapterDisplay>(*atom);
    GetChildAs<KaxChapterString,        EbmlUnicodeString>(*display) = cstrutf8_to_UTFstring((boost::format("INDEX %|1$02d|") % (i + offset)).str().c_str());
    GetChildAs<KaxChapterLanguage,      EbmlString>(*display)        = "eng";
  }
}

static void
add_elements_for_cue_entry(cue_parser_args_t &a,
                           KaxTags **tags) {
  if (a.start_indices.empty())
    mxerror(boost::format(Y("Cue sheet parser: No INDEX entry found for the previous TRACK entry (current line: %1%)\n")) % a.line_num);

  if (!((a.start_indices[0] >= a.min_tc) && ((a.start_indices[0] <= a.max_tc) || (a.max_tc == -1))))
    return;

  cue_entries_to_chapter_name(a.performer, a.title, a.global_performer, a.global_title, a.name, a.num);

  if (nullptr == a.edition) {
    a.edition                                           = &GetChild<KaxEditionEntry>(*a.chapters);
    GetChildAs<KaxEditionUID, EbmlUInteger>(*a.edition) = create_unique_uint32(UNIQUE_EDITION_IDS);
  }

  a.atom                                                      = &GetFirstOrNextChild<KaxChapterAtom>(*a.edition, a.atom);
  GetChildAs<KaxChapterPhysicalEquiv, EbmlUInteger>(*a.atom)  = CHAPTER_PHYSEQUIV_TRACK;
  uint32_t cuid                                               = create_unique_uint32(UNIQUE_CHAPTER_IDS);
  GetChildAs<KaxChapterUID,           EbmlUInteger>(*a.atom)  = cuid;
  GetChildAs<KaxChapterTimeStart,     EbmlUInteger>(*a.atom)  = a.start_of_track - a.offset;

  KaxChapterDisplay *display                                  = &GetChild<KaxChapterDisplay>(*a.atom);
  GetChildAs<KaxChapterString,   EbmlUnicodeString>(*display) = cue_str_internal_to_utf(a, a.name);
  GetChildAs<KaxChapterLanguage, EbmlString>(*display)        = a.language;

  add_subchapters_for_index_entries(a);

  add_tag_for_cue_entry(a, tags, cuid);
}

static std::string
get_quoted(std::string src,
           int offset) {
  src.erase(0, offset);
  strip(src);

  if (!src.empty() && (src[0] == '"'))
    src.erase(0, 1);

  if (!src.empty() && (src[src.length() - 1] == '"'))
    src.erase(src.length() - 1);

  return src;
}

static std::string
erase_colon(std::string &s,
            size_t skip) {
  size_t i = skip + 1;

  while ((s.length() > i) && (s[i] == ' '))
    i++;

  while ((s.length() > i) && (isalpha(s[i])))
    i++;

  if (s.length() == i)
    return s;

  if (':' == s[i])
    s.erase(i, 1);

  else if (s.substr(i, 2) == " :")
    s.erase(i, 2);

  return s;
}

KaxChapters *
parse_cue_chapters(mm_text_io_c *in,
                   int64_t min_tc,
                   int64_t max_tc,
                   int64_t offset,
                   const std::string &language,
                   const std::string &charset,
                   KaxTags **tags) {
  cue_parser_args_t a;
  std::string line;

  in->setFilePointer(0);
  a.chapters = new KaxChapters;

  if (in->get_byte_order() == BO_NONE) {
    a.do_convert = true;
    a.cc_utf8    = charset_converter_c::init(charset);
  }

  a.language = language.empty() ? "eng" : language;
  a.min_tc   = min_tc;
  a.max_tc   = max_tc;
  a.offset   = offset;

  try {
    while (in->getline2(line)) {
      a.line_num++;
      strip(line);

      if ((line.empty()) || ba::istarts_with(line, "file "))
        continue;

      if (ba::istarts_with(line, "performer ")) {
        if (0 == a.num)
          a.global_performer = get_quoted(line, 10);
        else
          a.performer        = get_quoted(line, 10);

      } else if (ba::istarts_with(line, "catalog "))
        a.global_catalog = get_quoted(line, 8);

      else if (ba::istarts_with(line, "title ")) {
        if (0 == a.num)
          a.global_title = get_quoted(line, 6);
        else
          a.title        = get_quoted(line, 6);

      } else if (ba::istarts_with(line, "index ")) {
        unsigned int index, min, sec, frames;

        line.erase(0, 6);
        strip(line);
        if (sscanf(line.c_str(), "%u %u:%u:%u", &index, &min, &sec, &frames) < 4)
          mxerror(boost::format(Y("Cue sheet parser: Invalid INDEX entry in line %1%.\n")) % a.line_num);

        bool index_ok = false;
        if (99 >= index) {
          if ((a.start_indices.empty()) && (1 == index))
            a.index00_missing = true;

          if ((a.start_indices.size() == index) || ((a.start_indices.size() == (index - 1)) && a.index00_missing)) {
            int64_t timestamp = min * 60 * 1000000000ll + sec * 1000000000ll + frames * 1000000000ll / 75;
            a.start_indices.push_back(timestamp);

            if ((1 == index) || (0 == index))
              a.start_of_track = timestamp;

            index_ok = true;
          }
        }

        if (!index_ok)
          mxerror(boost::format(Y("Cue sheet parser: Invalid INDEX number (got %1%, expected %2%) in line %3%,\n")) % index % a.start_indices.size() % a.line_num);

      } else if (ba::istarts_with(line, "track ")) {
        if ((line.length() < 5) || strcasecmp(&line[line.length() - 5], "audio"))
          continue;

        if (1 <= a.num)
          add_elements_for_cue_entry(a, tags);
        else
          add_tag_for_global_cue_settings(a, tags);

        a.num++;
        a.start_of_track  = -1;
        a.index00_missing = false;
        a.performer       = "";
        a.title           = "";
        a.isrc            = "";
        a.date            = "";
        a.genre           = "";
        a.flags           = "";
        a.start_indices.clear();
        a.comment.clear();

      } else if (ba::istarts_with(line, "isrc "))
        a.isrc = get_quoted(line, 5);

      else if (ba::istarts_with(line, "flags "))
        a.flags = get_quoted(line, 6);

      else if (ba::istarts_with(line, "rem ")) {
        erase_colon(line, 4);
        if (ba::istarts_with(line, "rem date ") || ba::istarts_with(line, "rem year ")) {
          if (0 == a.num)
            a.global_date = get_quoted(line, 9);
          else
            a.date        = get_quoted(line, 9);

        } else if (ba::istarts_with(line, "rem genre ")) {
          if (0 == a.num)
            a.global_genre = get_quoted(line, 10);
          else
            a.genre        = get_quoted(line, 10);

        } else if (ba::istarts_with(line, "rem discid "))
          a.global_disc_id = get_quoted(line, 11);

        else if (ba::istarts_with(line, "rem comment ")) {
          if (0 == a.num)
            a.global_comment.push_back(get_quoted(line, 12));
          else
            a.comment.push_back(get_quoted(line, 12));

        } else {
          if (0 == a.num)
            a.global_rem.push_back(get_quoted(line, 4));
          else
            a.comment.push_back(get_quoted(line, 4));
        }
      }
    }

    if (1 <= a.num)
      add_elements_for_cue_entry(a, tags);

  } catch(mtx::exception &e) {

    delete a.chapters;
    throw;
  }

  if (0 == a.num) {
    delete a.chapters;
    return nullptr;
  }

  return a.chapters;
}
