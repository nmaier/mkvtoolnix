/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>

#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/xml/ebml_tags_converter.h"
#include "propedit/propedit.h"
#include "propedit/tag_target.h"

using namespace libmatroska;

tag_target_c::tag_target_c()
  : target_c{tt_undefined}
  , m_tag_operation_mode(tag_target_c::tom_undefined)
{
}

tag_target_c::~tag_target_c() {
}

void
tag_target_c::validate() {
  if (!m_file_name.empty() && !m_new_tags)
    m_new_tags = mtx::xml::ebml_tags_converter_c::parse_file(m_file_name);
}

void
tag_target_c::parse_tags_spec(const std::string &spec) {
  m_spec                         = spec;
  std::vector<std::string> parts = split(spec, ":", 2);
  balg::to_lower(parts[0]);

  if (parts[0] == "all")
    m_tag_operation_mode = tom_all;

  else if (parts[0] == "global")
    m_tag_operation_mode = tom_global;

  else if (parts[0] == "track") {
    m_tag_operation_mode = tom_track;
    parts                = split(parts[1], ":", 2);
    parse_track_spec(balg::to_lower_copy(parts[0]));

  } else
    throw false;

  m_file_name = parts[1];
}

void
tag_target_c::dump_info()
  const
{
  mxinfo(boost::format("  tag_target:\n"
                       "    type:                 %1%\n"
                       "    selection_mode:       %2%\n"
                       "    selection_param:      %3%\n"
                       "    selection_track_type: %4%\n"
                       "    track_uid:            %5%\n"
                       "    file_name:            %6%\n")
         % static_cast<int>(m_type)
         % static_cast<int>(m_selection_mode)
         % m_selection_param
         % m_selection_track_type
         % m_track_uid
         % m_file_name);

  for (auto &change : m_changes)
    change->dump_info();
}

bool
tag_target_c::has_changes()
  const {
  return true;
}

bool
tag_target_c::non_track_target()
  const {
  return (tag_target_c::tom_all    == m_tag_operation_mode)
      || (tag_target_c::tom_global == m_tag_operation_mode);
}

bool
tag_target_c::track_target_with_sub_master()
  const {
  return true;
}

void
tag_target_c::execute() {
  if (tag_target_c::tom_all == m_tag_operation_mode)
    add_or_replace_all_master_elements(m_new_tags.get());

  else if (tag_target_c::tom_global == m_tag_operation_mode)
    add_or_replace_global_tags(m_new_tags.get());

  else if (tag_target_c::tom_track == m_tag_operation_mode)
    add_or_replace_track_tags(m_new_tags.get());

  else
    assert(false);

  if (m_level1_element->ListSize()) {
    fix_mandatory_tag_elements(m_level1_element);
    if (!m_level1_element->CheckMandatory())
      mxerror(boost::format(Y("Error parsing the tags in '%1%': some mandatory elements are missing.\n")) % m_file_name);
  }
}

void
tag_target_c::add_or_replace_global_tags(KaxTags *tags) {
  size_t idx = 0;
  while (m_level1_element->ListSize() > idx) {
    KaxTag *tag = dynamic_cast<KaxTag *>((*m_level1_element)[idx]);
    if (!tag || (-1 != get_tag_tuid(*tag)))
      ++idx;
    else {
      delete tag;
      m_level1_element->Remove(idx);
    }
  }

  if (tags) {
    idx = 0;
    while (tags->ListSize() > idx) {
      KaxTag *tag = dynamic_cast<KaxTag *>((*tags)[0]);
      if (!tag || (-1 != get_tag_tuid(*tag)))
        ++idx;
      else {
        m_level1_element->PushElement(*tag);
        tags->Remove(idx);
      }
    }
  }
}

void
tag_target_c::add_or_replace_track_tags(KaxTags *tags) {
  int64_t track_uid = static_cast<uint64>(GetChildAs<KaxTrackUID, EbmlUInteger>(m_sub_master));

  size_t idx = 0;
  while (m_level1_element->ListSize() > idx) {
    KaxTag *tag = dynamic_cast<KaxTag *>((*m_level1_element)[idx]);
    if (!tag || (track_uid != get_tag_tuid(*tag)))
      ++idx;
    else {
      delete tag;
      m_level1_element->Remove(idx);
    }
  }

  if (tags) {
    remove_track_uid_tag_targets(tags);

    idx = 0;
    while (tags->ListSize() > idx) {
      KaxTag *tag = dynamic_cast<KaxTag *>((*tags)[0]);
      if (!tag)
        ++idx;
      else {
        GetChildAs<KaxTagTrackUID, EbmlUInteger>(GetChild<KaxTagTargets>(tag)) = track_uid;
        m_level1_element->PushElement(*tag);
        tags->Remove(idx);
      }
    }
  }
}
