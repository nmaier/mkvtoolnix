/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   general XML element parser, definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_XML_ELEMENT_PARSER_H
#define __MTX_COMMON_XML_ELEMENT_PARSER_H

#include "common/common_pch.h"

#include <expat.h>

#include "common/xml/element_mapping.h"

namespace libebml {
  class EbmlElement;
  class EbmlMaster;
};

class mm_text_io_c;

using namespace libebml;

class xml_parser_error_c: public error_c {
public:
  int m_line, m_column;

public:
  xml_parser_error_c(const std::string &message, XML_Parser &parser)
    : error_c(message)
    , m_line(XML_GetCurrentLineNumber(parser))
    , m_column(XML_GetCurrentColumnNumber(parser))
  {
  }

  xml_parser_error_c()
    : error_c(Y("No error"))
    , m_line(-1)
    , m_column(-1) {
  }

  virtual std::string get_error() const {
    return (boost::format(Y("Line %1%, column %2%: %3%")) % m_line % m_column % error).str();
  }
};

class xml_parser_c {
private:
  std::string m_xml_attribute_name, m_xml_attribute_value;

protected:
  enum state_t {
    XMLP_STATE_INITIAL,
    XMLP_STATE_ATTRIBUTE_NAME,
    XMLP_STATE_ATTRIBUTE_VALUE,
    XMLP_STATE_AFTER_HEADER
  };

  state_t m_xml_parser_state;
  mm_text_io_c *m_xml_source;

  XML_Parser m_xml_parser;

public:
  xml_parser_c(mm_text_io_c *xml_source);
  xml_parser_c();
  virtual ~xml_parser_c();

  void setup_xml_parser();

  virtual void start_element_cb(const char *name, const char **atts) {
  }
  virtual void end_element_cb(const char *name) {
  }
  virtual void add_data_cb(const XML_Char *s, int len) {
  }

  virtual void parse_xml_file();
  virtual bool parse_one_xml_line();

private:
  void handle_xml_encoding(std::string &line);
};

struct parser_data_t {
public:
  XML_Parser parser;

  std::string file_name, parser_name;
  const parser_element_t *mapping;

  int depth, skip_depth;
  bool done_reading, data_allowed;

  std::string bin, format;

  std::vector<EbmlElement *> parents;
  std::vector<int> parent_idxs;

  EbmlMaster *root_element;
public:
  parser_data_t();
};
typedef counted_ptr<parser_data_t> parser_data_cptr;

#define CPDATA static_cast<parser_data_t *>(pdata)

#define xmlp_pelt  static_cast<parser_data_t *>(pdata)->parents.back()
#define xmlp_pname xmlp_parent_name(static_cast<parser_data_t *>(pdata), xmlp_pelt)

EbmlMaster * parse_xml_elements(const char *parser_name, const parser_element_t *mapping, mm_text_io_c *in);

const char *xmlp_parent_name(parser_data_t *pdata, EbmlElement *e);

void xmlp_error(parser_data_t *pdata, const std::string &message);
inline void
xmlp_error(parser_data_t *pdata,
           const boost::format &format) {
  xmlp_error(pdata, format.str());
}

#endif
