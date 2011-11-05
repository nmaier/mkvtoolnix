/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   chapter parser/writer functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_CHAPTERS_H
#define __MTX_COMMON_CHAPTERS_H

#include "common/os.h"

#include <string>

#include <ebml/EbmlElement.h>

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

class mm_io_c;
class mm_text_io_c;

KaxChapters *
parse_chapters(const std::string &file_name, int64_t min_tc = 0, int64_t max_tc = -1, int64_t offset = 0, const std::string &language = "", const std::string &charset = "",
               bool exception_on_error = false, bool *is_simple_format = NULL, KaxTags **tags = NULL);

KaxChapters *
parse_chapters(mm_text_io_c *io, int64_t min_tc = 0, int64_t max_tc = -1, int64_t offset = 0, const std::string &language = "", const std::string &charset = "",
               bool exception_on_error = false, bool *is_simple_format = NULL, KaxTags **tags = NULL);

bool probe_xml_chapters(mm_text_io_c *in);
KaxChapters *parse_xml_chapters(mm_text_io_c *in, int64_t min_tc, int64_t max_tc, int64_t offset, bool exception_on_error = false);

bool probe_simple_chapters(mm_text_io_c *in);
KaxChapters *parse_simple_chapters(mm_text_io_c *in, int64_t min_tc, int64_t max_tc, int64_t offset, const std::string &language, const std::string &charset, bool exception_on_error = false);

extern std::string g_cue_to_chapter_name_format;
bool probe_cue_chapters(mm_text_io_c *in);
KaxChapters *parse_cue_chapters(mm_text_io_c *in, int64_t min_tc, int64_t max_tc, int64_t offset, const std::string &language, const std::string &charset,
                                            bool exception_on_error = false, KaxTags **tags = NULL);

void write_chapters_xml(KaxChapters *chapters, mm_io_c *out);
void write_chapters_simple(int &chapter_num, KaxChapters *chapters, mm_io_c *out);

#define copy_chapters(source) dynamic_cast<KaxChapters *>(source->Clone())
KaxChapters *select_chapters_in_timeframe(KaxChapters *chapters, int64_t min_tc, int64_t max_tc, int64_t offset);

extern std::string g_default_chapter_language, g_default_chapter_country;

int64_t get_chapter_uid(KaxChapterAtom &atom);
int64_t get_chapter_start(KaxChapterAtom &atom, int64_t value_if_not_found = -1);
int64_t get_chapter_end(KaxChapterAtom &atom, int64_t value_if_not_found = -1);
std::string get_chapter_name(KaxChapterAtom &atom);

void fix_mandatory_chapter_elements(EbmlElement *e);
KaxEditionEntry *find_edition_with_uid(KaxChapters &chapters, uint64_t uid);
KaxChapterAtom *find_chapter_with_uid(KaxChapters &chapters, uint64_t uid);

void move_chapters_by_edition(KaxChapters &dst, KaxChapters &src);
void adjust_chapter_timecodes(EbmlMaster &master, int64_t offset);
void merge_chapter_entries(EbmlMaster &master);
int count_chapter_atoms(EbmlMaster &master);

void align_chapter_edition_uids(KaxChapters *chapters);
void align_chapter_edition_uids(KaxChapters &reference, KaxChapters &modify);

#endif // __MTX_COMMON_CHAPTERS_H
