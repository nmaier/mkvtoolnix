/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML tag parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ebml.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "common/tags/parser.h"
#include "common/xml/element_mapping.h"
#include "common/xml/element_parser.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace libebml;
using namespace libmatroska;

static int
tet_index(const char *name) {
  int i;

  for (i = 0; NULL != tag_elements[i].name; i++)
    if (!strcmp(name, tag_elements[i].name))
      return i;

  mxerror(boost::format(Y("tet_index: '%1%' not found\n")) % name);

  return -1;
}

static void
end_simple_tag(void *pdata) {
  KaxTagSimple *simple = dynamic_cast<KaxTagSimple *>(xmlp_pelt);
  assert(NULL != simple);

  if ((FINDFIRST(simple, KaxTagString) != NULL) && (FINDFIRST(simple, KaxTagBinary) != NULL))
    xmlp_error(CPDATA, Y("Only one of <String> and <Binary> may be used beneath <Simple> but not both at the same time."));
}

void
parse_xml_tags(const std::string &name,
               KaxTags *tags) {
  mm_text_io_c *in = NULL;
  try {
    in = new mm_text_io_c(new mm_file_io_c(name));
  } catch(...) {
    mxerror(boost::format(Y("Could not open '%1%' for reading.\n")) % name);
  }

  try {
    int i;
    for (i = 0; NULL != tag_elements[i].name; ++i) {
      tag_elements[i].start_hook = NULL;
      tag_elements[i].end_hook   = NULL;
    }

    tag_elements[tet_index("Simple")].end_hook = end_simple_tag;

    EbmlMaster *m = parse_xml_elements("Tag", tag_elements, in);
    if (NULL != m) {
      KaxTags *new_tags = dynamic_cast<KaxTags *>(sort_ebml_master(m));
      assert(NULL != new_tags);

      while (new_tags->ListSize() > 0) {
        tags->PushElement(*(*new_tags)[0]);
        new_tags->Remove(0);
      }

      delete new_tags;
    }
  } catch (mtx::exception &e) {
    mxerror(e.error());
  }

  delete in;
}
