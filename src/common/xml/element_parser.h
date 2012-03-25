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

#include "common/error.h"
#include "common/xml/element_mapping.h"

namespace libebml {
  class EbmlElement;
  class EbmlMaster;
};

class mm_text_io_c;

using namespace libebml;

namespace mtx {
  namespace xml {
    class parser_x: public exception {
    protected:
      std::string m_error;
      int m_line, m_column;

    public:
      parser_x(const std::string &error, XML_Parser &parser)
        : m_error(error)
        , m_line(XML_GetCurrentLineNumber(parser))
        , m_column(XML_GetCurrentColumnNumber(parser))
      {
      }

      parser_x()
        : m_error(Y("No error"))
        , m_line(-1)
        , m_column(-1)
      {
      }

      virtual ~parser_x() throw() { }

      virtual const char *what() const throw() {
        return "XML parser error";
      }

      virtual std::string error() const throw() {
        return (boost::format(Y("Line %1%, column %2%: %3%")) % m_line % m_column % m_error).str();
      }

      virtual int get_line() const {
        return m_line;
      }

      virtual int get_column() const {
        return m_column;
      }
    };

    class file_parser_x: public parser_x {
    protected:
      std::string m_parser_name, m_file_name;

    public:
      file_parser_x(const std::string &parser_name, const std::string &file_name, const std::string &message, XML_Parser &parser)
        : parser_x(message, parser)
        , m_parser_name(parser_name)
        , m_file_name(file_name)
      {
      }
      virtual ~file_parser_x() throw() { }

      virtual std::string error() const throw() {
        return (boost::format(Y("Error: %1% parser failed for '%2%', line %3%, column %4%: %5%\n")) % m_parser_name % m_file_name % m_line % m_column % m_error).str();
      }
    };
  }
}

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

  virtual void start_element_cb(const char */* name */, const char **/* atts */) {
  }
  virtual void end_element_cb(const char */* name */) {
  }
  virtual void add_data_cb(const XML_Char */* s */, int /* len */) {
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
typedef std::shared_ptr<parser_data_t> parser_data_cptr;

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
