/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks and other items from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "extract/track_spec.h"

track_spec_t::track_spec_t()
  : tid(0)
  , tuid(0)
  , extract_cuesheet(false)
  , target_mode(track_spec_t::tm_normal)
  , extract_blockadd_level(-1)
  , done(false)
{
}
