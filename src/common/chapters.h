/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   chapter parser/writer functions
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __CHAPTERS_H
#define __CHAPTERS_H

#include <stdio.h>

#include "ebml/EbmlElement.h"

#include "common.h"
#include "mm_io.h"

namespace libebml {
  class EbmlMaster;
};

namespace libmatroska {
  class KaxChapters;
  class KaxTags;
  class KaxEditionEntry;
  class KaxChapterAtom;
};

using namespace libebml;
using namespace libmatroska;

KaxChapters *MTX_DLL_API
parse_chapters(const string &file_name, int64_t min_tc = 0,
               int64_t max_tc = -1, int64_t offset = 0,
               const string &language = "",
               const string &charset = "",
               bool exception_on_error = false,
               bool *is_simple_format = NULL,
               KaxTags **tags = NULL);

KaxChapters *MTX_DLL_API
parse_chapters(mm_text_io_c *io, int64_t min_tc = 0,
               int64_t max_tc = -1, int64_t offset = 0,
               const string &language = "",
               const string &charset = "",
               bool exception_on_error = false,
               bool *is_simple_format = NULL,
               KaxTags **tags = NULL);

bool MTX_DLL_API probe_xml_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API parse_xml_chapters(mm_text_io_c *in, int64_t min_tc,
                                            int64_t max_tc, int64_t offset,
                                            bool exception_on_error = false);

bool MTX_DLL_API probe_simple_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API
parse_simple_chapters(mm_text_io_c *in, int64_t min_tc,
                      int64_t max_tc, int64_t offset,
                      const string &language, const string &charset,
                      bool exception_on_error = false);

extern string MTX_DLL_API cue_to_chapter_name_format;
bool MTX_DLL_API probe_cue_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API parse_cue_chapters(mm_text_io_c *in, int64_t min_tc,
                                            int64_t max_tc, int64_t offset,
                                            const string &language,
                                            const string &charset,
                                            bool exception_on_error = false,
                                            KaxTags **tags = NULL);


void MTX_DLL_API write_chapters_xml(KaxChapters *chapters, mm_io_c *out);
void MTX_DLL_API write_chapters_simple(int &chapter_num, KaxChapters *chapters,
                                       mm_io_c *out);

#define copy_chapters(source) dynamic_cast<KaxChapters *>(source->Clone())
KaxChapters *MTX_DLL_API
select_chapters_in_timeframe(KaxChapters *chapters, int64_t min_tc,
                             int64_t max_tc, int64_t offset);

extern string MTX_DLL_API default_chapter_language, default_chapter_country;

int64_t MTX_DLL_API get_chapter_uid(KaxChapterAtom &atom);
int64_t MTX_DLL_API get_chapter_start(KaxChapterAtom &atom);
string MTX_DLL_API get_chapter_name(KaxChapterAtom &atom);

void MTX_DLL_API fix_mandatory_chapter_elements(EbmlElement *e);
KaxEditionEntry *MTX_DLL_API find_edition_with_uid(KaxChapters &chapters,
                                                   uint64_t uid);
KaxChapterAtom *MTX_DLL_API find_chapter_with_uid(KaxChapters &chapters,
                                                  uint64_t uid);

void MTX_DLL_API move_chapters_by_edition(KaxChapters &dst, KaxChapters &src);
void MTX_DLL_API adjust_chapter_timecodes(EbmlMaster &master, int64_t offset);
#endif // __CHAPTERS_H

