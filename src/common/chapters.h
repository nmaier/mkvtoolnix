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

KaxChapters *parse_chapters(const char *file_name, int64_t min_tc = 0,
                            int64_t max_tc = -1, int64_t offset = 0,
                            const char *language = NULL,
                            const char *charset = NULL,
                            bool exception_on_error = false,
                            bool *is_simple_format = NULL);

bool probe_xml_chapters(mm_text_io_c *in);
KaxChapters *parse_xml_chapters(mm_text_io_c *in, int64_t min_tc,
                                int64_t max_tc, int64_t offset,
                                bool exception_on_error = false);

bool probe_simple_chapters(mm_text_io_c *in);
KaxChapters *parse_simple_chapters(mm_text_io_c *in, int64_t min_tc,
                                   int64_t max_tc, int64_t offset,
                                   const char *language,
                                   const char *charset,
                                   bool exception_on_error = false);


void write_chapters_xml(KaxChapters *chapters, FILE *out);
void write_chapters_simple(int &chapter_num, KaxChapters *chapters, FILE *out);

KaxChapters *copy_chapters(KaxChapters *source);
KaxChapters *select_chapters_in_timeframe(KaxChapters *chapters,
                                          int64_t min_tc, int64_t max_tc,
                                          int64_t offset, bool validate);

#endif // __CHAPTERS_H

