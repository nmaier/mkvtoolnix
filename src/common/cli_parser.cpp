/** \brief command line parsing

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/bind.hpp>
#include <string>

#include "common/cli_parser.h"
#include "common/command_line.h"
#include "common/common.h"
#include "common/strings/editing.h"
#include "common/translation.h"

#define INDENT_COLUMN_OPTION_NAME         2
#define INDENT_COLUMN_OPTION_DESCRIPTION 30
#define INDENT_COLUMN_SECTION_HEADER      1
#define WRAP_COLUMN                      79

cli_parser_c::option_t::option_t()
  : m_needs_arg(false)
{
}

cli_parser_c::option_t::option_t(cli_parser_c::option_t::option_type_e type,
                                 const translatable_string_c &description)
  : m_type(type)
  , m_description(description)
  , m_needs_arg(false)
{
}

cli_parser_c::option_t::option_t(const std::string &spec,
                                 const translatable_string_c &description,
                                 cli_parser_cb_t callback,
                                 bool needs_arg)
  : m_type(cli_parser_c::option_t::ot_option)
  , m_spec(spec)
  , m_description(description)
  , m_callback(callback)
  , m_needs_arg(needs_arg)
{
}

std::string
cli_parser_c::option_t::format_text() {
  std::string text;
  std::string description = m_description.get_translated();
  int indent_column       = 0;
  int current_column      = 0;

  if (cli_parser_c::option_t::ot_option == m_type) {
    indent_column  = INDENT_COLUMN_OPTION_DESCRIPTION;
    text           = std::string(INDENT_COLUMN_OPTION_NAME, ' ') + m_name;
    current_column = text.length();

  } else if (cli_parser_c::option_t::ot_section_header == m_type) {
    indent_column  = INDENT_COLUMN_SECTION_HEADER;
    text           = "\n";
    description   += ":";
  }

  if ((0 != indent_column) && (indent_column <= current_column)) {
    text           += "\n";
    current_column  = 0;
  }

  std::string indent(indent_column, ' ');
  text                                += std::string(indent_column - current_column, ' ');
  current_column                       = utf8_strlen(text);
  std::string::size_type current_pos   = 0;
  bool first_word_in_line              = true;
  bool needs_space                     = false;
  const char * break_chars             = " ,.)/:";

  while (description.length() > current_pos) {
    std::string::size_type word_start = description.find_first_not_of(" ", current_pos);
    if (std::string::npos == word_start)
      break;

    std::string::size_type word_end = description.find_first_of(break_chars, word_start);
    char next_needs_space           = false;
    if (std::string::npos == word_end)
      word_end = description.length();

    else if (description[word_end] != ' ')
      ++word_end;

    else
      next_needs_space = true;

    std::string word     = description.substr(word_start, word_end - word_start);
    bool needs_space_now = needs_space && (0 != word.find_first_of(break_chars));

    if (!first_word_in_line && ((current_column + (needs_space_now ? 0 : 1) + utf8_strlen(word)) >= WRAP_COLUMN)) {
      text               += "\n" + indent;
      current_column      = indent_column;
      first_word_in_line  = true;
    }

    if (!first_word_in_line && needs_space_now) {
      text += " ";
      ++current_column;
    }

    text               += word;
    current_column     += utf8_strlen(word);
    current_pos         = word_end;
    first_word_in_line  = false;
    needs_space         = next_needs_space;
  }

  text += "\n";

  return text;
}

// ------------------------------------------------------------

cli_parser_c::cli_parser_c(const std::vector<std::string> &args)
  : m_args(args)
{
  m_hooks[cli_parser_c::ht_common_options_parsed] = std::vector<cli_parser_cb_t>();
  m_hooks[cli_parser_c::ht_unknown_option]        = std::vector<cli_parser_cb_t>();
}

void
cli_parser_c::parse_args() {
  set_usage();
  while (handle_common_cli_args(m_args, ""))
    set_usage();

  run_hooks(cli_parser_c::ht_common_options_parsed);

  std::vector<std::string>::const_iterator sit;
  mxforeach(sit, m_args) {
    bool no_next_arg = (sit + 1) == m_args.end();
    m_current_arg    = *sit;
    m_next_arg       = no_next_arg ? "" : *(sit + 1);

    std::map<std::string, cli_parser_c::option_t>::iterator option_it(m_option_map.find(m_current_arg));
    if (option_it != m_option_map.end()) {
      cli_parser_c::option_t &option(option_it->second);

      if (option.m_needs_arg) {
        if (no_next_arg)
          mxerror(boost::format(Y("Missing argument to '%1%'.\n")) % m_current_arg);
        ++sit;
      }

      option.m_callback();

    } else if (!run_hooks(cli_parser_c::ht_unknown_option))
      mxerror(boost::format(Y("Unknown option '%1%'.\n")) % m_current_arg);
  }
}

void
cli_parser_c::add_option(const std::string &spec,
                         cli_parser_cb_t callback,
                         const translatable_string_c &description) {
  std::vector<std::string> parts = split(spec, "=", 2);
  bool needs_arg                 = parts.size() == 2;
  cli_parser_c::option_t option(spec, description, callback, needs_arg);

  std::vector<std::string> names = split(parts[0], "|");
  std::vector<std::string>::iterator name;
  mxforeach(name, names) {
    std::string full_name = '@' == (*name)[0]     ? *name
                          : 1   == name->length() ? std::string( "-") + *name
                          :                         std::string("--") + *name;

    if (map_has_key(m_option_map, full_name))
      mxerror(boost::format("cli_parser_c::add_option(): Programming error: option '%1%' is already used for spec '%2%' "
                            "and cannot be used for spec '%3%'.\n") % full_name % m_option_map[full_name].m_spec % spec);

    m_option_map[full_name] = option;

    if (!option.m_name.empty())
      option.m_name += ", ";
    option.m_name += full_name;
  }

  if (needs_arg)
    option.m_name += " " + parts[1];

  m_options.push_back(option);
}

void
cli_parser_c::add_section_header(const translatable_string_c &title) {
  m_options.push_back(cli_parser_c::option_t(cli_parser_c::option_t::ot_section_header, title));
}

void
cli_parser_c::add_information(const translatable_string_c &information) {
  m_options.push_back(cli_parser_c::option_t(cli_parser_c::option_t::ot_information, information));
}

void
cli_parser_c::add_separator() {
  m_options.push_back(cli_parser_c::option_t(cli_parser_c::option_t::ot_information, translatable_string_c("")));
}

#define OPT(name, description) add_option(name, boost::bind(&cli_parser_c::dummy_callback, this), description)

void
cli_parser_c::add_common_options() {
  OPT("v|verbose",                      YT("Increase verbosity."));
  OPT("ui-language=<code>",             YT("Force the translations for 'code' to be used."));
  OPT("command-line-charset=<charset>", YT("Charset for strings on the command line"));
  OPT("output-charset=<cset>",          YT("Output messages in this charset"));
  OPT("r|redirect-output=<file>",       YT("Redirects all messages into this file."));
  OPT("@file",                          YT("Reads additional command line options from the specified file (see man page)."));
  OPT("h|help",                         YT("Show this help."));
  OPT("V|version",                      YT("Show version information."));
}

#undef OPT

void
cli_parser_c::set_usage() {
  usage_text = "";
  std::vector<cli_parser_c::option_t>::iterator option_it;
  mxforeach(option_it, m_options)
    usage_text += option_it->format_text();
}

void
cli_parser_c::dummy_callback() {
  mxerror(boost::format("cli_parser_c::dummy_callback(): %1%") % BUGMSG);
}

void
cli_parser_c::add_hook(cli_parser_c::hook_type_e hook_type,
                       const cli_parser_cb_t &callback) {
  m_hooks[hook_type].push_back(callback);
}

bool
cli_parser_c::run_hooks(cli_parser_c::hook_type_e hook_type) {
  if (m_hooks[hook_type].empty())
    return false;

  std::vector<cli_parser_cb_t>::iterator hook_it;
  mxforeach(hook_it, m_hooks[hook_type])
    if (*hook_it)
      (*hook_it)();

  return true;
}
