/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Mapping from XML elements to EBML elements

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ebml/EbmlElement.h>
#include <matroska/KaxConfig.h>
#include <matroska/KaxSegment.h>

#include "common.h"
#include "commonebml.h"
#include "xml_element_mapping.h"

using namespace libmatroska;

static EbmlId no_id((uint32_t)0, 0);

parser_element_t *chapter_elements = NULL;
parser_element_t *tag_elements = NULL;

static void
init_mapping_table(parser_element_t *table) {
  const char *debug_name;
  int i;

  for (i = 0; table[i].name != NULL; i++) {
    debug_name = table[i].debug_name != NULL ? table[i].debug_name :
      table[i].name;
    try {
      table[i].id =
        find_ebml_callbacks(KaxSegment::ClassInfos, debug_name).GlobalId;
    } catch (...) {
      mxerror("Error initializing the tables for the chapter and tag "
              "elements: Could not find the element with the debug name "
              "'%s'. %s\n", debug_name, BUGMSG);
    }
  }
}

void
xml_element_map_init() {
  static parser_element_t _chapter_elements[] = {
    {"Chapters", EBMLT_MASTER, 0, 0, 0, no_id, NULL, NULL, NULL},

    {"EditionEntry", EBMLT_MASTER, 1, 0, 0, no_id, NULL, NULL, NULL},
    {"EditionUID", EBMLT_UINT, 2, 0, NO_MAX_VALUE, no_id, NULL, NULL, NULL},
    {"EditionFlagHidden", EBMLT_BOOL, 2, 0, 0, no_id, NULL, NULL, NULL},
    {"EditionFlagOrdered", EBMLT_BOOL, 2, 0, 0, no_id, NULL, NULL, NULL},
    {"EditionFlagDefault", EBMLT_BOOL, 2, 0, 0, no_id, NULL, NULL, NULL},

    {"ChapterAtom", EBMLT_MASTER, 2, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterUID", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL, NULL},
    {"ChapterTimeStart", EBMLT_TIME, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterTimeEnd", EBMLT_TIME, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterFlagHidden", EBMLT_BOOL, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterFlagEnabled", EBMLT_BOOL, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterPhysicalEquiv", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL,
     NULL, NULL},

    {"ChapterProcess", EBMLT_MASTER, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterProcessCodecID", EBMLT_UINT, 4, 0, NO_MAX_VALUE, no_id, NULL,
     NULL, NULL},
    {"ChapterProcessPrivate", EBMLT_BINARY, 4, 0, 0, no_id, NULL, NULL,
     NULL},

    {"ChapterProcessCommand", EBMLT_MASTER, 4, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterProcessTime", EBMLT_UINT, 5, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterProcessData", EBMLT_BINARY, 5, 0, 0, no_id, NULL, NULL, NULL},

    {"ChapterTrack", EBMLT_MASTER, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterTrackNumber", EBMLT_UINT, 4, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     NULL},

    {"ChapterDisplay", EBMLT_MASTER, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterString", EBMLT_USTRING, 4, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterLanguage", EBMLT_STRING, 4, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterCountry", EBMLT_STRING, 4, 0, 0, no_id, NULL, NULL, NULL},

    {NULL, EBMLT_MASTER, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL, NULL}
  };

  static parser_element_t _tag_elements[] = {
    {"Tags", EBMLT_MASTER, 0, 0, 0, no_id, NULL, NULL, NULL},

    {"Tag", EBMLT_MASTER, 1, 0, 0, no_id, NULL, NULL, NULL},

    {"Targets", EBMLT_MASTER, 2, 0, 0, no_id, NULL, NULL, "TagTargets"},
    {"TrackUID", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagTrackUID"},
    {"EditionUID", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagEditionUID"},
    {"ChapterUID", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagChapterUID"},
    {"AttachmentUID", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagAttachmentUID"},
    {"TargetType", EBMLT_STRING, 3, 0, 0, no_id, NULL, NULL, "TagTargetType"},
    {"TargetTypeValue", EBMLT_UINT, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagTargetTypeValue"},

    {"Simple", EBMLT_MASTER, 2, 0, 0, no_id, NULL, NULL, "TagSimple"},
    {"Name", EBMLT_USTRING, 3, 0, 0,  no_id, NULL, NULL, "TagName"},
    {"String", EBMLT_USTRING, 3, 0, 0, no_id, NULL, NULL, "TagString"},
    {"Binary", EBMLT_BINARY, 3, 0, 0, no_id, NULL, NULL, "TagBinary"},
    {"TagLanguage", EBMLT_STRING, 3, 0, 0, no_id, NULL, NULL, "TagLanguage"},
    {"DefaultLanguage", EBMLT_BOOL, 3, 0, 1, no_id, NULL, NULL, "TagDefault"},

    {NULL, EBMLT_MASTER, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL, NULL}
  };

  chapter_elements = _chapter_elements;
  tag_elements = _tag_elements;
  init_mapping_table(chapter_elements);
  init_mapping_table(tag_elements);
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

