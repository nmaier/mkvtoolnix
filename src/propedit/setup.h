/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PROPEDIT_SETUP_H
#define __PROPEDIT_SETUP_H

#include "common/os.h"

#include <string>
#include <vector>

#include "common/cli_parser.h"
#include "propedit/options.h"

void setup();

class propedit_cli_parser_c: public cli_parser_c {
protected:
  options_cptr m_options;
  target_cptr m_target;

public:
  propedit_cli_parser_c(const std::vector<std::string> &args);
  virtual ~propedit_cli_parser_c();

  virtual options_cptr run();

protected:
  void add_option(const std::string &spec, void (propedit_cli_parser_c::*callback)(), const std::string &description);

  virtual void set_usage();

  void add_target();
  void add_change();
  void set_parse_mode();
  void set_file_name();
  void list_property_names();
};

#endif // __PROPEDIT_SETUP_H
