/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   the mmg_options_t struct

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wx.h>

#include "common/wx.h"
#include "mmg/mmg.h"

void
mmg_options_t::validate() {
  if (   (ODM_FROM_FIRST_INPUT_FILE != output_directory_mode)
      && (ODM_PREVIOUS              != output_directory_mode)
      && (ODM_FIXED                 != output_directory_mode))
    output_directory_mode = ODM_FROM_FIRST_INPUT_FILE;

  if (   (priority != wxU("highest")) && (priority != wxU("higher")) && (priority != wxU("normal"))
      && (priority != wxU("lower"))   && (priority != wxU("lowest")))
    priority = wxU("normal");
}

