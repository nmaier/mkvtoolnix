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

#include "ebml/EbmlElement.h"

#include "xml_element_mapping.h"

namespace libmatroska {
  extern EbmlId KaxChapters_TheId;
  extern EbmlId KaxEditionEntry_TheId;
  extern EbmlId KaxEditionUID_TheId;
  extern EbmlId KaxEditionFlagHidden_TheId;
  extern EbmlId KaxEditionFlagDefault_TheId;
  extern EbmlId KaxEditionManaged_TheId;
  extern EbmlId KaxChapterAtom_TheId;
  extern EbmlId KaxChapterUID_TheId;
  extern EbmlId KaxChapterTimeStart_TheId;
  extern EbmlId KaxChapterTimeEnd_TheId;
  extern EbmlId KaxChapterFlagHidden_TheId;
  extern EbmlId KaxChapterFlagEnabled_TheId;
  extern EbmlId KaxChapterPhysicalEquiv_TheId;
  extern EbmlId KaxChapterTrack_TheId;
  extern EbmlId KaxChapterTrackNumber_TheId;
  extern EbmlId KaxChapterDisplay_TheId;
  extern EbmlId KaxChapterString_TheId;
  extern EbmlId KaxChapterLanguage_TheId;
  extern EbmlId KaxChapterCountry_TheId;
  extern EbmlId KaxChapterProcess_TheId;
  extern EbmlId KaxChapterProcessTime_TheId;

  extern EbmlId KaxTags_TheId;
  extern EbmlId KaxTag_TheId;
  extern EbmlId KaxTagTargets_TheId;
  extern EbmlId KaxTagSimple_TheId;
  extern EbmlId KaxTagName_TheId;
  extern EbmlId KaxTagLangue_TheId;
  extern EbmlId KaxTagDefault_TheId;
  extern EbmlId KaxTagString_TheId;
  extern EbmlId KaxTagBinary_TheId;
  extern EbmlId KaxTagTrackUID_TheId;
  extern EbmlId KaxTagEditionUID_TheId;
  extern EbmlId KaxTagChapterUID_TheId;
  extern EbmlId KaxTagAttachmentUID_TheId;
  extern EbmlId KaxTagTargetType_TheId;
  extern EbmlId KaxTagTargetTypeValue_TheId;
};

using namespace libmatroska;

parser_element_t chapter_elements[] = {
  {"Chapters", ebmlt_master, 0, 0, 0, KaxChapters_TheId, NULL, NULL},

  {"EditionEntry", ebmlt_master, 1, 0, 0, KaxEditionEntry_TheId, NULL, NULL},
  {"EditionUID", ebmlt_uint, 2, 0, NO_MAX_VALUE, KaxEditionUID_TheId, NULL,
   NULL},
  {"EditionFlagHidden", ebmlt_bool, 2, 0, 0, KaxEditionFlagHidden_TheId,
   NULL, NULL},
  {"EditionManaged", ebmlt_uint, 2, 0, NO_MAX_VALUE, KaxEditionManaged_TheId,
   NULL, NULL},
  {"EditionFlagDefault", ebmlt_bool, 2, 0, 0, KaxEditionFlagDefault_TheId,
   NULL, NULL},

  {"ChapterAtom", ebmlt_master, 2, 0, 0, KaxChapterAtom_TheId, NULL,
   NULL},
  {"ChapterUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxChapterUID_TheId, NULL,
   NULL},
  {"ChapterTimeStart", ebmlt_time, 3, 0, 0, KaxChapterTimeStart_TheId, NULL,
   NULL},
  {"ChapterTimeEnd", ebmlt_time, 3, 0, 0, KaxChapterTimeEnd_TheId, NULL,
   NULL},
  {"ChapterFlagHidden", ebmlt_bool, 3, 0, 0, KaxChapterFlagHidden_TheId,
   NULL, NULL},
  {"ChapterFlagEnabled", ebmlt_bool, 3, 0, 0, KaxChapterFlagEnabled_TheId,
   NULL, NULL},

  {"ChapterTrack", ebmlt_master, 3, 0, 0, KaxChapterTrack_TheId,
   NULL, NULL},
  {"ChapterTrackNumber", ebmlt_uint, 4, 0, NO_MAX_VALUE,
   KaxChapterTrackNumber_TheId, NULL, NULL},

  {"ChapterDisplay", ebmlt_master, 3, 0, 0, KaxChapterDisplay_TheId,
   NULL, NULL},
  {"ChapterString", ebmlt_ustring, 4, 0, 0, KaxChapterString_TheId,
   NULL, NULL},
  {"ChapterLanguage", ebmlt_string, 4, 0, 0, KaxChapterLanguage_TheId,
   NULL, NULL},
  {"ChapterCountry", ebmlt_string, 4, 0, 0, KaxChapterCountry_TheId,
   NULL, NULL},

  {NULL, ebmlt_master, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL}
};

parser_element_t tag_elements[] = {
  {"Tags", ebmlt_master, 0, 0, 0, KaxTags_TheId, NULL, NULL},

  {"Tag", ebmlt_master, 1, 0, 0, KaxTag_TheId, NULL, NULL},

  {"Targets", ebmlt_master, 2, 0, 0, KaxTagTargets_TheId, NULL, NULL},
  {"TrackUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxTagTrackUID_TheId,
   NULL, NULL},
  {"EditionUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxTagEditionUID_TheId,
   NULL, NULL},
  {"ChapterUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxTagChapterUID_TheId,
   NULL, NULL},
  {"AttachmentUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxTagAttachmentUID_TheId,
   NULL, NULL},
  {"TargetType", ebmlt_string, 3, 0, 0, KaxTagTargetType_TheId, NULL, NULL},
  {"TargetTypeValue", ebmlt_uint, 3, 0, NO_MAX_VALUE,
   KaxTagTargetTypeValue_TheId, NULL, NULL},

  {"Simple", ebmlt_master, 2, 0, 0, KaxTagSimple_TheId, NULL, NULL},
  {"Name", ebmlt_ustring, 3, 0, 0,  KaxTagName_TheId, NULL, NULL},
  {"String", ebmlt_ustring, 3, 0, 0, KaxTagString_TheId, NULL, NULL},
  {"Binary", ebmlt_binary, 3, 0, 0, KaxTagBinary_TheId, NULL, NULL},
  {"TagLanguage", ebmlt_string, 3, 0, 0, KaxTagLangue_TheId, NULL, NULL},
  {"DefaultLanguage", ebmlt_bool, 3, 0, 1, KaxTagDefault_TheId, NULL, NULL},

  {NULL, ebmlt_master, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL}
};

int
xml_element_map_index(const parser_element_t *element_map,
                      const char *name) {
  int i;

  for (i = 0; element_map[i].name != NULL; i++)
    if (!strcmp(name, element_map[i].name))
      return i;

  return -1;
}

