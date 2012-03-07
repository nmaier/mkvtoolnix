/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Mapping from XML elements to EBML elements

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomm@free.fr>.
*/

#include "common/common_pch.h"

#include <matroska/KaxConfig.h>
#include <matroska/KaxSegment.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/tags/parser.h"
#include "common/xml/element_mapping.h"

using namespace libmatroska;

static EbmlId no_id((uint32)0, 0);

parser_element_t *chapter_elements     = nullptr;
parser_element_t *tag_elements         = nullptr;
parser_element_t *segmentinfo_elements = nullptr;

static void
init_mapping_table(parser_element_t *table) {
  int i;
  for (i = 0; table[i].name != nullptr; i++) {
    if (EBMLT_SKIP == table[i].type)
      continue;

    const char *debug_name      = table[i].debug_name != nullptr ? table[i].debug_name : table[i].name;
    const EbmlCallbacks *result = find_ebml_callbacks(EBML_INFO(KaxSegment), debug_name);
    if (nullptr == result)
      mxerror(boost::format(Y("Error initializing the tables for the chapter, tag and segment info elements: "
                              "Could not find the element with the debug name '%1%'. %2%\n")) % debug_name % BUGMSG);
    table[i].id = EBML_INFO_ID(*result);
  }
}

void
xml_element_map_init() {
  static parser_element_t _chapter_elements[] = {
    {"Chapters",              EBMLT_MASTER,  0,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"EditionEntry",          EBMLT_MASTER,  1,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"EditionUID",            EBMLT_UINT,    2,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"EditionFlagHidden",     EBMLT_BOOL,    2,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"EditionFlagOrdered",    EBMLT_BOOL,    2,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"EditionFlagDefault",    EBMLT_BOOL,    2,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"EditionProcessed",      EBMLT_SKIP,    2,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"ChapterAtom",           EBMLT_MASTER,  2,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterUID",            EBMLT_UINT,    3,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTimeStart",      EBMLT_TIME,    3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTimeEnd",        EBMLT_TIME,    3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterFlagHidden",     EBMLT_BOOL,    3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterFlagEnabled",    EBMLT_BOOL,    3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterPhysicalEquiv",  EBMLT_UINT,    3,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"ChapterSegmentUID",     EBMLT_BINARY,  3, 16,           16, no_id,                nullptr, nullptr, nullptr},

    {"ChapterProcess",        EBMLT_MASTER,  3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterProcessCodecID", EBMLT_UINT,    4,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"ChapterProcessPrivate", EBMLT_BINARY,  4,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"ChapterProcessCommand", EBMLT_MASTER,  4,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterProcessTime",    EBMLT_UINT,    5,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterProcessData",    EBMLT_BINARY,  5,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"ChapterTrack",          EBMLT_MASTER,  3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTrackNumber",    EBMLT_UINT,    4,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},

    {"ChapterDisplay",        EBMLT_MASTER,  3,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterString",         EBMLT_USTRING, 4,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterLanguage",       EBMLT_STRING,  4,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterCountry",        EBMLT_STRING,  4,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {nullptr,                    EBMLT_MASTER,  0,  0,            0, EbmlId((uint32)0, 0), nullptr, nullptr, nullptr}
  };

  static parser_element_t _tag_elements[] = {
    {"Tags",            EBMLT_MASTER,  0, 0,            0, no_id,                nullptr, nullptr, nullptr},

    {"Tag",             EBMLT_MASTER,  1, 0,            0, no_id,                nullptr, nullptr, nullptr},

    {"Targets",         EBMLT_MASTER,  2, 0,            0, no_id,                nullptr, nullptr, "TagTargets"},
    {"TrackUID",        EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                nullptr, nullptr, "TagTrackUID"},
    {"EditionUID",      EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                nullptr, nullptr, "TagEditionUID"},
    {"ChapterUID",      EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                nullptr, nullptr, "TagChapterUID"},
    {"AttachmentUID",   EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                nullptr, nullptr, "TagAttachmentUID"},
    {"TargetType",      EBMLT_STRING,  3, 0,            0, no_id,                nullptr, nullptr, "TagTargetType"},
    {"TargetTypeValue", EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                nullptr, nullptr, "TagTargetTypeValue"},

    {"Simple",          EBMLT_MASTER,  2, 0,            0, no_id,                nullptr, nullptr, "TagSimple"},
    {"Name",            EBMLT_USTRING, 3, 0,            0, no_id,                nullptr, nullptr, "TagName"},
    {"String",          EBMLT_USTRING, 3, 0,            0, no_id,                nullptr, nullptr, "TagString"},
    {"Binary",          EBMLT_BINARY,  3, 0,            0, no_id,                nullptr, nullptr, "TagBinary"},
    {"TagLanguage"    , EBMLT_STRING,  3, 0,            0, no_id,                nullptr, nullptr, "TagLanguage"},
    {"DefaultLanguage", EBMLT_BOOL,    3, 0,            1, no_id,                nullptr, nullptr, "TagDefault"},

    {nullptr,              EBMLT_MASTER,  0, 0,            0, EbmlId((uint32)0, 0), nullptr, nullptr, nullptr}
  };

  static parser_element_t _segmentinfo_elements[] = {
    {"Info",                       EBMLT_MASTER, 0,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"SegmentUID",                 EBMLT_BINARY, 1, 16,           16, no_id,                nullptr, nullptr, nullptr},
    {"NextSegmentUID",             EBMLT_BINARY, 1, 16,           16, no_id,                nullptr, nullptr, "NextUID"},
    {"PreviousSegmentUID",         EBMLT_BINARY, 1, 16,           16, no_id,                nullptr, nullptr, "PrevUID"},

    {"SegmentFamily",              EBMLT_BINARY, 1,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {"ChapterTranslate",           EBMLT_MASTER, 1,  0,            0, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTranslateEditionUID", EBMLT_UINT,   2,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTranslateCodec",      EBMLT_UINT,   2,  0, NO_MAX_VALUE, no_id,                nullptr, nullptr, nullptr},
    {"ChapterTranslateID",         EBMLT_BINARY, 2,  0,            0, no_id,                nullptr, nullptr, nullptr},

    {nullptr,                         EBMLT_MASTER, 0,  0,            0, EbmlId((uint32)0, 0), nullptr, nullptr, nullptr}
  };

  chapter_elements     = _chapter_elements;
  tag_elements         = _tag_elements;
  segmentinfo_elements = _segmentinfo_elements;
  init_mapping_table(chapter_elements);
  init_mapping_table(tag_elements);
  init_mapping_table(segmentinfo_elements);
}

int
xml_element_map_index(const parser_element_t *element_map,
                      const char *name) {
  int i;
  for (i = 0; element_map[i].name != nullptr; i++)
    if (!strcmp(name, element_map[i].name))
      return i;

  return -1;
}
