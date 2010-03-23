/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>

#include "extract/mkvextract.h"
#include "extract/options.h"

options_c::options_c()
  : m_simple_chapter_format(false)
  , m_parse_mode(kax_analyzer_c::parse_mode_fast)
  , m_extraction_mode(options_c::em_unknown)
{
}
