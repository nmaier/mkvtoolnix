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
 * general XML element parser, definitions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __XML_ELEMENT_PARSER_H
#define __XML_ELEMENT_PARSER_H

#include <expat.h>
#include <setjmp.h>

#include <string>
#include <vector>

#include "xml_element_mapping.h"

namespace libebml {
  class EbmlElement;
  class EbmlMaster;
};

class mm_text_io_c;

using namespace std;
using namespace libebml;

typedef struct {
  XML_Parser parser;

  const char *file_name;
  const char *parser_name;
  const parser_element_t *mapping;

  int depth;
  bool done_reading, data_allowed;

  string *bin;

  vector<EbmlElement *> *parents;
  vector<int> *parent_idxs;

  EbmlMaster *root_element;

  jmp_buf parse_error_jmp;
  string *parse_error_msg;
} parser_data_t;

#define xmlp_pelt (*((parser_data_t *)pdata)->parents) \
                     [((parser_data_t *)pdata)->parents->size() - 1]
#define xmlp_pname xmlp_parent_name((parser_data_t *)pdata, xmlp_pelt)

EbmlMaster * MTX_DLL_API
parse_xml_elements(const char *parser_name, const parser_element_t *mapping,
                   mm_text_io_c *in);

const char * MTX_DLL_API
xmlp_parent_name(parser_data_t *pdata, EbmlElement *e);

void MTX_DLL_API
xmlp_error(parser_data_t *pdata, const char *fmt, ...);


#endif
