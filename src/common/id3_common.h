/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   helper functions for ID3 tags
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __ID3_COMMON_H
#define __ID3_COMMON_H

#include "os.h"

class mm_io_c;

int MTX_DLL_API skip_id3v2_tag(mm_io_c &io);
int MTX_DLL_API id3v1_tag_present_at_end(mm_io_c &io);
int MTX_DLL_API id3v2_tag_present_at_end(mm_io_c &io);
int MTX_DLL_API id3_tag_present_at_end(mm_io_c &io);

#endif /* __ID3_COMMON_H */
