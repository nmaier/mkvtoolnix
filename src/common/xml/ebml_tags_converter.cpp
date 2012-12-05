/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for tags

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/strings/formatting.h"
#include "common/xml/ebml_tags_converter.h"

namespace mtx { namespace xml {

ebml_tags_converter_c::ebml_tags_converter_c()
{
  setup_maps();
}

ebml_tags_converter_c::~ebml_tags_converter_c() {
}

void
ebml_tags_converter_c::setup_maps() {
  m_debug_to_tag_name_map["TagTargets"]         = "Targets";
  m_debug_to_tag_name_map["TagTrackUID"]        = "TrackUID";
  m_debug_to_tag_name_map["TagEditionUID"]      = "EditionUID";
  m_debug_to_tag_name_map["TagChapterUID"]      = "ChapterUID";
  m_debug_to_tag_name_map["TagAttachmentUID"]   = "AttachmentUID";
  m_debug_to_tag_name_map["TagTargetType"]      = "TargetType";
  m_debug_to_tag_name_map["TagTargetTypeValue"] = "TargetTypeValue";
  m_debug_to_tag_name_map["TagSimple"]          = "Simple";
  m_debug_to_tag_name_map["TagName"]            = "Name";
  m_debug_to_tag_name_map["TagString"]          = "String";
  m_debug_to_tag_name_map["TagBinary"]          = "Binary";
  m_debug_to_tag_name_map["TagLanguage"]        = "TagLanguage";
  m_debug_to_tag_name_map["TagDefault"]         = "DefaultLanguage";

  m_limits["TagDefault"]                        = limits_t{ true, true, 0, 1 };

  reverse_debug_to_tag_name_map();

  if (debugging_requested("ebml_converter_semantics"))
    dump_semantics("Tags");
}

void
ebml_tags_converter_c::write_xml(KaxTags &tags,
                                 mm_io_c &out) {
  document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> ");

  ebml_tags_converter_c converter;
  converter.to_xml(tags, doc);

  std::stringstream out_stream;
  doc->save(out_stream, "  ", pugi::format_default | pugi::format_write_bom);
  out.puts(out_stream.str());
}

void
ebml_tags_converter_c::fix_ebml(EbmlMaster &root)
  const {
  for (auto child : root)
    if (dynamic_cast<KaxTag *>(child))
      fix_tag(*static_cast<KaxTag *>(child));
}

void
ebml_tags_converter_c::fix_tag(KaxTag &tag)
  const {
  for (auto child : tag)
    if (dynamic_cast<KaxTag *>(child))
      fix_tag(*static_cast<KaxTag *>(child));

  auto simple = FindChild<KaxTagSimple>(tag);
  if (!simple)
    throw conversion_x{ Y("<Tag> is missing the <Simple> child.") };

  if (!FindChild<KaxTagName>(simple))
    throw conversion_x{ Y("<Simple> is missing the <Name> child.") };

  auto string = FindChild<KaxTagString>(*simple);
  auto binary = FindChild<KaxTagBinary>(*simple);
  if (string && binary)
    throw conversion_x{ Y("Only one of <String> and <Binary> may be used beneath <Simple> but not both at the same time.") };
  if (!string && !binary && !FindChild<KaxTagSimple>(*simple))
    throw conversion_x{ Y("<Simple> must contain either a <String> or a <Binary> child.") };
}

kax_tags_cptr
ebml_tags_converter_c::parse_file(std::string const &file_name,
                                  bool throw_on_error) {
  auto parse = [&]() -> std::shared_ptr<KaxTags> {
    auto master = ebml_tags_converter_c{}.to_ebml(file_name, "Tags");
    fix_mandatory_tag_elements(static_cast<KaxTags *>(master.get()));
    return std::dynamic_pointer_cast<KaxTags>(master);
  };

  if (throw_on_error)
    return parse();

  try {
    return parse();

  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The XML tag file '%1%' could not be read.\n")) % file_name);

  } catch (mtx::xml::xml_parser_x &ex) {
    mxerror(boost::format(Y("The XML tag file '%1%' contains an error at position %3%: %2%\n")) % file_name % ex.result().description() % ex.result().offset);

  } catch (mtx::xml::exception &ex) {
    mxerror(boost::format(Y("The XML tag file '%1%' contains an error: %2%\n")) % file_name % ex.what());
  }

  return kax_tags_cptr{};
}

}}
