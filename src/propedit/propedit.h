/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_PROPEDIT_H
#define __PROPEDIT_PROPEDIT_H

#include "common/os.h"

#include <string>
#include <vector>

#include <ebml/EbmlMaster.h>

#include "common/kax_analyzer.h"

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

class target_c {
public:
  std::vector<change_c> m_changes;
  std::string m_target_spec;
  EbmlMaster *m_target;

public:
  target_c();

  void validate();
};

class options_c {
public:
  std::string m_file_name;
  std::vector<target_c> m_targets;
  bool m_show_progress;
  kax_analyzer_c::parse_mode_e m_parse_mode;

public:
  options_c();

  void validate();
};

#endif // __PROPEDIT_PROPEDIT_H
