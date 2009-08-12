/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/output.h"
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

void
change_c::dump_info()
  const
{
  mxinfo(boost::format("    change:\n"
                       "      type:  %1%\n"
                       "      name:  %2%\n"
                       "      value: %3%\n")
         % static_cast<int>(m_type)
         % m_name
         % m_value);
}

bool
change_c::lookup_property(std::vector<property_element_c> &table) {
  std::vector<property_element_c>::iterator property_it;
  mxforeach(property_it, table)
    if (property_it->m_name == m_name) {
      m_property = *property_it;
      return true;
    }

  return false;
}
