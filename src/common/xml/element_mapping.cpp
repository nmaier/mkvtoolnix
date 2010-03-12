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

#include <ebml/EbmlElement.h>
#include <matroska/KaxConfig.h>
#include <matroska/KaxSegment.h>

#include "common/chapters/chapters.h"
#include "common/common.h"
#include "common/ebml.h"
#include "common/tags/parser.h"
#include "common/xml/element_mapping.h"

using namespace libmatroska;

static EbmlId no_id((uint32)0, 0);

parser_element_t *chapter_elements = NULL;
parser_element_t *tag_elements = NULL;
parser_element_t *segmentinfo_elements = NULL;

static void
init_mapping_table(parser_element_t *table) {
  const char *debug_name;
  const EbmlCallbacks *result;
  int i;

  for (i = 0; table[i].name != NULL; i++) {
    if (EBMLT_SKIP == table[i].type)
      continue;
    debug_name = table[i].debug_name != NULL ? table[i].debug_name :
      table[i].name;
    result = find_ebml_callbacks(CLASS_INFO(KaxSegment), debug_name);
    if (NULL == result)
      mxerror(boost::format(Y("Error initializing the tables for the chapter, tag and segment info elements: "
                              "Could not find the element with the debug name '%1%'. %2%\n")) % debug_name % BUGMSG);
    table[i].id = result->GlobalId;
  }
}

void
xml_element_map_init() {
  static parser_element_t _chapter_elements[] = {
    {"Chapters",              EBMLT_MASTER,  0,  0,            0, no_id,                  NULL, NULL, NULL},

    {"EditionEntry",          EBMLT_MASTER,  1,  0,            0, no_id,                  NULL, NULL, NULL},
    {"EditionUID",            EBMLT_UINT,    2,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"EditionFlagHidden",     EBMLT_BOOL,    2,  0,            0, no_id,                  NULL, NULL, NULL},
    {"EditionFlagOrdered",    EBMLT_BOOL,    2,  0,            0, no_id,                  NULL, NULL, NULL},
    {"EditionFlagDefault",    EBMLT_BOOL,    2,  0,            0, no_id,                  NULL, NULL, NULL},
    {"EditionProcessed",      EBMLT_SKIP,    2,  0,            0, no_id,                  NULL, NULL, NULL},

    {"ChapterAtom",           EBMLT_MASTER,  2,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterUID",            EBMLT_UINT,    3,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"ChapterTimeStart",      EBMLT_TIME,    3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterTimeEnd",        EBMLT_TIME,    3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterFlagHidden",     EBMLT_BOOL,    3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterFlagEnabled",    EBMLT_BOOL,    3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterPhysicalEquiv",  EBMLT_UINT,    3,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"ChapterSegmentUID",     EBMLT_BINARY,  3, 16,           16, no_id,                  NULL, NULL, NULL},

    {"ChapterProcess",        EBMLT_MASTER,  3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterProcessCodecID", EBMLT_UINT,    4,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"ChapterProcessPrivate", EBMLT_BINARY,  4,  0,            0, no_id,                  NULL, NULL, NULL},

    {"ChapterProcessCommand", EBMLT_MASTER,  4,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterProcessTime",    EBMLT_UINT,    5,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterProcessData",    EBMLT_BINARY,  5,  0,            0, no_id,                  NULL, NULL, NULL},

    {"ChapterTrack",          EBMLT_MASTER,  3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterTrackNumber",    EBMLT_UINT,    4,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},

    {"ChapterDisplay",        EBMLT_MASTER,  3,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterString",         EBMLT_USTRING, 4,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterLanguage",       EBMLT_STRING,  4,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterCountry",        EBMLT_STRING,  4,  0,            0, no_id,                  NULL, NULL, NULL},

    {NULL,                    EBMLT_MASTER,  0,  0,            0, EbmlId((uint32)0, 0), NULL, NULL, NULL}
  };

  static parser_element_t _tag_elements[] = {
    {"Tags",            EBMLT_MASTER,  0, 0,            0, no_id,                  NULL, NULL, NULL},

    {"Tag",             EBMLT_MASTER,  1, 0,            0, no_id,                  NULL, NULL, NULL},

    {"Targets",         EBMLT_MASTER,  2, 0,            0, no_id,                  NULL, NULL, "TagTargets"},
    {"TrackUID",        EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                  NULL, NULL, "TagTrackUID"},
    {"EditionUID",      EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                  NULL, NULL, "TagEditionUID"},
    {"ChapterUID",      EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                  NULL, NULL, "TagChapterUID"},
    {"AttachmentUID",   EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                  NULL, NULL, "TagAttachmentUID"},
    {"TargetType",      EBMLT_STRING,  3, 0,            0, no_id,                  NULL, NULL, "TagTargetType"},
    {"TargetTypeValue", EBMLT_UINT,    3, 0, NO_MAX_VALUE, no_id,                  NULL, NULL, "TagTargetTypeValue"},

    {"Simple",          EBMLT_MASTER,  2, 0,            0, no_id,                  NULL, NULL, "TagSimple"},
    {"Name",            EBMLT_USTRING, 3, 0,            0, no_id,                  NULL, NULL, "TagName"},
    {"String",          EBMLT_USTRING, 3, 0,            0, no_id,                  NULL, NULL, "TagString"},
    {"Binary",          EBMLT_BINARY,  3, 0,            0, no_id,                  NULL, NULL, "TagBinary"},
    {"TagLanguage"    , EBMLT_STRING,  3, 0,            0, no_id,                  NULL, NULL, "TagLanguage"},
    {"DefaultLanguage", EBMLT_BOOL,    3, 0,            1, no_id,                  NULL, NULL, "TagDefault"},

    {NULL,              EBMLT_MASTER,  0, 0,            0, EbmlId((uint32)0, 0), NULL, NULL, NULL}
  };

  static parser_element_t _segmentinfo_elements[] = {
    {"Info",                       EBMLT_MASTER, 0,  0,            0, no_id,                  NULL, NULL, NULL},

    {"SegmentUID",                 EBMLT_BINARY, 1, 16,           16, no_id,                  NULL, NULL, NULL},
    {"NextSegmentUID",             EBMLT_BINARY, 1, 16,           16, no_id,                  NULL, NULL, "NextUID"},
    {"PreviousSegmentUID",         EBMLT_BINARY, 1, 16,           16, no_id,                  NULL, NULL, "PrevUID"},

    {"SegmentFamily",              EBMLT_BINARY, 1,  0,            0, no_id,                  NULL, NULL, NULL},

    {"ChapterTranslate",           EBMLT_MASTER, 1,  0,            0, no_id,                  NULL, NULL, NULL},
    {"ChapterTranslateEditionUID", EBMLT_UINT,   2,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"ChapterTranslateCodec",      EBMLT_UINT,   2,  0, NO_MAX_VALUE, no_id,                  NULL, NULL, NULL},
    {"ChapterTranslateID",         EBMLT_BINARY, 2,  0,            0, no_id,                  NULL, NULL, NULL},

    {NULL,                         EBMLT_MASTER, 0,  0,            0, EbmlId((uint32)0, 0), NULL, NULL, NULL}
  };

  chapter_elements = _chapter_elements;
  tag_elements = _tag_elements;
  segmentinfo_elements = _segmentinfo_elements;
  init_mapping_table(chapter_elements);
  init_mapping_table(tag_elements);
  init_mapping_table(segmentinfo_elements);
}

int
xml_element_map_index(const parser_element_t *element_map,
                      const char *name) {
  int i;

  for (i = 0; element_map[i].name != NULL; i++)
    if (!strcmp(name, element_map[i].name))
      return i;

  return -1;
}
