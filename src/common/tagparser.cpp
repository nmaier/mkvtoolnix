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
 * XML tag parser. Functions for the start tags + helper functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "chapters.h"
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

#define CPDATA (parser_data_t *)pdata

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
parse_xml_tags(const char *name,
               KaxTags *tags) {
  KaxTags *new_tags;
  EbmlMaster *m;
  mm_text_io_c *in;
  int i;

  in = NULL;
  try {
    in = new mm_text_io_c(name);
  } catch(...) {
    mxerror("Could not open '%s' for reading.\n", name);
  }

  for (i = 0; tag_elements[i].name != NULL; i++) {
    tag_elements[i].start_hook = NULL;
    tag_elements[i].end_hook = NULL;
  }
  tag_elements[tag_element_map_index("Simple")].end_hook = end_simple_tag;

  try {
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
    mxerror("%s", e.get_error());
  }

  delete in;
}

void
fix_mandatory_tag_elements(EbmlElement *e) {
  if (dynamic_cast<KaxTagSimple *>(e) != NULL) {
    KaxTagSimple &s = *static_cast<KaxTagSimple *>(e);
    GetChild<KaxTagLangue>(s);
    GetChild<KaxTagDefault>(s);

  } else if (dynamic_cast<KaxTagTargets *>(e) != NULL) {
    KaxTagTargets &t = *static_cast<KaxTagTargets *>(e);
    GetChild<KaxTagTargetTypeValue>(t);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_tag_elements((*m)[i]);
  }
}

KaxTags *
select_tags_for_chapters(KaxTags &tags,
                         KaxChapters &chapters) {
  KaxTags *new_tags;
  KaxTagTargets *targets;
  KaxTagEditionUID *t_euid;
  KaxTagChapterUID *t_cuid;
  int tags_idx, targets_idx;
  bool copy;

  new_tags = NULL;
  for (tags_idx = 0; tags_idx < tags.ListSize(); tags_idx++) {
    if (dynamic_cast<KaxTag *>(tags[tags_idx]) == NULL)
      continue;
    copy = true;
    targets = FINDFIRST(static_cast<EbmlMaster *>(tags[tags_idx]),
                        KaxTagTargets);
    if (targets != NULL) {
      for (targets_idx = 0; targets_idx < targets->ListSize(); targets_idx++) {
        t_euid = dynamic_cast<KaxTagEditionUID *>((*targets)[targets_idx]);
        if ((t_euid != NULL) &&
            (find_edition_with_uid(chapters, uint64(*t_euid)) == NULL)) {
          copy = false;
          break;
        }
        t_cuid = dynamic_cast<KaxTagChapterUID *>((*targets)[targets_idx]);
        if ((t_cuid != NULL) &&
            (find_chapter_with_uid(chapters, uint64(*t_cuid)) == NULL)) {
          copy = false;
          break;
        }
      }
    }

    if (copy) {
      if (new_tags == NULL)
        new_tags = new KaxTags;
      new_tags->PushElement(*(tags[tags_idx]->Clone()));
    }
  }

  return new_tags;
}
