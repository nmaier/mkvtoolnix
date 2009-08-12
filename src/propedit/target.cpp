/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/regex.hpp>
#include <stdexcept>

#include "common/common.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "propedit/target.h"

target_c::target_c()
  : m_type(target_c::tt_undefined)
  , m_selection_mode(target_c::sm_undefined)
  , m_selection_param(-1)
  , m_selection_track_type(' ')
  , m_target(NULL)
{
}

void
target_c::validate() {
  std::vector<change_cptr>::iterator change_it;
  mxforeach(change_it, m_changes)
    (*change_it)->validate();
}

void
target_c::add_change(change_c::change_type_e type,
                     const std::string &spec) {
  std::string name, value;
  if (change_c::ct_delete == type)
    name = spec;

  else {
    std::vector<std::string> parts = split(spec, "=", 2);
    if (2 != parts.size())
      throw std::runtime_error(Y("missing value"));

    name  = parts[0];
    value = parts[1];
  }

  if (name.empty())
    throw std::runtime_error(Y("missing property name"));

  m_changes.push_back(change_cptr(new change_c(type, name, value)));
}

void
target_c::parse_target_spec(std::string spec) {
  if (spec.empty())
    return;

  spec = downcase(spec);

  if ((spec == "segment_info") || (spec == "segmentinfo") || (spec == "info")) {
    m_type = target_c::tt_segment_info;
    return;
  }

  std::string prefix("track:");
  if (starts_with_case(spec, prefix)) {
    parse_track_spec(spec.substr(prefix.length()));
    return;
  }

  throw false;
}

void
target_c::parse_track_spec(const std::string &spec) {
  boost::regex track_re("^([absv=\\+]?)(\\d+)", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(spec, matches, track_re))
    throw false;

  std::string prefix = matches[1].str();
  parse_int(matches[2].str(), m_selection_param);

  m_selection_mode = prefix.empty() ? target_c::sm_by_number
                   : prefix == "="  ? target_c::sm_by_uid
                   : prefix == "+"  ? target_c::sm_by_position
                   :                  target_c::sm_by_type_and_position;
  if (target_c::sm_by_type_and_position == m_selection_mode)
    m_selection_track_type = prefix[0];
}

void
target_c::dump_info()
  const
{
  mxinfo(boost::format("  target:\n"
                       "    type:                 %1%\n"
                       "    selection_mode:       %2%\n"
                       "    selection_param:      %3%\n"
                       "    selection_track_type: %4%\n")
         % static_cast<int>(m_type)
         % static_cast<int>(m_selection_mode)
         % m_selection_param
         % m_selection_track_type);

  std::vector<change_cptr>::const_iterator change_it;
  mxforeach(change_it, m_changes)
    (*change_it)->dump_info();
}

bool
target_c::operator ==(const target_c &cmp)
  const
{
  return (m_type                 == cmp.m_type)
      && (m_selection_mode       == cmp.m_selection_mode)
      && (m_selection_param      == cmp.m_selection_param)
      && (m_selection_track_type == cmp.m_selection_track_type);
}

bool
target_c::operator !=(const target_c &cmp)
  const
{
  return !(*this == cmp);
}

bool
target_c::has_changes()
  const
{
  return !m_changes.empty();
}
