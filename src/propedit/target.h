/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_TARGET_H
#define __PROPEDIT_TARGET_H

#include "common/os.h"

#include <string>
#include <vector>

#include <ebml/EbmlMaster.h>

#include "propedit/change.h"

class target_c {
public:
  std::vector<change_c> m_changes;
  std::string m_target_spec;
  EbmlMaster *m_target;

public:
  target_c();

  void validate();
};

#endif // __PROPEDIT_TARGET_H
