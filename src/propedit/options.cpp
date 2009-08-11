/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "propedit/options.h"

options_c::options_c()
  : m_show_progress(false)
  , m_parse_mode(kax_analyzer_c::parse_mode_full)
{
}

void
options_c::validate() {
}
