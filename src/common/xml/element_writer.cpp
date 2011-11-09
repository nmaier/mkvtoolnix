/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML chapter writer functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ebml/EbmlBinary.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlDate.h>

#include "common/base64.h"
#include "common/ebml.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/xml/xml.h"
#include "common/xml/element_writer.h"

using namespace libebml;

static std::string
space(int num) {
  return std::string(num, ' ');
}

static void
print_binary(int level,
             const char *name,
             EbmlElement *e,
             mm_io_c *out) {
  EbmlBinary *b;
  const unsigned char *p;
  std::string s;
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
    out->puts(boost::format("<%1% format=\"ascii\">%2%</%1%>\n") % name % escape_xml(s));

  } else {
    std::string prefix = (boost::format("<%1% format=\"hex\">") % name).str();

    for (i = 0; i < size; i++) {
      if ((i % 16) != 0)
        s += " ";
      s += (boost::format("%|1$02x|") % (int)*p).str();
      ++p;
      if ((((i + 1) % 16) == 0) && ((i + 1) < size))
        s += (boost::format("\n%1%") % space((level + 1) * 2)).str();
    }

    if ((level * 2 + 2 * strlen(name) + 2 + 3 + s.length()) <= 78)
      out->puts(boost::format("%1%%2%</%3%>\n") % prefix % s % name);
    else
      out->puts(boost::format("%1%\n%2%%3%\n%4%</%5%>\n") % prefix % space((level + 1) * 2) % s % space(level * 2) % name);
  }
}

void
write_xml_element_rec(int level,
                      int parent_idx,
                      EbmlElement *e,
                      mm_io_c *out,
                      const parser_element_t *element_map) {
  EbmlMaster *m;
  size_t elt_idx, i;
  bool found;
  std::string s;

  elt_idx = parent_idx;
  found = false;
  while ((element_map[elt_idx].name != NULL) &&
         (element_map[elt_idx].level >=
          element_map[parent_idx].level)) {
    if (element_map[elt_idx].id == EbmlId(*e)) {
      found = true;
      break;
    }
    elt_idx++;
  }

  out->puts(space(level * 2));

  if (!found) {
    out->puts(boost::format(Y("<!-- Unknown element '%1%' -->\n")) % EBML_NAME(e));
    return;
  }

  if (element_map[elt_idx].type != EBMLT_BINARY)
    out->puts(boost::format("<%1%>") % element_map[elt_idx].name);
  switch (element_map[elt_idx].type) {
    case EBMLT_MASTER:
      out->puts("\n");
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

      out->puts(boost::format("%1%</%2%>\n") % space(level * 2) % element_map[elt_idx].name);
      break;

    case EBMLT_UINT:
    case EBMLT_BOOL:
      out->puts(boost::format("%1%</%2%>\n") % uint64(*dynamic_cast<EbmlUInteger *>(e)) % element_map[elt_idx].name);
      break;

    case EBMLT_STRING:
      out->puts(boost::format("%1%</%2%>\n") % escape_xml(std::string(*dynamic_cast<EbmlString *>(e))) % element_map[elt_idx].name);
      break;

    case EBMLT_USTRING:
      s = escape_xml(UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>(e)).c_str()));
      out->puts(boost::format("%1%</%2%>\n") % s % element_map[elt_idx].name);
      break;

    case EBMLT_TIME:
      out->puts(boost::format("%1%</%2%>\n") % format_timecode(uint64(*dynamic_cast<EbmlUInteger *>(e))) % element_map[elt_idx].name);
      break;

    case EBMLT_BINARY:
      print_binary(level, element_map[elt_idx].name, e, out);
      break;

    default:
      assert(false);
  }
}

// ------------------------------------------------------------------------

xml_formatter_c::xml_formatter_c(mm_io_c &out,
                                 const std::string &encoding)
  : m_out(out)
  , m_temp_io(counted_ptr<mm_text_io_c>(new mm_text_io_c(new mm_mem_io_c(NULL, 100000, 4000))))
  , m_encoding(encoding)
  , m_cc_utf8(charset_converter_c::init(m_encoding))
  , m_header_written(false)
  , m_depth(0)
  , m_state(XMLF_STATE_NONE)
{
  m_xml_source = m_temp_io.get_object();
}

xml_formatter_c::~xml_formatter_c() {
}

void
xml_formatter_c::set_doctype(const std::string &dtd,
                             const std::string &file) {
  if (m_header_written)
    throw mtx::xml::formatter_x(Y("The header has already been written."));

  m_dtd = dtd;
  m_dtd_file = file;
}

void
xml_formatter_c::set_stylesheet(const std::string &type,
                                const std::string &file) {
  if (m_header_written)
    throw mtx::xml::formatter_x(Y("The header has already been written."));

  m_stylesheet_type = type;
  m_stylesheet_file = file;
}

void
xml_formatter_c::write_header() {
  if (m_header_written)
    throw mtx::xml::formatter_x(Y("The header has already been written."));

#if defined(SYS_WINDOWS)
  m_out.use_dos_style_newlines(true);
#endif
  m_out.write_bom(m_encoding);
  m_out.puts(boost::format("<?xml version=\"1.0\" encoding=\"%1%\"?>\n") % escape_xml(m_encoding));
  if ((m_dtd != "") && (m_dtd_file != ""))
    m_out.puts(boost::format("\n<!-- DOCTYPE %1% SYSTEM \"%2%\" -->\n") % m_dtd % escape_xml(m_dtd_file));
  if ((m_stylesheet_type != "") && (m_stylesheet_file != ""))
    m_out.puts(boost::format("\n<?xml-stylesheet type=\"%1%\" href=\"%2%\"?>\n") % escape_xml(m_stylesheet_type) % escape_xml(m_stylesheet_file));

  m_header_written = true;
}

void
xml_formatter_c::format(const std::string &text) {
  try {
    m_temp_io->save_pos();
    m_temp_io->write(text.c_str(), text.length());
    m_temp_io->restore_pos();

    while (parse_one_xml_line())
      ;

  } catch (mtx::xml::parser_x &error) {
    throw mtx::xml::formatter_x(boost::format(Y("XML parser error at line %1%: %2%.")) % error.get_line() % error.error());
  }
}

void
xml_formatter_c::flush() {
}

void
xml_formatter_c::start_element_cb(const char *name,
                                  const char **atts) {
  std::string element;

  strip(m_data_buffer, true);
  m_data_buffer = escape_xml(m_cc_utf8->native(m_data_buffer));

  if (XMLF_STATE_START == m_state)
    m_out.puts(">");
  else if (XMLF_STATE_DATA == m_state)
    m_out.puts(m_data_buffer);

  element = create_xml_node_name(name, atts);
  element.erase(element.length() - 1);
  m_out.puts(boost::format("\n%1%%2%") % space(m_depth * 2) % element);

  ++m_depth;
  m_data_buffer = "";
  m_state = XMLF_STATE_START;
}

void
xml_formatter_c::end_element_cb(const char *name) {
  strip(m_data_buffer, true);
  m_data_buffer = escape_xml(m_cc_utf8->native(m_data_buffer));

  --m_depth;

  if (XMLF_STATE_END == m_state)
    m_out.puts(boost::format("\n%1%</%2%>") % space(m_depth * 2) % name);
  else if (XMLF_STATE_START == m_state) {
    if (m_data_buffer == "")
      m_out.puts("/>");
    else
      m_out.puts(boost::format(">\n%1%%2%\n%3%</%4%>") % space((m_depth + 1) * 2) % m_data_buffer % space(m_depth * 2));
  } else if (XMLF_STATE_DATA == m_state)
    m_out.puts(boost::format("%1%\n%2%</%3%>") % m_data_buffer % space(m_depth * 2) % name);

  m_data_buffer = "";
  m_state = XMLF_STATE_END;
}

void
xml_formatter_c::add_data_cb(const XML_Char *s,
                             int len) {
  std::string test_data_buffer;

  m_data_buffer.append((const char *)s, len);
  test_data_buffer = m_data_buffer;
  strip(test_data_buffer, true);
  if (test_data_buffer == "")
    return;

  if ((XMLF_STATE_START == m_state) && (test_data_buffer != ""))
    m_out.puts(boost::format(">\n%1%") % space(m_depth * 2));
  else if (XMLF_STATE_END == m_state)
    m_out.puts(boost::format("\n%1%") % space(m_depth * 2));
  m_state = XMLF_STATE_DATA;
}

void
xml_formatter_c::format_fixed(const std::string &text) {
  if (XMLF_STATE_START == m_state)
    m_out.puts(boost::format(">\n%1%") % space(m_depth * 2));
  else if (XMLF_STATE_END == m_state)
    m_out.puts(boost::format("\n%1%") % space(m_depth * 2));
  m_state = XMLF_STATE_DATA;
  m_out.puts(text);
}
