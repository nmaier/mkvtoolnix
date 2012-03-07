/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML chapter parser functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <expat.h>
#include <ctype.h>
#include <stdarg.h>

#include <matroska/KaxChapters.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/unique_numbers.h"
#include "common/xml/element_mapping.h"
#include "common/xml/element_parser.h"

using namespace libmatroska;

static void
end_edition_entry(void *pdata) {
  KaxEditionUID *euid = nullptr;
  EbmlMaster *m       = static_cast<EbmlMaster *>(xmlp_pelt);
  size_t num           = 0;
  size_t i;

  for (i = 0; i < m->ListSize(); i++) {
    if (is_id((*m)[i], KaxEditionUID))
      euid = dynamic_cast<KaxEditionUID *>((*m)[i]);

    else if (is_id((*m)[i], KaxChapterAtom))
      num++;
  }

  if (0 == num)
    xmlp_error(CPDATA, Y("At least one <ChapterAtom> element is needed."));

  if (nullptr == euid) {
    euid = new KaxEditionUID;
    *static_cast<EbmlUInteger *>(euid) = create_unique_uint32(UNIQUE_EDITION_IDS);
    m->PushElement(*euid);
  }
}

static void
end_edition_uid(void *pdata) {
  KaxEditionUID *euid = static_cast<KaxEditionUID *>(xmlp_pelt);

  if (!is_unique_uint32(uint32(*euid), UNIQUE_EDITION_IDS)) {
    mxwarn(boost::format(Y("Chapter parser: The EditionUID %1% is not unique and could not be reused. A new one will be created.\n")) % uint32(*euid));
    *static_cast<EbmlUInteger *>(euid) = create_unique_uint32(UNIQUE_EDITION_IDS);
  }
}

static void
end_chapter_uid(void *pdata) {
  KaxChapterUID *cuid = static_cast<KaxChapterUID *>(xmlp_pelt);

  if (!is_unique_uint32(uint32(*cuid), UNIQUE_CHAPTER_IDS)) {
    mxwarn(boost::format(Y("Chapter parser: The ChapterUID %1% is not unique and could not be reused. A new one will be created.\n")) % uint32(*cuid));
    *static_cast<EbmlUInteger *>(cuid) = create_unique_uint32(UNIQUE_CHAPTER_IDS);
  }
}

static void
end_chapter_atom(void *pdata) {
  EbmlMaster *m = static_cast<EbmlMaster *>(xmlp_pelt);

  if (m->FindFirstElt(EBML_INFO(KaxChapterTimeStart), false) == nullptr)
    xmlp_error(CPDATA, Y("<ChapterAtom> is missing the <ChapterTimeStart> child."));

  if (m->FindFirstElt(EBML_INFO(KaxChapterUID), false) == nullptr) {
    KaxChapterUID *cuid = new KaxChapterUID;
    *static_cast<EbmlUInteger *>(cuid) = create_unique_uint32(UNIQUE_CHAPTER_IDS);
    m->PushElement(*cuid);
  }
}

static void
end_chapter_track(void *pdata) {
  EbmlMaster *m = static_cast<EbmlMaster *>(xmlp_pelt);

  if (m->FindFirstElt(EBML_INFO(KaxChapterTrackNumber), false) == nullptr)
    xmlp_error(CPDATA, Y("<ChapterTrack> is missing the <ChapterTrackNumber> child."));
}

static void
end_chapter_display(void *pdata) {
  EbmlMaster *m = static_cast<EbmlMaster *>(xmlp_pelt);

  if (m->FindFirstElt(EBML_INFO(KaxChapterString), false) == nullptr)
    xmlp_error(CPDATA, Y("<ChapterDisplay> is missing the <ChapterString> child."));

  if (m->FindFirstElt(EBML_INFO(KaxChapterLanguage), false) == nullptr) {
    KaxChapterLanguage *cl         = new KaxChapterLanguage;
    *static_cast<EbmlString *>(cl) = "und";
    m->PushElement(*cl);
  }
}

static void
end_chapter_language(void *pdata) {
  EbmlString *s = static_cast<EbmlString *>(xmlp_pelt);
  int index     = map_to_iso639_2_code(std::string(*s).c_str());

  if (-1 == index)
    xmlp_error(CPDATA, boost::format(Y("'%1%' is not a valid ISO639-2 language code.")) % std::string(*s));

  *s = iso639_languages[index].iso639_2_code;
}

static void
end_chapter_country(void *pdata) {
  EbmlString *s = static_cast<EbmlString *>(xmlp_pelt);

  if (!is_valid_cctld(std::string(*s).c_str()))
    xmlp_error(CPDATA, boost::format(Y("'%1%' is not a valid ccTLD country code.")) % std::string(*s));
}

static int
cet_index(const char *name) {
  int i;

  for (i = 0; chapter_elements[i].name != nullptr; i++)
    if (!strcmp(name, chapter_elements[i].name))
      return i;

  mxerror(boost::format(Y("cet_index: '%1%' not found\n")) % name);
  return -1;
}

bool
probe_xml_chapters(mm_text_io_c *in) {
  std::string s;

  in->setFilePointer(0);

  while (in->getline2(s)) {
    // I assume that if it looks like XML then it is a XML chapter file :)
    strip(s);
    if (!strncasecmp(s.c_str(), "<?xml", 5))
      return true;
    else if (!s.empty())
      return false;
  }

  return false;
}

KaxChapters *
parse_xml_chapters(mm_text_io_c *in,
                   int64_t min_tc,
                   int64_t max_tc,
                   int64_t offset,
                   bool exception_on_error) {
  KaxChapters *chapters;

  try {
    int i;
    for (i = 0; nullptr != chapter_elements[i].name; ++i) {
      chapter_elements[i].start_hook = nullptr;
      chapter_elements[i].end_hook   = nullptr;
    }

    chapter_elements[cet_index("EditionEntry")].end_hook    = end_edition_entry;
    chapter_elements[cet_index("EditionUID")].end_hook      = end_edition_uid;
    chapter_elements[cet_index("ChapterAtom")].end_hook     = end_chapter_atom;
    chapter_elements[cet_index("ChapterUID")].end_hook      = end_chapter_uid;
    chapter_elements[cet_index("ChapterTrack")].end_hook    = end_chapter_track;
    chapter_elements[cet_index("ChapterDisplay")].end_hook  = end_chapter_display;
    chapter_elements[cet_index("ChapterCountry")].end_hook  = end_chapter_country;
    chapter_elements[cet_index("ChapterLanguage")].end_hook = end_chapter_language;

    EbmlMaster *m = parse_xml_elements("Chapter", chapter_elements, in);
    chapters = dynamic_cast<KaxChapters *>(sort_ebml_master(m));
    assert(nullptr != chapters);
    chapters = select_chapters_in_timeframe(chapters, min_tc, max_tc, offset);

  } catch (mtx::xml::parser_x &e) {
    if (!exception_on_error)
      mxerror(e.error());
    throw;
  }

  fix_mandatory_chapter_elements(chapters);

  return chapters;
}
