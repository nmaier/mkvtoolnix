/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   XML chapter writer functions
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlMaster.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlDate.h>

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "mm_io.h"
#include "xml_element_writer.h"

using namespace libebml;

static void
print_binary(int level,
             const char *name,
             EbmlElement *e,
             mm_io_c *out) {
  EbmlBinary *b;
  const unsigned char *p;
  string s;
  int i, size;

  b = (EbmlBinary *)e;
  p = (const unsigned char *)b->GetBuffer();
  size = b->GetSize();

  for (i = 0; i < size; i++) {
    if ((i % 16) != 0)
      s += " ";
    s += mxsprintf("%02x", *p);
    ++p;
    if ((((i + 1) % 16) == 0) && ((i + 1) < size))
      s += mxsprintf("\n%*s", (level + 1) * 2, "");
  }

  if ((level * 2 + 2 * strlen(name) + 2 + 3 + s.length()) <= 78)
    out->printf("%s</%s>\n", s.c_str(), name);
  else
    out->printf("\n%*s%s\n%*s</%s>\n", (level + 1) * 2, "", s.c_str(),
                level * 2, "", name);
}

void
write_xml_element_rec(int level,
                      int parent_idx,
                      EbmlElement *e,
                      mm_io_c *out,
                      const parser_element_t *element_map) {
  EbmlMaster *m;
  int elt_idx, i;
  bool found;
  string s;

  elt_idx = parent_idx;
  found = false;
  while ((element_map[elt_idx].name != NULL) &&
         (element_map[elt_idx].level >=
          element_map[parent_idx].level)) {
    if (element_map[elt_idx].id == e->Generic().GlobalId) {
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

  out->printf("<%s%s>", element_map[elt_idx].name,
              element_map[elt_idx].type == EBMLT_BINARY ?
              " format=\"hex\"" : "");
  switch (element_map[elt_idx].type) {
    case EBMLT_MASTER:
      out->printf("\n");
      m = dynamic_cast<EbmlMaster *>(e);
      assert(m != NULL);
      for (i = 0; i < m->ListSize(); i++)
        write_xml_element_rec(level + 1, elt_idx, (*m)[i], out, element_map);

      if (element_map[elt_idx].end_hook != NULL) {
        xml_writer_cb_t cb;

        cb.level = level;
        cb.parent_idx = parent_idx;
        cb.elt_idx = elt_idx;
        cb.e = e;
        cb.out = out;

        element_map[elt_idx].end_hook(&cb);
      }

      for (i = 0; i < level; i++)
        out->printf("  ");
      out->printf("</%s>\n", element_map[elt_idx].name);
      break;

    case EBMLT_UINT:
    case EBMLT_BOOL:
      out->printf("%llu</%s>\n", uint64(*dynamic_cast<EbmlUInteger *>(e)),
                  element_map[elt_idx].name);
      break;

    case EBMLT_STRING:
      s = escape_xml(string(*dynamic_cast<EbmlString *>(e)));
      out->printf("%s</%s>\n", s.c_str(), element_map[elt_idx].name);
      break;

    case EBMLT_USTRING:
      s = UTFstring_to_cstrutf8(UTFstring(*static_cast
                                          <EbmlUnicodeString *>(e)).c_str());
      s = escape_xml(s);
      out->printf("%s</%s>\n", s.c_str(), element_map[elt_idx].name);
      break;

    case EBMLT_TIME:
      out->printf(FMT_TIMECODEN "</%s>\n", 
                  ARG_TIMECODEN(uint64(*dynamic_cast<EbmlUInteger *>(e))),
                  element_map[elt_idx].name);
      break;

    case EBMLT_BINARY:
      print_binary(level, element_map[elt_idx].name, e, out);
      break;

    default:
      assert(false);
  }
}

