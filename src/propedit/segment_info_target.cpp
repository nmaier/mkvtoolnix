/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>

#include "common/output.h"
#include "propedit/propedit.h"
#include "propedit/segment_info_target.h"

using namespace libmatroska;

segment_info_target_c::segment_info_target_c()
  : target_c()
{
}

segment_info_target_c::~segment_info_target_c() {
}

bool
segment_info_target_c::operator ==(target_c const &cmp)
  const {
  return dynamic_cast<segment_info_target_c const *>(&cmp);
}

void
segment_info_target_c::validate() {
  auto property_table = &property_element_c::get_table_for(KaxInfo::ClassInfos, nullptr, false);

  for (auto &change : m_changes)
    change->validate(property_table);
}

void
segment_info_target_c::add_change(change_c::change_type_e type,
                                  const std::string &spec) {
  m_changes.push_back(change_c::parse_spec(type, spec));
}

void
segment_info_target_c::dump_info()
  const {
  mxinfo(boost::format("  segment_info_target:\n"));

  for (auto &change : m_changes)
    change->dump_info();
}

bool
segment_info_target_c::has_changes()
  const {
  return !m_changes.empty();
}

void
segment_info_target_c::execute() {
  for (auto &change : m_changes)
    change->execute(m_master, m_sub_master);
}
