/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   XML segment info parser functions

   Written by Steve Lhomme <steve.lhomme@free.fr> and
   Moritz Bunkus <moritz@bunkus.org>.
*/

#include <expat.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSegment.h>

#include "commonebml.h"
#include "error.h"
#include "iso639.h"
#include "mm_io.h"
#include "segmentinfo.h"
#include "xml_element_mapping.h"
#include "xml_element_parser.h"

using namespace std;
using namespace libmatroska;

// {{{ XML chapters

#define CPDATA (parser_data_t *)pdata

static void
end_segmentinfo_data(void *pdata) {
/* TODO verify that the FamilyUID is set
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
*/
}

static void
end_segmentinfo_family(void *pdata) {
  /* TODO verify that the FamilyUID is 128 bits */
}

static void
end_segmentinfo_links(void *pdata) {
  /* TODO verify that the ChapterLinkCodec and ChapterLinkID are set */
}

bool
probe_xml_segmentinfos(mm_text_io_c *in) {
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

KaxSegment *
parse_xml_segmentinfo(mm_text_io_c *in,
                      bool exception_on_error) {
  KaxSegment *segment;
  EbmlMaster *m;
  string error;
  int i;

  for (i = 0; segmentinfo_elements[i].name != NULL; i++) {
    segmentinfo_elements[i].start_hook = NULL;
    segmentinfo_elements[i].end_hook = NULL;
  }
  segmentinfo_elements[chapter_element_map_index("Info")].end_hook =
    end_segmentinfo_data;
  segmentinfo_elements[chapter_element_map_index("FamilyUID")].end_hook =
    end_segmentinfo_family;
  segmentinfo_elements[chapter_element_map_index("Links")].end_hook =
    end_segmentinfo_links;

  try {
    m = parse_xml_elements("Info", segmentinfo_elements, in);
    segment = dynamic_cast<KaxSegment *>(sort_ebml_master(m));
    assert(segment != NULL);
  } catch (error_c e) {
    if (!exception_on_error)
      mxerror("%s", e.get_error());
    error = (const char *)e;
    segment = NULL;
  }

  if ((segment != NULL) && (verbose > 1))
    debug_dump_elements(segment, 0);

  if (error.length() > 0)
    throw error_c(error);

  fix_mandatory_segmentinfo_elements(segment);

  return segment;
}

// }}}
