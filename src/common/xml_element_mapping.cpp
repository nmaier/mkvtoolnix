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
 * Mapping from XML elements to EBML elements
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
    {"Chapters", ebmlt_master, 0, 0, 0, no_id, NULL, NULL, NULL},

    {"EditionEntry", ebmlt_master, 1, 0, 0, no_id, NULL, NULL, NULL},
    {"EditionUID", ebmlt_uint, 2, 0, NO_MAX_VALUE, no_id, NULL, NULL, NULL},
    {"EditionFlagHidden", ebmlt_bool, 2, 0, 0, no_id, NULL, NULL, NULL},
    {"EditionProcessed", ebmlt_uint, 2, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     NULL},
    {"EditionFlagDefault", ebmlt_bool, 2, 0, 0, no_id, NULL, NULL, NULL},

    {"ChapterAtom", ebmlt_master, 2, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL, NULL},
    {"ChapterTimeStart", ebmlt_time, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterTimeEnd", ebmlt_time, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterFlagHidden", ebmlt_bool, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterFlagEnabled", ebmlt_bool, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterProcessedPrivate", ebmlt_binary, 3, 0, 0, no_id, NULL, NULL,
     NULL},

    {"ChapterTrack", ebmlt_master, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterTrackNumber", ebmlt_uint, 4, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     NULL},

    {"ChapterDisplay", ebmlt_master, 3, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterString", ebmlt_ustring, 4, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterLanguage", ebmlt_string, 4, 0, 0, no_id, NULL, NULL, NULL},
    {"ChapterCountry", ebmlt_string, 4, 0, 0, no_id, NULL, NULL, NULL},

    {NULL, ebmlt_master, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL, NULL}
  };

  static parser_element_t _tag_elements[] = {
    {"Tags", ebmlt_master, 0, 0, 0, no_id, NULL, NULL, NULL},

    {"Tag", ebmlt_master, 1, 0, 0, no_id, NULL, NULL, NULL},

    {"Targets", ebmlt_master, 2, 0, 0, no_id, NULL, NULL, "TagTargets"},
    {"TrackUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagTrackUID"},
    {"EditionUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagEditionUID"},
    {"ChapterUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagChapterUID"},
    {"AttachmentUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagAttachmentUID"},
    {"TargetType", ebmlt_string, 3, 0, 0, no_id, NULL, NULL, "TagTargetType"},
    {"TargetTypeValue", ebmlt_uint, 3, 0, NO_MAX_VALUE, no_id, NULL, NULL,
     "TagTargetTypeValue"},

    {"Simple", ebmlt_master, 2, 0, 0, no_id, NULL, NULL, "TagSimple"},
    {"Name", ebmlt_ustring, 3, 0, 0,  no_id, NULL, NULL, "TagName"},
    {"String", ebmlt_ustring, 3, 0, 0, no_id, NULL, NULL, "TagString"},
    {"Binary", ebmlt_binary, 3, 0, 0, no_id, NULL, NULL, "TagBinary"},
    {"TagLanguage", ebmlt_string, 3, 0, 0, no_id, NULL, NULL, "TagLanguage"},
    {"DefaultLanguage", ebmlt_bool, 3, 0, 1, no_id, NULL, NULL, "TagDefault"},

    {NULL, ebmlt_master, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL, NULL}
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

