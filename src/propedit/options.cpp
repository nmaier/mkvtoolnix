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
  , m_parse_mode(kax_analyzer_c::parse_mode_fast)
{
}

void
options_c::validate() {
  if (m_file_name.empty())
    mxerror(Y("No file name given.\n"));

  if (!has_changes())
    mxerror(Y("Nothing to do.\n"));
}

target_cptr
options_c::add_target(target_c::target_type_e type,
                      const std::string &spec) {
  target_cptr target(new target_c);
  target->m_type = type;
  target->parse_target_spec(spec);

  if (!m_targets.empty()) {
    std::vector<target_cptr>::iterator target_it;
    mxforeach(target_it, m_targets)
      if (**target_it == *target)
        return *target_it;
  }

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
    mxerror(boost::format(Y("More than one file name has been given ('%1%' and '%2%').\n")) % m_file_name % file_name);

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

void
options_c::dump_info()
  const
{
  mxinfo(boost::format("options:\n"
                       "  file_name:     %1%\n"
                       "  show_progress: %2%\n"
                       "  parse_mode:    %3%\n")
         % m_file_name
         % m_show_progress
         % static_cast<int>(m_parse_mode));

  std::vector<target_cptr>::const_iterator target_it;
  mxforeach(target_it, m_targets)
    (*target_it)->dump_info();
}

bool
options_c::has_changes()
  const
{
  std::vector<target_cptr>::const_iterator target_it;
  mxforeach(target_it, m_targets)
    if ((*target_it)->has_changes())
      return true;

  return false;
}
