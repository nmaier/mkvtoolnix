/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML to EBML element mapping definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __MTX_COMMON_XML_ELEMENT_MAPPING_H
#define __MTX_COMMON_XML_ELEMENT_MAPPING_H

#include "common/common_pch.h"

#include <ebml/EbmlElement.h>

using namespace libebml;

typedef void (*parser_element_callback_t)(void *pdata);

enum ebml_type_e {EBMLT_MASTER, EBMLT_INT, EBMLT_UINT, EBMLT_BOOL, EBMLT_STRING, EBMLT_USTRING, EBMLT_TIME, EBMLT_BINARY, EBMLT_FLOAT, EBMLT_SKIP};

#define NO_MIN_VALUE -9223372036854775807ll-1
#define NO_MAX_VALUE  9223372036854775807ll

typedef struct {
  const char *name;
  ebml_type_e type;
  int level;
  int64_t min_value;
  int64_t max_value;
  EbmlId id;
  parser_element_callback_t start_hook;
  parser_element_callback_t end_hook;
  const char *debug_name;
} parser_element_t;

extern parser_element_t *chapter_elements;
extern parser_element_t *tag_elements;
extern parser_element_t *segmentinfo_elements;

#define chapter_element_map_index(name)     xml_element_map_index(chapter_elements,     name)
#define tag_element_map_index(name)         xml_element_map_index(tag_elements,         name)
#define segmentinfo_element_map_index(name) xml_element_map_index(segmentinfo_elements, name)

int xml_element_map_index(const parser_element_t *element_map, const char *name);
void xml_element_map_init();

#endif
