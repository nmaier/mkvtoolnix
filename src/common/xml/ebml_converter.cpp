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

#include <sstream>

#include <matroska/KaxSegment.h>

#include "common/base64.h"
#include "common/ebml.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/strings/utf8.h"
#include "common/xml/ebml_converter.h"

namespace mtx { namespace xml {

ebml_converter_c::ebml_converter_c()
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
ebml_converter_c::parse_value(parser_context_t &ctx,
                              value_parser_t default_parser)
  const {
  auto parser = m_parser_map.find(ctx.name);

  if (parser != m_parser_map.end())
    parser->second(ctx);
  else
    default_parser(ctx);
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
ebml_converter_c::fix_ebml(EbmlMaster &)
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
ebml_converter_c::parse_uint(parser_context_t &ctx) {
  uint64_t value;
  if (!::parse_uint(strip_copy(ctx.content), value))
    throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("An unsigned integer was expected.") };

  if (ctx.has_min && (value < static_cast<uint64_t>(ctx.min)))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Minimum allowed value: %1%, actual value: %2%")) % ctx.min % value).str() };
  if (ctx.has_max && (value > static_cast<uint64_t>(ctx.max)))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Maximum allowed value: %1%, actual value: %2%")) % ctx.max % value).str() };

  static_cast<EbmlUInteger &>(ctx.e) = value;
}

void
ebml_converter_c::parse_int(parser_context_t &ctx) {
  int64_t value;
  if (!::parse_int(strip_copy(ctx.content), value))
    throw malformed_data_x{ ctx.name, ctx.node.offset_debug() };

  if (ctx.has_min && (value < ctx.min))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Minimum allowed value: %1%, actual value: %2%")) % ctx.min % value).str() };
  if (ctx.has_max && (value > ctx.max))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Maximum allowed value: %1%, actual value: %2%")) % ctx.max % value).str() };

  static_cast<EbmlSInteger &>(ctx.e) = value;
}

void
ebml_converter_c::parse_string(parser_context_t &ctx) {
  static_cast<EbmlString &>(ctx.e) = ctx.content;
}

void
ebml_converter_c::parse_ustring(parser_context_t &ctx) {
  static_cast<EbmlUnicodeString &>(ctx.e) = UTFstring{to_wide(ctx.content).c_str()};
}

void
ebml_converter_c::parse_timecode(parser_context_t &ctx) {
  int64_t value;
  if (!::parse_timecode(strip_copy(ctx.content), value)) {
    auto details = (boost::format(Y("Expected a time in the following format: HH:MM:SS.nnn "
                                    "(HH = hour, MM = minute, SS = second, nnn = millisecond up to nanosecond. "
                                    "You may use up to nine digits for 'n' which would mean nanosecond precision). "
                                    "You may omit the hour as well. Found '%1%' instead. Additional error message: %2%"))
                    % ctx.content % timecode_parser_error.c_str()).str();

    throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), details };
  }

  if (ctx.has_min && (value < ctx.min))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Minimum allowed value: %1%, actual value: %2%")) % ctx.min % value).str() };
  if (ctx.has_max && (value > ctx.max))
    throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Maximum allowed value: %1%, actual value: %2%")) % ctx.max % value).str() };

  static_cast<EbmlUInteger &>(ctx.e) = value;
}

void
ebml_converter_c::parse_binary(parser_context_t &ctx) {
  auto test_min_max = [&](std::string const &content) {
    if (ctx.has_min && (content.length() < static_cast<size_t>(ctx.min)))
      throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Minimum allowed length: %1%, actual length: %2%")) % ctx.min % content.length()).str() };
    if (ctx.has_max && (content.length() > static_cast<size_t>(ctx.max)))
      throw out_of_range_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Maximum allowed length: %1%, actual length: %2%")) % ctx.max % content.length()).str() };
  };

  ctx.handled_attributes["format"] = true;

  auto content = strip_copy(ctx.content);

  if (balg::starts_with(content, "@")) {
    if (content.length() == 1)
      throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("No filename found after the '@'.") };

    auto file_name = content.substr(1);
    try {
      mm_file_io_c in(file_name, MODE_READ);
      auto size = in.get_size();
      content.resize(size);

      if (in.read(content, size) != size)
        throw mtx::mm_io::end_of_file_x{};

      test_min_max(content);

      static_cast<EbmlBinary &>(ctx.e).CopyBuffer(reinterpret_cast<binary const *>(content.c_str()), size);

    } catch (mtx::mm_io::exception &) {
      throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Could not open/read the file '%1%'.")) % file_name).str() };
    }

    return;
  }

  std::string format = balg::to_lower_copy(std::string{ctx.node.attribute("format").value()});
  if (format.empty())
    format = "base64";

  if (format == "hex") {
    auto hex_content = boost::regex_replace(content, boost::regex{"(0x|\\s|\\r|\\n)+", boost::regex::perl | boost::regex::icase}, "");
    if (boost::regex_search(hex_content, boost::regex{"[^\\da-f]", boost::regex::perl | boost::regex::icase}))
      throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("Non-hex digits encountered.") };

    if ((hex_content.size() % 2) == 1)
      throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("Invalid length of hexadecimal content: must be divisable by 2.") };

    content.clear();
    content.resize(hex_content.size() / 2);

    for (auto idx = 0u; idx < hex_content.length(); idx += 2)
      content[idx / 2] = static_cast<char>(from_hex(hex_content.substr(idx, 2)));

  } else if (format == "base64") {
    try {
      std::string destination(' ', content.size());
      auto dest_len = base64_decode(content, reinterpret_cast<unsigned char *>(&destination[0]));
      if (-1 == dest_len)
        throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("Invalid data for Base64 encoding found.") };
      content = destination.substr(0, dest_len);

    } catch (mtx::base64::exception &) {
      throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), Y("Invalid data for Base64 encoding found.") };
    }

  } else if (format != "ascii")
    throw malformed_data_x{ ctx.name, ctx.node.offset_debug(), (boost::format(Y("Invalid 'format' attribute '%1%'.")) % format).str() };

  test_min_max(content);

  static_cast<EbmlBinary &>(ctx.e).CopyBuffer(reinterpret_cast<binary const *>(content.c_str()), content.length());
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

ebml_master_cptr
ebml_converter_c::to_ebml(std::string const &file_name,
                          std::string const &root_name) {
  auto doc = load_xml(file_name);

  auto root_node = doc->document_element();
  if (!root_node)
    return ebml_master_cptr();

  if (root_name != root_node.name())
    throw conversion_x{boost::format(Y("The root element must be <%1%>.")) % root_name};

  ebml_master_cptr ebml_root{new KaxSegment};

  to_ebml_recursively(*ebml_root, root_node);

  auto master = dynamic_cast<EbmlMaster *>((*ebml_root)[0]);
  if (nullptr == master)
    throw conversion_x{Y("The XML root element is not a master element.")};

  fix_ebml(*master);

  ebml_root->Remove(0);

  return ebml_master_cptr{master};
}

void
ebml_converter_c::to_ebml_recursively(EbmlMaster &parent,
                                      pugi::xml_node &node) {
  std::map<std::string, bool> handled_attributes;

  auto converted_node   = convert_node_or_attribute_to_ebml(parent, node, pugi::xml_attribute{}, handled_attributes);
  auto converted_master = dynamic_cast<EbmlMaster *>(converted_node);

  for (auto attribute = node.attributes_begin(); node.attributes_end() != attribute; attribute++) {
    if (handled_attributes[attribute->name()])
      continue;

    if (!converted_master)
      throw invalid_attribute_x{ node.name(), attribute->name(), node.offset_debug() };

    convert_node_or_attribute_to_ebml(*converted_master, node, *attribute, handled_attributes);
  }

  for (auto child : node) {
    if (child.type() != pugi::node_element)
      continue;

    if (converted_master)
      to_ebml_recursively(*converted_master, child);
    else
      throw invalid_child_node_x{ node.first_child().name(), node.name(), node.offset_debug() };
  }
}

EbmlElement *
ebml_converter_c::convert_node_or_attribute_to_ebml(EbmlMaster &parent,
                                                    pugi::xml_node const &node,
                                                    pugi::xml_attribute const &attribute,
                                                    std::map<std::string, bool> &handled_attributes) {
  std::string name  = attribute ? attribute.name()  : node.name();
  std::string value = attribute ? attribute.value() : node.child_value();
  auto new_element  = verify_and_create_element(parent, name, node);

  parser_context_t ctx { name, value, *new_element, node, handled_attributes, false, false, 0, 0 };

  if (dynamic_cast<EbmlUInteger *>(new_element))
    parse_value(ctx, parse_uint);

  else if (dynamic_cast<EbmlSInteger *>(new_element))
    parse_value(ctx, parse_uint);

  else if (dynamic_cast<EbmlString *>(new_element))
    parse_value(ctx, parse_string);

  else if (dynamic_cast<EbmlUnicodeString *>(new_element))
    parse_value(ctx, parse_ustring);

  else if (dynamic_cast<EbmlBinary *>(new_element))
    parse_value(ctx, parse_binary);

  else if (!dynamic_cast<EbmlMaster *>(new_element))
    throw invalid_child_node_x{ name, get_tag_name(parent), node.offset_debug() };

  parent.PushElement(*new_element);

  return new_element;
}

EbmlElement *
ebml_converter_c::verify_and_create_element(EbmlMaster &parent,
                                            std::string const &name,
                                            pugi::xml_node const &node) {
  auto debug_name = get_debug_name(name);
  auto &context   = EBML_CONTEXT(&parent);
  bool found      = false;
  EbmlId id(static_cast<uint32>(0), 0);
  size_t i;

  for (i = 0; i < EBML_CTX_SIZE(context); i++)
    if (debug_name == EBML_CTX_IDX_INFO(context, i).DebugName) {
      found = true;
      id    = EBML_CTX_IDX_ID(context, i);
      break;
    }

  if (!found)
    throw invalid_child_node_x{ name, get_tag_name(parent), node.offset_debug() };

  auto semantic = find_ebml_semantic(EBML_INFO(KaxSegment), id);
  if ((nullptr != semantic) && EBML_SEM_UNIQUE(*semantic))
    for (i = 0; i < parent.ListSize(); i++)
      if (EbmlId(*parent[i]) == id)
        throw duplicate_child_node_x{ name, get_tag_name(parent), node.offset_debug() };

  return create_ebml_element(EBML_INFO(KaxSegment), id);
}

document_cptr
ebml_converter_c::load_xml(std::string const &file_name) {
  mm_text_io_c in(new mm_file_io_c(file_name, MODE_READ));
  std::string content;

  if (in.read(content, in.get_size()) != in.get_size())
    throw mtx::mm_io::end_of_file_x{};

  if (BO_NONE == in.get_byte_order()) {
    boost::regex encoding_re("^ \\s* "              // ignore leading whitespace
                             "<\\?xml"              // XML declaration start
                             "[^\\?]+"              // skip to encoding, but don't go beyond XML declaration
                             "encoding \\s* = \\s*" // encoding attribute
                             "\" ( [^\"]+ ) \"",    // attribute value
                             boost::regex::perl | boost::regex::mod_x | boost::regex::icase);
    boost::match_results<std::string::const_iterator> matches;
    if (boost::regex_search(content, matches, encoding_re))
      content = charset_converter_c::init(matches[1].str())->utf8(content);
  }

  std::stringstream scontent(content);
  document_cptr doc(new pugi::xml_document);
  auto result = doc->load(scontent);
  if (!result)
    throw xml_parser_x{result};

  return doc;
}

}}