/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   XML tag parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "common.h"
#include "commonebml.h"
#include "error.h"
#include "mm_io.h"
#include "tagparser.h"
#include "xml_element_mapping.h"
#include "xml_element_parser.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace std;
using namespace libebml;
using namespace libmatroska;

static int
tet_index(const char *name) {
  int i;

  for (i = 0; tag_elements[i].name != NULL; i++)
    if (!strcmp(name, tag_elements[i].name))
      return i;

  mxerror("tet_index: '%s' not found\n", name);
  return -1;
}

static void
end_simple_tag(void *pdata) {
  KaxTagSimple *simple;

  simple = dynamic_cast<KaxTagSimple *>(xmlp_pelt);
  assert(simple != NULL);
  if ((FINDFIRST(simple, KaxTagString) != NULL) &&
      (FINDFIRST(simple, KaxTagBinary) != NULL))
    xmlp_error(CPDATA, "Only one of <String> and <Binary> may be used beneath "
               "<Simple> but not both at the same time.");
}

void
parse_xml_tags(const string &name,
               KaxTags *tags) {
  KaxTags *new_tags;
  EbmlMaster *m;
  mm_text_io_c *in;
  int i;

  in = NULL;
  try {
    in = new mm_text_io_c(new mm_file_io_c(name));
  } catch(...) {
    mxerror("Could not open '%s' for reading.\n", name.c_str());
  }

  try {
    for (i = 0; NULL != tag_elements[i].name; ++i) {
      tag_elements[i].start_hook = NULL;
      tag_elements[i].end_hook = NULL;
    }

    tag_elements[tet_index("Simple")].end_hook =
      end_simple_tag;

    m = parse_xml_elements("Tag", tag_elements, in);
    if (m != NULL) {
      new_tags = dynamic_cast<KaxTags *>(sort_ebml_master(m));
      assert(new_tags != NULL);
      while (new_tags->ListSize() > 0) {
        tags->PushElement(*(*new_tags)[0]);
        new_tags->Remove(0);
      }
      delete new_tags;
    }
  } catch (error_c e) {
    mxerror("%s", e.get_error().c_str());
  }

  delete in;
}
