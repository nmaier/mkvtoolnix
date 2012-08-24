/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_OPTIONS_H
#define MTX_PROPEDIT_OPTIONS_H

#include "common/common_pch.h"

#include <ebml/EbmlMaster.h>

#include "common/kax_analyzer.h"
#include "propedit/attachment_target.h"
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

  target_cptr add_track_or_segmentinfo_target(std::string const &spec);
  void add_tags(const std::string &spec);
  void add_chapters(const std::string &spec);
  void add_attachment_command(attachment_target_c::command_e command, std::string const &spec, attachment_target_c::options_t const &options);
  void set_file_name(const std::string &file_name);
  void set_parse_mode(const std::string &parse_mode);
  void dump_info() const;
  bool has_changes() const;

  void find_elements(kax_analyzer_c *analyzer);

  void execute();

protected:
  void remove_empty_targets();
  void merge_targets();
};
typedef std::shared_ptr<options_c> options_cptr;

#endif // MTX_PROPEDIT_OPTIONS_H
