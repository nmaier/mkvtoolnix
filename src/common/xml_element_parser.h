/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   general XML element parser, definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XML_ELEMENT_PARSER_H
#define __XML_ELEMENT_PARSER_H

#include "os.h"

#include <expat.h>
#include <setjmp.h>

#include <string>
#include <vector>

#include "common.h"
#include "xml_element_mapping.h"

namespace libebml {
  class EbmlElement;
  class EbmlMaster;
};

class mm_text_io_c;

using namespace std;
using namespace libebml;

class MTX_DLL_API xml_parser_error_c: public error_c {
public:
  int m_line, m_column;

public:
  xml_parser_error_c(const string &message, XML_Parser &parser):
    error_c(message), m_line(XML_GetCurrentLineNumber(parser)),
    m_column(XML_GetCurrentColumnNumber(parser)) {
  }

  xml_parser_error_c():
    error_c("no error"), m_line(-1), m_column(-1) {
  }

  virtual string get_error() {
    return mxsprintf("Line %d, column %d: ", m_line, m_column) + error;
  }
};

class MTX_DLL_API xml_parser_c {
private:
  jmp_buf m_parser_error_jmp_buf;
  xml_parser_error_c m_saved_parser_error;
  string m_xml_attribute_name, m_xml_attribute_value;

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

  virtual void throw_error(const xml_parser_error_c &error);

private:
  void handle_xml_encoding(string &line);
};

typedef struct {
  XML_Parser parser;

  const char *file_name;
  const char *parser_name;
  const parser_element_t *mapping;

  int depth, skip_depth;
  bool done_reading, data_allowed;

  string *bin;
  const char *format;

  vector<EbmlElement *> *parents;
  vector<int> *parent_idxs;

  EbmlMaster *root_element;

  jmp_buf parse_error_jmp;
  string *parse_error_msg;
} parser_data_t;

#define xmlp_pelt (*((parser_data_t *)pdata)->parents) \
                     [((parser_data_t *)pdata)->parents->size() - 1]
#define xmlp_pname xmlp_parent_name((parser_data_t *)pdata, xmlp_pelt)

EbmlMaster * MTX_DLL_API
parse_xml_elements(const char *parser_name, const parser_element_t *mapping,
                   mm_text_io_c *in);

const char * MTX_DLL_API
xmlp_parent_name(parser_data_t *pdata, EbmlElement *e);

void MTX_DLL_API
xmlp_error(parser_data_t *pdata, const char *fmt, ...);


#endif
