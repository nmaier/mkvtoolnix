/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "propedit/options.h"

options_c::options_c()
  : m_show_progress(false)
  , m_parse_mode(kax_analyzer_c::parse_mode_full)
{
}

void
options_c::validate() {
  if (m_file_name.empty())
    mxerror(Y("No file name given.\n"));
}

target_cptr
options_c::add_target(target_c::target_type_e type,
                      const std::string &spec) {
  target_cptr target(new target_c);
  target->m_type = type;
  target->parse_target_spec(spec);

  m_targets.push_back(target);

  return target;
}

target_cptr
options_c::add_target(target_c::target_type_e type) {
  return add_target(type, "");
}

target_cptr
options_c::add_target(const std::string &spec) {
  return add_target(target_c::tt_undefined, spec);
}

void
options_c::set_file_name(const std::string &file_name) {
  if (!m_file_name.empty())
    mxinfo(boost::format(Y("More than one file name has been given ('%1%' and '%2%').")) % m_file_name % file_name);

  m_file_name = file_name;
}

void
options_c::set_parse_mode(const std::string &parse_mode) {
  if (parse_mode == "full")
    m_parse_mode = kax_analyzer_c::parse_mode_full;

  else if (parse_mode == "fast")
    m_parse_mode = kax_analyzer_c::parse_mode_fast;

  else
    throw false;
}
