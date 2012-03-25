/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML chapter writer functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_XML_ELEMENT_WRITER_H
#define __MTX_COMMON_XML_ELEMENT_WRITER_H

#include "common/common_pch.h"

#include <expat.h>

#include "common/mm_io.h"
#include "common/xml/element_parser.h"

namespace libebml {
  class EbmlElement;
};

using namespace libebml;

namespace mtx {
  namespace xml {
    class formatter_x: public exception {
    protected:
      std::string m_message;
    public:
      formatter_x(const std::string &message)  : m_message(message)       { }
      formatter_x(const boost::format &message): m_message(message.str()) { }
      virtual ~formatter_x() throw() { }

      virtual const char *what() const throw() {
        return m_message.c_str();
      }
    };
  }
}

struct xml_writer_cb_t {
  int level;
  int parent_idx;
  int elt_idx;
  EbmlElement *e;
  mm_io_c *out;
};

void write_xml_element_rec(int level, int parent_idx, EbmlElement *e, mm_io_c *out, const parser_element_t *element_map);

class xml_formatter_c: public xml_parser_c {
private:
  mm_io_c &m_out;
  std::shared_ptr<mm_text_io_c> m_temp_io;
  std::string m_encoding, m_dtd, m_dtd_file, m_stylesheet_type, m_stylesheet_file;
  charset_converter_cptr m_cc_utf8;
  bool m_header_written;

  std::string m_data_buffer;
  int m_depth;

  enum xml_formatter_state_e {
    XMLF_STATE_NONE,
    XMLF_STATE_START,
    XMLF_STATE_END,
    XMLF_STATE_DATA
  };
  xml_formatter_state_e m_state;

public:
  xml_formatter_c(mm_io_c &out, const std::string &encoding = std::string("UTF-8"));
  virtual ~xml_formatter_c();

  virtual void set_doctype(const std::string &dtd, const std::string &file);
  virtual void set_stylesheet(const std::string &type, const std::string &file);

  virtual void write_header();
  virtual void format(const std::string &text);
  virtual void format_fixed(const std::string &text);
  virtual void flush();

  virtual void start_element_cb(const char *name, const char **atts);
  virtual void end_element_cb(const char *name);
  virtual void add_data_cb(const XML_Char *s, int len);
};

#endif // __MTX_COMMON_XML_ELEMENT_WRITER_H
