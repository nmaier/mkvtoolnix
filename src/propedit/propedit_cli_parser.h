/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_PROPEDIT_CLI_PARSER_H
#define __PROPEDIT_PROPEDIT_CLI_PARSER_H

#include "common/common_pch.h"

#include "common/cli_parser.h"
#include "propedit/options.h"

class propedit_cli_parser_c: public cli_parser_c {
protected:
  options_cptr m_options;
  target_cptr m_target;

public:
  propedit_cli_parser_c(const std::vector<std::string> &args);

  options_cptr run();

protected:
  void init_parser();

  void add_target();
  void add_change();
  void add_tags();
  void add_chapters();
  void set_parse_mode();
  void set_file_name();

  void list_property_names();
  void list_property_names_for_table(const std::vector<property_element_c> &table, const std::string &title, const std::string &edit_spec);

  std::map<property_element_c::ebml_type_e, const char *> &get_ebml_type_abbrev_map();
};

#endif // __PROPEDIT_PROPEDIT_CLI_PARSER_H
