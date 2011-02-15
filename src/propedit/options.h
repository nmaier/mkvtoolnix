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
  std::vector<target_cptr> m_targets;
  bool m_show_progress;
  kax_analyzer_c::parse_mode_e m_parse_mode;

public:
  options_c();

  void validate();
  void options_parsed();

  target_cptr add_target(target_c::target_type_e type);
  target_cptr add_target(const std::string &spec);
  void add_tags(const std::string &spec);
  void set_file_name(const std::string &file_name);
  void set_parse_mode(const std::string &parse_mode);
  void dump_info() const;
  bool has_changes() const;

  void find_elements(kax_analyzer_c *analyzer);

  void execute();

protected:
  target_cptr add_target(target_c::target_type_e type, const std::string &spec);
  void remove_empty_targets();
  void merge_targets();
};
typedef counted_ptr<options_c> options_cptr;

#endif // __PROPEDIT_OPTIONS_H
