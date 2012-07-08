/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for chapters

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/strings/formatting.h"
#include "common/unique_numbers.h"
#include "common/xml/ebml_chapters_converter.h"

namespace mtx { namespace xml {

ebml_chapters_converter_c::ebml_chapters_converter_c()
{
  setup_maps();
}

ebml_chapters_converter_c::~ebml_chapters_converter_c() {
}

void
ebml_chapters_converter_c::setup_maps() {
  m_formatter_map["ChapterTimeStart"]  = format_timecode;
  m_formatter_map["ChapterTimeEnd"]    = format_timecode;

  m_parser_map["ChapterTimeStart"]     = parse_timecode;
  m_parser_map["ChapterTimeEnd"]       = parse_timecode;

  m_limits["EditionUID"]               = limits_t{ true, false, 1, 0 };
  m_limits["EditionFlagHidden"]        = limits_t{ true, true,  0, 1 };
  m_limits["EditionFlagDefault"]       = limits_t{ true, true,  0, 1 };
  m_limits["EditionFlagOrdered"]       = limits_t{ true, true,  0, 1 };
  m_limits["ChapterFlagHidden"]        = limits_t{ true, true,  0, 1 };
  m_limits["ChapterFlagEnabled"]       = limits_t{ true, true,  0, 1 };
  m_limits["ChapterUID"]               = limits_t{ true, false, 1, 0 };
  m_limits["ChapterSegmentUID"]        = limits_t{ true, false, 1, 0 };
  m_limits["ChapterSegmentEditionUID"] = limits_t{ true, false, 1, 0 };
  m_limits["ChapterTrackNumber"]       = limits_t{ true, false, 1, 0 };

  reverse_debug_to_tag_name_map();

  if (debugging_requested("ebml_converter_semantics"))
    dump_semantics("Chapters");
}

void
ebml_chapters_converter_c::fix_xml(document_cptr &doc)
  const {
  auto result = doc->select_nodes("//ChapterAtom[not(ChapterTimeStart)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterTimeStart").append_child(pugi::node_pcdata).set_value(::format_timecode(0).c_str());

  result = doc->select_nodes("//ChapterDisplay[not(ChapterString)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterString");

  result = doc->select_nodes("//ChapterDisplay[not(ChapterLanguage)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterLanguage").append_child(pugi::node_pcdata).set_value("eng");
}

void
ebml_chapters_converter_c::fix_ebml(EbmlMaster &chapters)
  const {
  for (auto element : chapters)
    if (dynamic_cast<KaxEditionEntry *>(element))
      fix_edition_entry(static_cast<KaxEditionEntry &>(*element));
}

void
ebml_chapters_converter_c::fix_edition_entry(KaxEditionEntry &eentry)
  const {
  bool atom_found = false;

  KaxEditionUID *euid = nullptr;
  for (auto element : eentry)
    if (dynamic_cast<KaxEditionUID *>(element)) {
      euid = static_cast<KaxEditionUID *>(element);
      if (!is_unique_number(uint64(*euid), UNIQUE_EDITION_IDS)) {
        mxwarn(boost::format(Y("Chapter parser: The EditionUID %1% is not unique and could not be reused. A new one will be created.\n")) % uint64(*euid));
        *static_cast<EbmlUInteger *>(euid) = create_unique_number(UNIQUE_EDITION_IDS);
      }

    } else if (dynamic_cast<KaxChapterAtom *>(element)) {
      atom_found = true;
      fix_atom(static_cast<KaxChapterAtom &>(*element));
    }

  if (!atom_found)
    throw conversion_x{Y("At least one <ChapterAtom> element is needed.")};

  if (!euid) {
    euid                               = new KaxEditionUID;
    *static_cast<EbmlUInteger *>(euid) = create_unique_number(UNIQUE_EDITION_IDS);
    eentry.PushElement(*euid);
  }
}

void
ebml_chapters_converter_c::fix_atom(KaxChapterAtom &atom)
  const {
  for (auto element : atom)
    if (dynamic_cast<KaxChapterAtom *>(element))
      fix_atom(*static_cast<KaxChapterAtom *>(element));

  if (!FindChild<KaxChapterTimeStart>(atom))
    throw conversion_x{Y("<ChapterAtom> is missing the <ChapterTimeStart> child.")};

  if (!FindChild<KaxChapterUID>(atom)) {
    KaxChapterUID *cuid                = new KaxChapterUID;
    *static_cast<EbmlUInteger *>(cuid) = create_unique_number(UNIQUE_CHAPTER_IDS);
    atom.PushElement(*cuid);
  }

  KaxChapterTrack *ctrack = FindChild<KaxChapterTrack>(atom);
  if (ctrack && !FindChild<KaxChapterTrackNumber>(ctrack))
    throw conversion_x{Y("<ChapterTrack> is missing the <ChapterTrackNumber> child.")};

  KaxChapterDisplay *cdisplay = FindChild<KaxChapterDisplay>(atom);
  if (cdisplay)
    fix_display(*cdisplay);
}

void
ebml_chapters_converter_c::fix_display(KaxChapterDisplay &display)
  const {
  if (!FindChild<KaxChapterString>(display))
    throw conversion_x{Y("<ChapterDisplay> is missing the <ChapterString> child.")};

  KaxChapterLanguage *clanguage = FindChild<KaxChapterLanguage>(display);
  if (!clanguage) {
    clanguage                             = new KaxChapterLanguage;
    *static_cast<EbmlString *>(clanguage) = "und";
    display.PushElement(*clanguage);

  } else {
    int index = map_to_iso639_2_code(std::string(*clanguage));

    if (-1 == index)
      throw conversion_x{boost::format(Y("'%1%' is not a valid ISO639-2 language code.")) % std::string(*clanguage)};

  }

  KaxChapterCountry *ccountry = FindChild<KaxChapterCountry>(display);
  if (ccountry && !is_valid_cctld(std::string(*ccountry)))
    throw conversion_x{boost::format(Y("'%1%' is not a valid ccTLD country code.")) % std::string(*ccountry)};
}

void
ebml_chapters_converter_c::write_xml(KaxChapters &chapters,
                                     mm_io_c &out) {
  document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Chapters SYSTEM \"matroskachapters.dtd\"> ");

  ebml_chapters_converter_c converter;
  converter.to_xml(chapters, doc);

  std::stringstream out_stream;
  doc->save(out_stream, "  ", pugi::format_default | pugi::format_write_bom);
  out.puts(out_stream.str());
}

bool
ebml_chapters_converter_c::probe_file(std::string const &file_name) {
  try {
    mm_text_io_c in(new mm_file_io_c(file_name, MODE_READ));
    std::string line;

    while (in.getline2(line)) {
      // I assume that if it looks like XML then it is a XML chapter file :)
      strip(line);
      if (balg::istarts_with(line, "<?xml"))
        return true;
      else if (!line.empty())
        return false;
    }

  } catch (...) {
  }

  return false;
}

kax_chapters_cptr
ebml_chapters_converter_c::parse_file(std::string const &file_name,
                                      bool throw_on_error) {
  auto parse = [&]() {
    auto master = ebml_chapters_converter_c{}.to_ebml(file_name, "Chapters");
    sort_ebml_master(master.get());
    fix_mandatory_chapter_elements(static_cast<KaxChapters *>(master.get()));
    return std::dynamic_pointer_cast<KaxChapters>(master);
  };

  if (!throw_on_error)
    return parse();

  try {
    return parse();

  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The XML chapter file '%1%' could not be read.\n")) % file_name);

  } catch (mtx::xml::xml_parser_x &ex) {
    mxerror(boost::format(Y("The XML chapter file '%1%' contains an error at position %3%: %2%\n")) % file_name % ex.result().description() % ex.result().offset);

  } catch (mtx::xml::exception &ex) {
    mxerror(boost::format(Y("The XML chapter file '%1%' contains an error: %2%\n")) % file_name % ex.what());
  }

  return kax_chapters_cptr{};
}

}}
