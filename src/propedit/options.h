/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_OPTIONS_H
#define __PROPEDIT_OPTIONS_H

#include "common/os.h"

#include <string>
#include <vector>

#include <ebml/EbmlMaster.h>

#include "common/kax_analyzer.h"
#include "propedit/target.h"

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

#endif // __PROPEDIT_OPTIONS_H
