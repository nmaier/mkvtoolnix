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

using namespace libmatroska;

KaxChapters *parse_chapters(const char *file_name, int64_t min_tc = 0,
                            int64_t max_tc = -1, int64_t offset = 0);

void write_chapters_xml(KaxChapters *chapters, FILE *out);
void write_chapters_simple(int &chapter_num, KaxChapters *chapters, FILE *out);

#endif // __CHAPTERS_H

