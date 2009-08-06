/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/regex.hpp>

#include "common/common.h"
#include "common/strings/editing.h"
#include "propedit/target.h"

target_c::target_c()
  : m_type(target_c::tt_undefined)
  , m_selection_mode(target_c::sm_undefined)
  , m_selection_param(-1)
  , m_target(NULL)
{
}

void
target_c::validate() {
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
      throw Y("missing value");

    name  = parts[0];
    value = parts[1];
  }

  if (name.empty())
    throw Y("missing property name");

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
}

void
target_c::parse_track_spec(const std::string &spec) {
  boost::regex track_re("^([[:alpha:]]+)?(_[[:alpha:]]+)?(\\.[^@]+)?(@.+)?", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(spec, matches, track_re))
    throw false;
}
