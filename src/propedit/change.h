/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_CHANGE_H
#define __PROPEDIT_CHANGE_H

#include "common/os.h"

#include <string>

class change_c {
public:
  enum change_type_e {
    ct_add,
    ct_set,
    ct_delete,
  };

  change_type_e m_type;
  std::string m_name, m_value;

public:
  change_c(change_type_e type, const std::string name, const std::string value);

  void validate();
};

#endif // __PROPEDIT_CHANGE_H
