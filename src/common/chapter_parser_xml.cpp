/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   XML chapter parser functions
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <expat.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "chapters.h"
#include "commonebml.h"
#include "error.h"
#include "iso639.h"
#include "mm_io.h"
#include "xml_element_mapping.h"
#include "xml_element_parser.h"

using namespace std;
using namespace libmatroska;

// {{{ XML chapters

#define CPDATA (parser_data_t *)pdata

static void
end_edition_entry(void *pdata) {
  EbmlMaster *m;
  KaxEditionUID *euid;
  int i, num;

  m = static_cast<EbmlMaster *>(xmlp_pelt);
  num = 0;
  euid = NULL;
  for (i = 0; i < m->ListSize(); i++) {
    if (is_id((*m)[i], KaxEditionUID))
      euid = dynamic_cast<KaxEditionUID *>((*m)[i]);
    else if (is_id((*m)[i], KaxChapterAtom))
      num++;
  }
  if (num == 0)
    xmlp_error(CPDATA, "At least one <ChapterAtom> element is needed.");
  if (euid == NULL) {
    euid = new KaxEditionUID;
    *static_cast<EbmlUInteger *>(euid) =
      create_unique_uint32(UNIQUE_EDITION_IDS);
    m->PushElement(*euid);
  }
}

static void
end_edition_uid(void *pdata) {
  KaxEditionUID *euid;

  euid = static_cast<KaxEditionUID *>(xmlp_pelt);
  if (!is_unique_uint32(uint32(*euid), UNIQUE_EDITION_IDS)) {
    mxwarn("Chapter parser: The EditionUID %u is not unique and could "
           "not be reused. A new one will be created.\n", uint32(*euid));
    *static_cast<EbmlUInteger *>(euid) =
      create_unique_uint32(UNIQUE_EDITION_IDS);
  }
}

static void
end_chapter_uid(void *pdata) {
  KaxChapterUID *cuid;

  cuid = static_cast<KaxChapterUID *>(xmlp_pelt);
  if (!is_unique_uint32(uint32(*cuid), UNIQUE_CHAPTER_IDS)) {
    mxwarn("Chapter parser: The ChapterUID %u is not unique and could "
           "not be reused. A new one will be created.\n", uint32(*cuid));
    *static_cast<EbmlUInteger *>(cuid) =
      create_unique_uint32(UNIQUE_CHAPTER_IDS);
  }
}

static void
end_chapter_atom(void *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(xmlp_pelt);
  if (m->FindFirstElt(KaxChapterTimeStart::ClassInfos, false) == NULL)
    xmlp_error(CPDATA, "<ChapterAtom> is missing the <ChapterTimeStart> "
               "child.");

  if (m->FindFirstElt(KaxChapterUID::ClassInfos, false) == NULL) {
    KaxChapterUID *cuid;

    cuid = new KaxChapterUID;
    *static_cast<EbmlUInteger *>(cuid) =
      create_unique_uint32(UNIQUE_CHAPTER_IDS);
    m->PushElement(*cuid);
  }
}

static void
end_chapter_track(void *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(xmlp_pelt);
  if (m->FindFirstElt(KaxChapterTrackNumber::ClassInfos, false) == NULL)
    xmlp_error(CPDATA, "<ChapterTrack> is missing the <ChapterTrackNumber> "
               "child.");
}

static void
end_chapter_display(void *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(xmlp_pelt);
  if (m->FindFirstElt(KaxChapterString::ClassInfos, false) == NULL)
    xmlp_error(CPDATA, "<ChapterDisplay> is missing the <ChapterString> "
               "child.");
  if (m->FindFirstElt(KaxChapterLanguage::ClassInfos, false) == NULL) {
    KaxChapterLanguage *cl;

    cl = new KaxChapterLanguage;
    *static_cast<EbmlString *>(cl) = "und";
    m->PushElement(*cl);
  }
}

static void
end_chapter_language(void *pdata) {
  EbmlString *s;

  s = static_cast<EbmlString *>(xmlp_pelt);
  if (!is_valid_iso639_2_code(string(*s).c_str()))
    xmlp_error(CPDATA, "'%s' is not a valid ISO639-2 language code.",
               string(*s).c_str());
}

static void
end_chapter_country(void *pdata) {
  EbmlString *s;

  s = static_cast<EbmlString *>(xmlp_pelt);
  if (!is_valid_iso639_1_code(string(*s).c_str()))
    xmlp_error(CPDATA, "'%s' is not a valid ISO639-1 country code.",
               string(*s).c_str());
}

bool
probe_xml_chapters(mm_text_io_c *in) {
  string s;

  in->setFilePointer(0);

  while (in->getline2(s)) {
    // I assume that if it looks like XML then it is a XML chapter file :)
    strip(s);
    if (!strncasecmp(s.c_str(), "<?xml", 5))
      return true;
    else if (s.length() > 0)
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
  EbmlMaster *m;
  string error;
  int i;

  for (i = 0; chapter_elements[i].name != NULL; i++) {
    chapter_elements[i].start_hook = NULL;
    chapter_elements[i].end_hook = NULL;
  }
  chapter_elements[chapter_element_map_index("EditionEntry")].end_hook =
    end_edition_entry;
  chapter_elements[chapter_element_map_index("EditionUID")].end_hook =
    end_edition_uid;
  chapter_elements[chapter_element_map_index("ChapterAtom")].end_hook =
    end_chapter_atom;
  chapter_elements[chapter_element_map_index("ChapterUID")].end_hook =
    end_chapter_uid;
  chapter_elements[chapter_element_map_index("ChapterTrack")].end_hook =
    end_chapter_track;
  chapter_elements[chapter_element_map_index("ChapterDisplay")].end_hook =
    end_chapter_display;
  chapter_elements[chapter_element_map_index("ChapterLanguage")].end_hook =
    end_chapter_language;
  chapter_elements[chapter_element_map_index("ChapterCountry")].end_hook =
    end_chapter_country;

  try {
    m = parse_xml_elements("Chapter", chapter_elements, in);
    chapters = dynamic_cast<KaxChapters *>(sort_ebml_master(m));
    assert(chapters != NULL);
    chapters = select_chapters_in_timeframe(chapters, min_tc, max_tc,
                                            offset);
  } catch (error_c e) {
    if (!exception_on_error)
      mxerror("%s", e.get_error());
    error = (const char *)e;
    chapters = NULL;
  }

  if ((chapters != NULL) && (verbose > 1))
    debug_dump_elements(chapters, 0);

  if (error.length() > 0)
    throw error_c(error);

  fix_mandatory_chapter_elements(chapters);

  return chapters;
}

// }}}
