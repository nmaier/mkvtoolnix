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
  bool ascii_only;

  b = (EbmlBinary *)e;
  p = (const unsigned char *)b->GetBuffer();
  size = b->GetSize();

  ascii_only = true;
  for (i = 0; i < size; i++)
    if ((p[i] != '\n') && (p[i] != '\r') && ((p[i] < ' ') || (p[i] >= 127))) {
      ascii_only = false;
      break;
    }

  if (ascii_only) {
    s.append((const char *)p, size);
    out->printf("<%s format=\"ascii\">%s</%s>\n", name, escape_xml(s).c_str(),
                name);

  } else {
    string prefix;

    prefix = mxsprintf("<%s format=\"hex\">", name);

    for (i = 0; i < size; i++) {
      if ((i % 16) != 0)
        s += " ";
      s += mxsprintf("%02x", *p);
      ++p;
      if ((((i + 1) % 16) == 0) && ((i + 1) < size))
        s += mxsprintf("\n%*s", (level + 1) * 2, "");
    }

    if ((level * 2 + 2 * strlen(name) + 2 + 3 + s.length()) <= 78)
      out->printf("%s%s</%s>\n", prefix.c_str(), s.c_str(), name);
    else
      out->printf("%s\n%*s%s\n%*s</%s>\n", prefix.c_str(), (level + 1) * 2, "",
                  s.c_str(), level * 2, "", name);
  }
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

  out->printf("%*s", level * 2, "");

  if (!found) {
    out->printf("<!-- Unknown element '%s' -->\n", e->Generic().DebugName);
    return;
  }

  if (element_map[elt_idx].type != EBMLT_BINARY)
    out->printf("<%s>", element_map[elt_idx].name);
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

      out->printf("%*s</%s>\n", level * 2, "", element_map[elt_idx].name);
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

// ------------------------------------------------------------------------

static void
xml_formatter_start_cb(void *user_data,
                       const char *name,
                       const char **atts) {
  ((xml_formatter_c *)user_data)->start_element_cb(name, atts);
}

static void
xml_formatter_end_cb(void *user_data,
                     const char *name) {
  ((xml_formatter_c *)user_data)->end_element_cb(name);
}

static void
xml_formatter_add_data_cb(void *user_data,
                          const XML_Char *s,
                          int len) {
  ((xml_formatter_c *)user_data)->add_data_cb(s, len);
}

xml_formatter_c::xml_formatter_c(mm_io_c *out,
                                 const string &encoding):
  m_out(out), m_encoding(encoding), m_cc_utf8(0), m_header_written(false),
  m_parser(NULL), m_state(XMLF_STATE_NONE) {

  m_cc_utf8 = utf8_init(m_encoding);

  m_parser = XML_ParserCreate(NULL);
  XML_SetUserData(m_parser, this);
  XML_SetElementHandler(m_parser, xml_formatter_start_cb,
                        xml_formatter_end_cb);
  XML_SetCharacterDataHandler(m_parser, xml_formatter_add_data_cb);
}

xml_formatter_c::~xml_formatter_c() {
  if (NULL != m_parser)
    XML_ParserFree(m_parser);
}

void
xml_formatter_c::set_doctype(const string &dtd,
                             const string &file) {
  if (m_header_written)
    throw xml_formatter_error_c("The header has already been written.");

  m_dtd = dtd;
  m_dtd_file = file;
}

void
xml_formatter_c::set_stylesheet(const string &type,
                                const string &file) {
  if (m_header_written)
    throw xml_formatter_error_c("The header has already been written.");

  m_stylesheet_type = type;
  m_stylesheet_file = file;
}

void
xml_formatter_c::write_header() {
  if (m_header_written)
    throw xml_formatter_error_c("The header has already been written.");

#if defined(SYS_WINDOWS)
  m_out->use_dos_style_newlines(true);
#endif
  m_out->write_bom(m_encoding);
  m_out->printf("<?xml version=\"1.0\" encoding=\"%s\"?>\n",
                escape_xml(m_encoding, true).c_str());
  if ((m_dtd != "") && (m_dtd_file != ""))
    m_out->printf("\n<!-- DOCTYPE %s SYSTEM \"%s\" -->\n", m_dtd.c_str(),
                  escape_xml(m_dtd_file, true).c_str());
  if ((m_stylesheet_type != "") && (m_stylesheet_file != ""))
    m_out->printf("\n<?xml-stylesheet type=\"%s\" href=\"%s\"?>\n",
                  escape_xml(m_stylesheet_type, true).c_str(),
                  escape_xml(m_stylesheet_file, true).c_str());

  m_header_written = true;
}

void
xml_formatter_c::format(const string &text) {
  if (XML_Parse(m_parser, text.c_str(), text.length(), 0) == 0) {
    XML_Error xerror;

    xerror = XML_GetErrorCode(m_parser);
    throw xml_formatter_error_c(mxsprintf("XML parser error at line %d: %s.",
                                          XML_GetCurrentLineNumber(m_parser),
                                          XML_ErrorString(xerror)));
  }
}

void
xml_formatter_c::flush() {
}

void
xml_formatter_c::start_element_cb(const char *name,
                                  const char **atts) {
  string element;

  strip(m_data_buffer, true);
  m_data_buffer = escape_xml(from_utf8(m_cc_utf8, m_data_buffer));

  if (XMLF_STATE_START == m_state)
    m_out->printf(">");
  else if (XMLF_STATE_DATA == m_state)
    m_out->printf("%s", m_data_buffer.c_str());

  element = create_xml_node_name(name, atts);
  element.erase(element.length() - 1);
  m_out->printf("\n%*s%s", m_depth * 2, "", element.c_str());

  ++m_depth;
  m_data_buffer = "";
  m_state = XMLF_STATE_START;
}

void
xml_formatter_c::end_element_cb(const char *name) {
  strip(m_data_buffer, true);
  m_data_buffer = escape_xml(from_utf8(m_cc_utf8, m_data_buffer));

  --m_depth;

  if (XMLF_STATE_END == m_state)
    m_out->printf("\n%*s</%s>", m_depth * 2, "", name);
  else if (XMLF_STATE_START == m_state) {
    if (m_data_buffer == "")
      m_out->printf("/>");
    else
      m_out->printf(">\n%*s%s\n%*s</%s>", (m_depth + 1) * 2, "",
                    m_data_buffer.c_str(), m_depth * 2, "");
  } else if (XMLF_STATE_DATA == m_state)
    m_out->printf("%s\n%*s</%s>", m_data_buffer.c_str(), m_depth * 2, "",
                  name);

  m_data_buffer = "";
  m_state = XMLF_STATE_END;
}

void
xml_formatter_c::add_data_cb(const XML_Char *s,
                             int len) {
  string test_data_buffer;

  m_data_buffer.append((const char *)s, len);
  test_data_buffer = m_data_buffer;
  strip(test_data_buffer, true);
  if (test_data_buffer == "")
    return;

  if ((XMLF_STATE_START == m_state) && (test_data_buffer != ""))
    m_out->printf(">\n%*s", m_depth * 2, "");
  else if (XMLF_STATE_END == m_state)
    m_out->printf("\n%*s", m_depth * 2, "");
  m_state = XMLF_STATE_DATA;
}

