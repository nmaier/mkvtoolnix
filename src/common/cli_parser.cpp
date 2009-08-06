/** \brief command line parsing

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <string>

#include "common/cli_parser.h"
#include "common/command_line.h"
#include "common/common.h"
#include "common/strings/editing.h"

cli_parser_c::option_t::option_t()
  : m_needs_arg(false)
{
}

cli_parser_c::option_t::option_t(std::string name,
                                 std::string description,
                                 cli_parser_cb_t callback,
                                 bool needs_arg)
  : m_name(name)
  , m_description(description)
  , m_callback(callback)
  , m_needs_arg(needs_arg)
{
}

cli_parser_c::cli_parser_c(const std::vector<std::string> &args)
  : m_args(args)
{
}

cli_parser_c::~cli_parser_c() {
}

void
cli_parser_c::parse_args() {
  set_usage();
  while (handle_common_cli_args(m_args, ""))
    set_usage();

  std::vector<std::string>::const_iterator sit;
  mxforeach(sit, m_args) {
    bool no_next_arg = (sit + 1) == m_args.end();
    m_current_arg    = *sit;
    m_next_arg       = no_next_arg ? "" : *(sit + 1);

    std::map<std::string, cli_parser_c::option_t>::iterator option_it(m_options.find(m_current_arg));
    if (option_it != m_options.end()) {
      cli_parser_c::option_t &option(option_it->second);

      if (option.m_needs_arg) {
        if (no_next_arg)
          mxerror(boost::format(Y("Missing argument to '%1%'.\n")) % m_current_arg);
        ++sit;
      }

      option.m_callback();

    } else if (m_default_callback)
      m_default_callback();

    else
      mxerror(boost::format(Y("Unknown option '%1%'.\n")) % m_current_arg);
  }
}

void
cli_parser_c::add_option(std::string spec,
                         cli_parser_cb_t callback,
                         const std::string &description) {
  bool needs_arg = false;

  if ((2 <= spec.length()) && (spec.substr(spec.length() - 2, 2) == "=s")) {
    needs_arg = true;
    spec.erase(spec.length() - 2);
  }

  cli_parser_c::option_t option(spec, description, callback, needs_arg);

  std::vector<std::string> parts = split(spec, "|");
  std::vector<std::string>::iterator name;
  mxforeach(name, parts) {
    std::string full_name = 1 == name->length() ? std::string("-") + *name : std::string("--") + *name;
    m_options[full_name]  = option;
  }
}

void
cli_parser_c::set_default_callback(cli_parser_cb_t callback) {
  m_default_callback = callback;
}
