/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapters.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief chapter parser/writer functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __CHAPTERS_H
#define __CHAPTERS_H

#include <stdio.h>

#include <matroska/KaxChapters.h>

#include "common.h"
#include "mm_io.h"

using namespace libmatroska;

KaxChapters *MTX_DLL_API
parse_chapters(const char *file_name, int64_t min_tc = 0,
               int64_t max_tc = -1, int64_t offset = 0,
               const char *language = NULL,
               const char *charset = NULL,
               bool exception_on_error = false,
               bool *is_simple_format = NULL);

bool MTX_DLL_API probe_xml_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API parse_xml_chapters(mm_text_io_c *in, int64_t min_tc,
                                            int64_t max_tc, int64_t offset,
                                            bool exception_on_error = false);

bool MTX_DLL_API probe_simple_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API
parse_simple_chapters(mm_text_io_c *in, int64_t min_tc,
                      int64_t max_tc, int64_t offset,
                      const char *language, const char *charset,
                      bool exception_on_error = false);

extern char *MTX_DLL_API cue_to_chapter_name_format;
bool MTX_DLL_API probe_cue_chapters(mm_text_io_c *in);
KaxChapters *MTX_DLL_API parse_cue_chapters(mm_text_io_c *in, int64_t min_tc,
                                            int64_t max_tc, int64_t offset,
                                            const char *language,
                                            const char *charset,
                                            bool exception_on_error = false);

void MTX_DLL_API write_chapters_xml(KaxChapters *chapters, FILE *out);
void MTX_DLL_API write_chapters_simple(int &chapter_num, KaxChapters *chapters,
                                       FILE *out);

KaxChapters *MTX_DLL_API copy_chapters(KaxChapters *source);
KaxChapters *MTX_DLL_API
select_chapters_in_timeframe(KaxChapters *chapters, int64_t min_tc,
                             int64_t max_tc, int64_t offset, bool validate);

extern string MTX_DLL_API default_chapter_language, default_chapter_country;

#endif // __CHAPTERS_H

