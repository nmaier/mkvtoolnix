/*
 * mkvextract -- extract tracks from Matroska files into other files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * writes tags in XML format
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTagMulti.h>

using namespace libmatroska;
using namespace std;

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mm_io.h"
#include "xml_element_mapping.h"

static void
print_binary(int level,
             const char *name,
             EbmlElement *e,
             mm_io_c *out) {
  EbmlBinary *b;
  string s;
  int i, idx, old_idx;

  b = (EbmlBinary *)e;
  s = base64_encode((const unsigned char *)b->GetBuffer(), b->GetSize(), true,
                    72 - level - 2);
  if (s[s.length() - 1] == '\n')
    s.erase(s.length() - 1);

  if ((level * 2 + 2 * strlen(name) + 2 + 3 + s.length()) <= 78) {
    out->printf("%s</%s>\n", s.c_str(), name);
    return;
  }

  out->printf("\n");

  for (i = 0; i < (level + 2); i++)
    out->printf("  ");

  old_idx = 0;
  for (idx = 0; idx < s.length(); idx++)
    if (s[idx] == '\n') {
      out->printf("%s\n", s.substr(old_idx, idx - old_idx).c_str());
      for (i = 0; i < (level + 2); i++)
        out->printf("  ");
      old_idx = idx + 1;
    }

  if (old_idx < s.length())
    out->printf("%s", s.substr(old_idx).c_str());

  out->printf("\n");

  for (i = 0; i < level; i++)
    out->printf("  ");
  out->printf("</%s>\n", name);
}

typedef struct {
  int level;
  int parent_idx;
  int elt_idx;
  EbmlElement *e;
} tag_writer_cb_t;

static void
write_xml_element_rec(int level,
                      int parent_idx,
                      EbmlElement *e,
                      mm_io_c *out) {
  EbmlMaster *m;
  int elt_idx, i;
  bool found;
  char *s;
  string x;

  elt_idx = parent_idx;
  found = false;
  while ((tag_elements[elt_idx].name != NULL) &&
         (tag_elements[elt_idx].level >=
          tag_elements[parent_idx].level)) {
    if (tag_elements[elt_idx].id == e->Generic().GlobalId) {
      found = true;
      break;
    }
    elt_idx++;
  }

  for (i = 0; i < level; i++)
    out->printf("  ");

  if (!found) {
    out->printf("<!-- Unknown element '%s' -->\n", e->Generic().DebugName);
    return;
  }

  out->printf("<%s>", tag_elements[elt_idx].name);
  switch (tag_elements[elt_idx].type) {
    case ebmlt_master:
      out->printf("\n");
      m = dynamic_cast<EbmlMaster *>(e);
      assert(m != NULL);
      for (i = 0; i < m->ListSize(); i++)
        write_xml_element_rec(level + 1, elt_idx, (*m)[i], out);

      if (tag_elements[elt_idx].end_hook != NULL) {
        tag_writer_cb_t cb;

        cb.level = level;
        cb.parent_idx = parent_idx;
        cb.elt_idx = elt_idx;
        cb.e = e;

        tag_elements[elt_idx].end_hook(&cb);
      }

      for (i = 0; i < level; i++)
        out->printf("  ");
      out->printf("</%s>\n", tag_elements[elt_idx].name);
      break;

    case ebmlt_uint:
    case ebmlt_bool:
      out->printf("%llu</%s>\n", uint64(*dynamic_cast<EbmlUInteger *>(e)),
                  tag_elements[elt_idx].name);
      break;

    case ebmlt_string:
      x = escape_xml(string(*dynamic_cast<EbmlString *>(e)).c_str());
      out->printf("%s</%s>\n", x.c_str(), tag_elements[elt_idx].name);
      break;

    case ebmlt_ustring:
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      x = escape_xml(s);
      out->printf("%s</%s>\n", x.c_str(), tag_elements[elt_idx].name);
      safefree(s);
      break;

    case ebmlt_time:
      out->printf(FMT_TIMECODEN "</%s>\n", 
                  ARG_TIMECODEN(uint64(*dynamic_cast<EbmlUInteger *>(e))),
                  tag_elements[elt_idx].name);
      break;

    case ebmlt_binary:
      print_binary(level, tag_elements[elt_idx].name, e, out);
      break;

    default:
      assert(false);
  }
}

void
write_tags_xml(KaxTags &tags,
               mm_io_c *out) {
  int i;

  for (i = 0; tag_elements[i].name != NULL; i++) {
    tag_elements[i].start_hook = NULL;
    tag_elements[i].end_hook = NULL;
  }

  for (i = 0; i < tags.ListSize(); i++)
    write_xml_element_rec(1, 0, tags[i], out);
}
