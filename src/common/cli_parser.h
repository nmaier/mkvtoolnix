/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMON_CLI_PARSER_H
#define __COMMON_CLI_PARSER_H

#include "common/os.h"

#include <boost/function.hpp>
#include <map>
#include <string>
#include <vector>

typedef boost::function<void(void)> cli_parser_cb_t;

class cli_parser_c {
protected:
  struct option_t {
    std::string m_name, m_description;
    cli_parser_cb_t m_callback;
    bool m_needs_arg;

    option_t();
    option_t(std::string name, std::string description, cli_parser_cb_t callback, bool needs_arg);
  };

  std::map<std::string, option_t> m_options;
  std::vector<std::string> m_args;

  std::string m_current_arg, m_next_arg;

  cli_parser_cb_t m_default_callback;

protected:
  cli_parser_c(const std::vector<std::string> &args);
  virtual ~cli_parser_c();

  virtual void add_option(std::string spec, cli_parser_cb_t callback, const std::string &description);
  virtual void set_default_callback(cli_parser_cb_t callback);
  virtual void parse_args();
  virtual void set_usage() = 0;
};

#endif // __COMMON_CLI_PARSER_H
