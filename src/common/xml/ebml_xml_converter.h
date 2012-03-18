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

class ebml_xml_converter_c {
  typedef std::function<void(pugi::xml_node &, EbmlElement *)> value_formatter_t;

protected:
  std::map<std::string, std::string> m_name_map;
  std::map<std::string, value_formatter_t> m_formatter_map;

public:
  ebml_xml_converter_c();
  virtual ~ebml_xml_converter_c();

  xml_document_cptr to_xml(EbmlElement *e, xml_document_cptr const &destination = xml_document_cptr(nullptr)) const;

public:
  static void format_uint(pugi::xml_node &node, EbmlElement *e);
  static void format_int(pugi::xml_node &node, EbmlElement *e);
  static void format_string(pugi::xml_node &node, EbmlElement *e);
  static void format_ustring(pugi::xml_node &node, EbmlElement *e);
  static void format_binary(pugi::xml_node &node, EbmlElement *e);
  static void format_timecode(pugi::xml_node &node, EbmlElement *e);

protected:
  void format_value(pugi::xml_node &node, EbmlElement *e, value_formatter_t default_formatter) const;

  void to_xml_recursively(pugi::xml_node &parent, EbmlElement *e) const;
  virtual void fix_xml(xml_document_cptr &doc) const;
  std::string get_name(EbmlElement *e) const;
};

#endif // MTX_COMMON_XML_EBML_XML_CONVERTER_H
