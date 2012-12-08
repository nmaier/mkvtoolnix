/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for ID3 tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_ID3_COMMON_H
#define MTX_COMMON_ID3_COMMON_H

#include "common/common_pch.h"

int skip_id3v2_tag(mm_io_c &io);
int id3v1_tag_present_at_end(mm_io_c &io);
int id3v2_tag_present_at_end(mm_io_c &io);
int id3_tag_present_at_end(mm_io_c &io);

#endif /* __ID3_COMMON_H */
