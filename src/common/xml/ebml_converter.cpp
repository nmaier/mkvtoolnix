/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/strings/formatting.h"
#include "common/xml/ebml_converter.h"

namespace mtx { namespace xml {

ebml_converter_c::ebml_converter_c()
  : m_debug_to_tag_name_map()
  , m_tag_to_debug_name_map()
  , m_formatter_map()
{
}

ebml_converter_c::~ebml_converter_c() {
}

document_cptr
ebml_converter_c::to_xml(EbmlElement &e,
                         document_cptr const &destination)
  const {
  document_cptr doc = destination ? destination : document_cptr(new pugi::xml_document);
  to_xml_recursively(*doc, e);
  fix_xml(doc);
  return doc;
}

std::string
ebml_converter_c::get_tag_name(EbmlElement &e)
  const {
  auto mapped_name = m_debug_to_tag_name_map.find(EBML_NAME(&e));
  return mapped_name == m_debug_to_tag_name_map.end() ? std::string(EBML_NAME(&e)) : mapped_name->second;
}

std::string
ebml_converter_c::get_debug_name(std::string const &tag_name)
  const {
  auto mapped_name = m_tag_to_debug_name_map.find(tag_name);
  return mapped_name == m_tag_to_debug_name_map.end() ? tag_name : mapped_name->second;
}

void
ebml_converter_c::format_value(pugi::xml_node &node,
                               EbmlElement &e,
                               value_formatter_t default_formatter)
  const {
  std::string name = get_tag_name(e);
  auto formatter   = m_formatter_map.find(name);

  if (formatter != m_formatter_map.end())
    formatter->second(node, e);
  else
    default_formatter(node, e);
}

void
ebml_converter_c::reverse_debug_to_tag_name_map() {
  m_tag_to_debug_name_map.clear();
  for (auto &pair : m_debug_to_tag_name_map)
    m_tag_to_debug_name_map[pair.second] = pair.first;
}

void
ebml_converter_c::fix_xml(document_cptr &)
  const {
}

void
ebml_converter_c::format_uint(pugi::xml_node &node,
                              EbmlElement &e) {
  node.append_child(pugi::node_pcdata).set_value(to_string(uint64(static_cast<EbmlUInteger &>(e))).c_str());
}

void
ebml_converter_c::format_int(pugi::xml_node &node,
                             EbmlElement &e) {
  node.append_child(pugi::node_pcdata).set_value(to_string(int64(static_cast<EbmlSInteger &>(e))).c_str());
}

void
ebml_converter_c::format_timecode(pugi::xml_node &node,
                                  EbmlElement &e) {
  node.append_child(pugi::node_pcdata).set_value(::format_timecode(uint64(static_cast<EbmlUInteger &>(e))).c_str());
}

void
ebml_converter_c::format_string(pugi::xml_node &node,
                                EbmlElement &e) {
  node.append_child(pugi::node_pcdata).set_value(std::string(static_cast<EbmlString &>(e)).c_str());
}

void
ebml_converter_c::format_ustring(pugi::xml_node &node,
                                 EbmlElement &e) {
  node.append_child(pugi::node_pcdata).set_value(UTFstring_to_cstrutf8(UTFstring(static_cast<EbmlUnicodeString &>(e))).c_str());
}

void
ebml_converter_c::format_binary(pugi::xml_node &node,
                                EbmlElement &e) {
  auto &binary    = static_cast<EbmlBinary &>(e);
  bool pure_ascii = true;
  std::string data(reinterpret_cast<char const *>(binary.GetBuffer()), binary.GetSize());

  for (auto c : data)
    if (('\n' != c) && ('\r' != c) && ((' ' > c) || (127 <= c))) {
      pure_ascii = false;
      break;
    }

  if (!pure_ascii)
    data = to_hex(data);

  node.append_child(pugi::node_pcdata).set_value(data.c_str());
  node.append_attribute("format") = pure_ascii ? "ascii" : "hex";
}

void
ebml_converter_c::to_xml_recursively(pugi::xml_node &parent,
                                     EbmlElement &e)
  const {
  std::string name = get_tag_name(e);
  auto new_node    = parent.append_child(name.c_str());

  if (dynamic_cast<EbmlMaster *>(&e)) {
    auto &master = static_cast<EbmlMaster &>(e);
    for (auto idx = 0u; master.ListSize() > idx; ++idx)
      to_xml_recursively(new_node, *master[idx]);

  } else if (dynamic_cast<EbmlUInteger *>(&e))
    format_value(new_node, e, format_uint);

  else if (dynamic_cast<EbmlSInteger *>(&e))
    format_value(new_node, e, format_uint);

  else if (dynamic_cast<EbmlString *>(&e))
    format_value(new_node, e, format_string);

  else if (dynamic_cast<EbmlUnicodeString *>(&e))
    format_value(new_node, e, format_ustring);

  else if (dynamic_cast<EbmlBinary *>(&e))
    format_value(new_node, e, format_binary);

  else {
    parent.remove_child(new_node);
    parent.append_child(pugi::node_comment).set_value((boost::format(" unknown EBML element '%1%' ") % name).str().c_str());
  }
}

}}
