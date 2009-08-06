/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "propedit/change.h"

change_c::change_c(change_c::change_type_e type,
                   const std::string &name,
                   const std::string &value)
  : m_type(type)
  , m_name(name)
  , m_value(value)
{
}

void
change_c::validate() {
}
