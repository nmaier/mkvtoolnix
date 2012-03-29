/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_EBML_XML_CONVERTER_H
#define MTX_COMMON_XML_EBML_XML_CONVERTER_H

#include "common/common_pch.h"

#include "common/ebml.h"
#include "common/xml/pugi.h"

namespace mtx { namespace xml {

class exception: public mtx::exception {
public:
  virtual const char *what() const throw() {
    return "generic XML error";
  }
};

class conversion_x: public exception {
protected:
  std::string m_message;
public:
  conversion_x(std::string const &message)  : m_message(message)       { }
  conversion_x(boost::format const &message): m_message(message.str()) { }
  virtual ~conversion_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class invalid_attribute_x: public exception {
protected:
  std::string m_message, m_node, m_attribute;
  size_t m_position;
public:
  invalid_attribute_x(std::string const &node, std::string const &attribute, size_t position)
    : m_node(node)
    , m_attribute(attribute)
    , m_position(position)
  {
    m_message = (boost::format(Y("Invalid attribute '%1%' in node '%2%' at position %3%")) % m_attribute % m_node % m_position).str();
  }
  virtual ~invalid_attribute_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class invalid_child_node_x: public exception {
protected:
  std::string m_message, m_node, m_parent;
  size_t m_position;
public:
  invalid_child_node_x(std::string const &node, std::string const &parent, size_t position)
    : m_node(node)
    , m_parent(parent)
    , m_position(position)
  {
    m_message = (boost::format(Y("<%1%> is not a valid child element of <%2%> at position %3%.")) % m_node % m_parent % m_position).str();
  }
  virtual ~invalid_child_node_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class duplicate_child_node_x: public exception {
protected:
  std::string m_message, m_node, m_parent;
  size_t m_position;
public:
  duplicate_child_node_x(std::string const &node, std::string const &parent, size_t position)
    : m_node(node)
    , m_parent(parent)
    , m_position(position)
  {
    m_message = (boost::format(Y("Only one instance of <%1%> is allowed beneath <%2%> at position %3%.")) % m_node % m_parent % m_position).str();
  }
  virtual ~duplicate_child_node_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class malformed_data_x: public exception {
protected:
  std::string m_message, m_node;
  size_t m_position;
public:
  malformed_data_x(std::string const &node, size_t position, std::string const &details = std::string{})
    : m_node(node)
    , m_position(position)
  {
    m_message = (boost::format(Y("The tag or attribute '%1%' at position %2% contains invalid or mal-formed data.")) % m_node % m_position).str();
    if (!details.empty())
      m_message += " " + details;
  }
  virtual ~malformed_data_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class out_of_range_x: public exception {
protected:
  std::string m_message, m_node;
  size_t m_position;
public:
  out_of_range_x(std::string const &node, size_t position, std::string const &details = std::string{})
    : m_node(node)
    , m_position(position)
  {
    m_message = (boost::format(Y("The tag or attribute '%1%' at position %2% contains data that is outside its allowed range.")) % m_node % m_position).str();
    if (!details.empty())
      m_message += " " + details;
  }
  virtual ~out_of_range_x() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

class xml_parser_x: public exception {
protected:
  pugi::xml_parse_result m_result;
public:
  xml_parser_x(pugi::xml_parse_result const &result) : m_result(result) { }
  virtual const char *what() const throw() {
    return "XML parser error";
  }
  pugi::xml_parse_result const &result() {
    return m_result;
  }
};

class ebml_converter_c {
public:
  struct limits_t {
    bool has_min, has_max;
    int64_t min, max;
    limits_t();
    limits_t(bool p_has_min, bool p_has_max, int64_t p_min, int64_t p_max);
  };

  struct parser_context_t {
    std::string const &name;
    std::string const &content;
    EbmlElement &e;
    pugi::xml_node const &node;
    std::map<std::string, bool> &handled_attributes;
    limits_t limits;
  };

  typedef std::function<void(pugi::xml_node &, EbmlElement &)> value_formatter_t;
  typedef std::function<void(parser_context_t &ctx)> value_parser_t;

protected:
  std::map<std::string, std::string> m_debug_to_tag_name_map, m_tag_to_debug_name_map;
  std::map<std::string, value_formatter_t> m_formatter_map;
  std::map<std::string, value_parser_t> m_parser_map;
  std::map<std::string, limits_t> m_limits;

public:
  ebml_converter_c();
  virtual ~ebml_converter_c();

  document_cptr to_xml(EbmlElement &e, document_cptr const &destination = document_cptr{}) const;
  ebml_master_cptr to_ebml(std::string const &file_name, std::string const &required_root_name);

public:
  static void format_uint(pugi::xml_node &node, EbmlElement &e);
  static void format_int(pugi::xml_node &node, EbmlElement &e);
  static void format_string(pugi::xml_node &node, EbmlElement &e);
  static void format_ustring(pugi::xml_node &node, EbmlElement &e);
  static void format_binary(pugi::xml_node &node, EbmlElement &e);
  static void format_timecode(pugi::xml_node &node, EbmlElement &e);

  static void parse_uint(parser_context_t &ctx);
  static void parse_int(parser_context_t &ctx);
  static void parse_string(parser_context_t &ctx);
  static void parse_ustring(parser_context_t &ctx);
  static void parse_binary(parser_context_t &ctx);
  static void parse_timecode(parser_context_t &ctx);

  static document_cptr load_xml(std::string const &file_name);

protected:
  void format_value(pugi::xml_node &node, EbmlElement &e, value_formatter_t default_formatter) const;
  void parse_value(parser_context_t &ctx, value_parser_t default_parser) const;

  void to_xml_recursively(pugi::xml_node &parent, EbmlElement &e) const;

  void to_ebml_recursively(EbmlMaster &parent, pugi::xml_node &node) const;
  EbmlElement *convert_node_or_attribute_to_ebml(EbmlMaster &parent, pugi::xml_node const &node, pugi::xml_attribute const &attribute, std::map<std::string, bool> &handled_attributes) const;
  EbmlElement *verify_and_create_element(EbmlMaster &parent, std::string const &name, pugi::xml_node const &node) const;

  virtual void fix_xml(document_cptr &doc) const;
  virtual void fix_ebml(EbmlMaster &root) const;

  std::string get_tag_name(EbmlElement &e) const;
  std::string get_debug_name(std::string const &tag_name) const;

  void reverse_debug_to_tag_name_map();

  void dump_semantics(std::string const &top_element_name) const;
  void dump_semantics_recursively(int level, EbmlElement &element, std::map<std::string, bool> &visited_masters) const;
};

}}

#endif // MTX_COMMON_XML_EBML_XML_CONVERTER_H
