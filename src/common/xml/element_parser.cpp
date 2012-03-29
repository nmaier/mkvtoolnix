/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML element parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cctype>

#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlBinary.h>

#include <matroska/KaxSegment.h>

#include "common/base64.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/xml/element_parser.h"

using namespace libebml;
using namespace libmatroska;

static void
xml_parser_start_element_cb(void *user_data,
                            const char *name,
                            const char **atts) {
  static_cast<xml_parser_c *>(user_data)->start_element_cb(name, atts);
}

static void
xml_parser_end_element_cb(void *user_data,
                          const char *name) {
  static_cast<xml_parser_c *>(user_data)->end_element_cb(name);
}

static void
xml_parser_add_data_cb(void *user_data,
                       const XML_Char *s,
                       int len) {
  static_cast<xml_parser_c *>(user_data)->add_data_cb(s, len);
}

xml_parser_c::xml_parser_c(mm_text_io_c *xml_source)
  : m_xml_parser_state(XMLP_STATE_INITIAL)
  , m_xml_source(xml_source)
  , m_xml_parser(nullptr)
{
}

xml_parser_c::xml_parser_c()
  : m_xml_parser_state(XMLP_STATE_INITIAL)
  , m_xml_parser(nullptr)
{
}

void
xml_parser_c::setup_xml_parser() {
  if (nullptr != m_xml_parser)
    XML_ParserFree(m_xml_parser);

  m_xml_parser = XML_ParserCreate(nullptr);
  XML_SetUserData(m_xml_parser, this);
  XML_SetElementHandler(m_xml_parser, xml_parser_start_element_cb, xml_parser_end_element_cb);
  XML_SetCharacterDataHandler(m_xml_parser, xml_parser_add_data_cb);
}

xml_parser_c::~xml_parser_c() {
  if (nullptr != m_xml_parser)
    XML_ParserFree(m_xml_parser);
}

void
xml_parser_c::parse_xml_file() {
  m_xml_source->setFilePointer(0);

  while (parse_one_xml_line())
    ;
}

bool
xml_parser_c::parse_one_xml_line() {
  std::string line;

  if (nullptr == m_xml_parser)
    setup_xml_parser();

  if (!m_xml_source->getline2(line))
    return false;

  handle_xml_encoding(line);

  line += "\n";
  if (XML_Parse(m_xml_parser, line.c_str(), line.length(), false) == 0) {
    XML_Error xerror  = XML_GetErrorCode(m_xml_parser);
    std::string error = XML_ErrorString(xerror);

    if (XML_ERROR_INVALID_TOKEN == xerror)
      error += Y("Remember that special characters like &, <, > and \" must be escaped in the usual HTML way: &amp; for '&', &lt; for '<', &gt; for '>' and &quot; for '\"'.");
    throw mtx::xml::parser_x(error, m_xml_parser);
  }

  return true;
}

void
xml_parser_c::handle_xml_encoding(std::string &line) {
  if ((XMLP_STATE_AFTER_HEADER == m_xml_parser_state) || (BO_NONE == m_xml_source->get_byte_order()))
    return;

  size_t pos = 0;
  std::string new_line;

  if (XMLP_STATE_INITIAL == m_xml_parser_state) {
    pos = line.find("<?xml");
    if (std::string::npos == pos)
      return;
    m_xml_parser_state = XMLP_STATE_ATTRIBUTE_NAME;
    pos += 5;
    new_line = line.substr(0, pos);
  }

  while ((line.length() > pos) &&
         (XMLP_STATE_AFTER_HEADER != m_xml_parser_state)) {
    char cur_char = line[pos];
    ++pos;

    if (XMLP_STATE_ATTRIBUTE_NAME == m_xml_parser_state) {
      if (('?' == cur_char) && (line.length() > pos) && ('>' == line[pos])) {
        new_line += "?>" + line.substr(pos + 1, line.length() - pos - 1);
        m_xml_parser_state = XMLP_STATE_AFTER_HEADER;

      } else if ('"' == cur_char)
        m_xml_parser_state = XMLP_STATE_ATTRIBUTE_VALUE;

      else if ((' ' != cur_char) && ('=' != cur_char))
        m_xml_attribute_name += cur_char;

    } else {
      // XMLP_STATE_ATTRIBUTE_VALUE
      if ('"' == cur_char) {
        m_xml_parser_state = XMLP_STATE_ATTRIBUTE_NAME;
        strip(m_xml_attribute_name);
        strip(m_xml_attribute_value);

        if (m_xml_attribute_name == "encoding") {
          balg::to_lower(m_xml_attribute_value);
          if ((m_xml_source->get_byte_order() == BO_NONE) && ((m_xml_attribute_value == "utf-8") || (m_xml_attribute_value == "utf8")))
            m_xml_source->set_byte_order(BO_UTF8);

          else if (balg::istarts_with(m_xml_attribute_value, "utf"))
            m_xml_attribute_value = "UTF-8";
        }

        new_line              += " " + m_xml_attribute_name + "=\"" + m_xml_attribute_value + "\"";
        m_xml_attribute_name   = "";
        m_xml_attribute_value  = "";

      } else
        m_xml_attribute_value += cur_char;
    }
  }

  line = new_line;
}
