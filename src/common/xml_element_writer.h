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

#ifndef __XML_ELEMENT_WRITER_H
#define __XML_ELEMENT_WRITER_H

#include "os.h"

#include <expat.h>

#include "mm_io.h"
#include "xml_element_parser.h"

namespace libebml {
  class EbmlElement;
};

using namespace libebml;

struct xml_writer_cb_t {
  int level;
  int parent_idx;
  int elt_idx;
  EbmlElement *e;
  mm_io_c *out;
};

void MTX_DLL_API
write_xml_element_rec(int level, int parent_idx,
                      EbmlElement *e, mm_io_c *out,
                      const parser_element_t *element_map);

class xml_formatter_error_c: public error_c {
public:
  xml_formatter_error_c(const string &_error):
    error_c(_error) { }
};

class xml_formatter_c {
private:
  mm_io_c *m_out;
  string m_encoding, m_dtd, m_dtd_file, m_stylesheet_type, m_stylesheet_file;
  int m_cc_utf8;
  bool m_header_written;

  XML_Parser m_parser;
  string m_data_buffer;
  int m_depth;

  enum xml_formatter_state_e {
    XMLF_STATE_NONE,
    XMLF_STATE_START,
    XMLF_STATE_END,
    XMLF_STATE_DATA
  };
  xml_formatter_state_e m_state;

public:
  xml_formatter_c(mm_io_c *out, const string &encoding = string("UTF-8"));
  virtual ~xml_formatter_c();

  virtual void set_doctype(const string &dtd, const string &file);
  virtual void set_stylesheet(const string &type, const string &file);

  virtual void write_header();
  virtual void format(const string &text);
  virtual void flush();

  virtual void start_element_cb(const char *name, const char **atts);
  virtual void end_element_cb(const char *name);
  virtual void add_data_cb(const XML_Char *s, int len);
};

#endif // __XML_ELEMENT_WRITER_H
