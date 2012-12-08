/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_H
#define MTX_COMMON_XML_H

#include "common/common_pch.h"

#include "pugixml.hpp"

namespace mtx {
namespace xml {

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
  ptrdiff_t m_position;
public:
  invalid_attribute_x(std::string const &node, std::string const &attribute, ptrdiff_t position)
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
  ptrdiff_t m_position;
public:
  invalid_child_node_x(std::string const &node, std::string const &parent, ptrdiff_t position)
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
  ptrdiff_t m_position;
public:
  duplicate_child_node_x(std::string const &node, std::string const &parent, ptrdiff_t position)
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
  ptrdiff_t m_position;
public:
  malformed_data_x(std::string const &node, ptrdiff_t position, std::string const &details = std::string{})
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
  ptrdiff_t m_position;
public:
  out_of_range_x(std::string const &node, ptrdiff_t position, std::string const &details = std::string{})
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

std::string escape(const std::string &src);
std::string create_node_name(const char *name, const char **atts);

typedef std::shared_ptr<pugi::xml_document> document_cptr;

document_cptr load_file(std::string const &file_name, unsigned int options = pugi::parse_default);

}}
#endif  // MTX_COMMON_XML_H
