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
 * XML to EBML element mapping definitions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __XML_ELEMENT_MAPPING_H
#define __XML_ELEMENT_MAPPING_H

#include "os.h"

#include "ebml/EbmlElement.h"

using namespace libebml;

typedef void (*parser_element_callback_t)(void *pdata);

enum ebml_type_t {ebmlt_master, ebmlt_int, ebmlt_uint, ebmlt_bool,
                  ebmlt_string, ebmlt_ustring, ebmlt_time, ebmlt_binary};

#define NO_MIN_VALUE -9223372036854775807ll-1
#define NO_MAX_VALUE 9223372036854775807ll

typedef struct {
  const char *name;
  ebml_type_t type;
  int level;
  int64_t min_value;
  int64_t max_value;
  const EbmlId id;
  parser_element_callback_t start_hook;
  parser_element_callback_t end_hook;
} parser_element_t;

extern parser_element_t chapter_elements[];
extern parser_element_t tag_elements[];

#define chapter_element_map_index(name) \
   xml_element_map_index(chapter_elements, name)
#define tag_element_map_index(name) \
   xml_element_map_index(tag_elements, name)

int MTX_DLL_API xml_element_map_index(const parser_element_t *element_map,
                                      const char *name);

#endif
