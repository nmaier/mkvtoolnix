/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of global variables found in mkvmerge.cpp

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MKVMERGE_H
#define MTX_MKVMERGE_H

#include "merge/pr_generic.h"

int64_t create_track_number(generic_reader_c *reader, int64_t tid);
void parse_arg_split_chapters(std::string const &arg);

#endif // MTX_MKVMERGE_H
