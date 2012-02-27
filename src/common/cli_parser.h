/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMON_CLI_PARSER_H
#define __COMMON_CLI_PARSER_H

#include "common/common_pch.h"

#include "common/translation.h"

#define INDENT_DEFAULT -1

typedef boost::function<void(void)> cli_parser_cb_t;

class cli_parser_c {
protected:
  struct option_t {
    enum option_type_e {
      ot_option,
      ot_section_header,
      ot_information,
      ot_informational_option,
    };

    option_type_e m_type;
    std::string m_spec, m_name;
    translatable_string_c m_description;
    cli_parser_cb_t m_callback;
    bool m_needs_arg;
    int m_indent;

    option_t();
    option_t(option_type_e type, const translatable_string_c &description, int indent = INDENT_DEFAULT);
    option_t(const std::string &spec, const translatable_string_c &description, cli_parser_cb_t callback, bool needs_arg);
    option_t(const std::string &name, const translatable_string_c &description);

    std::string format_text();
  };

  enum hook_type_e {
    ht_common_options_parsed,
    ht_unknown_option,
  };

  std::map<std::string, option_t> m_option_map;
  std::vector<option_t> m_options;
  std::vector<std::string> m_args;

  std::string m_current_arg, m_next_arg;

  std::map<hook_type_e, std::vector<cli_parser_cb_t>> m_hooks;

protected:
  cli_parser_c(const std::vector<std::string> &args);

  void add_option(const std::string &spec, cli_parser_cb_t callback, const translatable_string_c &description);
  void add_informational_option(const std::string &name, const translatable_string_c &description);
  void add_section_header(const translatable_string_c &title, int indent = INDENT_DEFAULT);
  void add_information(const translatable_string_c &information, int indent = INDENT_DEFAULT);
  void add_separator();
  void add_common_options();

  void parse_args();
  void set_usage();

  void dummy_callback();

  void add_hook(hook_type_e hook_type, const cli_parser_cb_t &callback);
  bool run_hooks(hook_type_e hook_type);
};

#endif // __COMMON_CLI_PARSER_H
