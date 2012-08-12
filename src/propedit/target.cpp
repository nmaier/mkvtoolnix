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
#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/xml/ebml_tags_converter.h"
#include "propedit/propedit.h"
#include "propedit/target.h"

using namespace libmatroska;

#define INVALID_TRACK_TYPE static_cast<track_type>(0)

target_c::target_c(target_c::target_type_e type)
  : m_type(type)
  , m_selection_mode(target_c::sm_undefined)
  , m_tag_operation_mode(target_c::tom_undefined)
  , m_selection_param(0)
  , m_selection_track_type(INVALID_TRACK_TYPE)
  , m_level1_element(nullptr)
  , m_master(nullptr)
  , m_sub_master(nullptr)
  , m_track_uid(0)
  , m_track_type(INVALID_TRACK_TYPE)
{
}

target_c::~target_c() {
}

void
target_c::validate() {
  assert(target_c::tt_undefined != m_type);

  std::vector<property_element_c> *property_table
    = !m_level1_element ? nullptr
    :                     &property_element_c::get_table_for(target_c::tt_segment_info == m_type ? KaxInfo::ClassInfos : KaxTracks::ClassInfos,
                                                               track_audio == m_track_type ? &KaxTrackAudio::ClassInfos
                                                             : track_video == m_track_type ? &KaxTrackVideo::ClassInfos
                                                             :                               nullptr,
                                                             false);

  for (auto &change : m_changes)
    change->validate(property_table);
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

  m_spec = spec;
  balg::to_lower(spec);

  if ((spec == "segment_info") || (spec == "segmentinfo") || (spec == "info")) {
    m_type = target_c::tt_segment_info;
    return;
  }

  std::string prefix("track:");
  if (balg::istarts_with(spec, prefix)) {
    m_type = target_c::tt_track;
    parse_track_spec(spec.substr(prefix.length()));
    return;
  }

  throw false;
}

void
target_c::parse_track_spec(const std::string &spec) {
  static boost::regex track_re("^([absv=@]?)(\\d+)", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(spec, matches, track_re))
    throw false;

  std::string prefix = matches[1].str();
  parse_number(matches[2].str(), m_selection_param);

  m_selection_mode = prefix.empty() ? target_c::sm_by_position
                   : prefix == "="  ? target_c::sm_by_uid
                   : prefix == "@"  ? target_c::sm_by_number
                   :                  target_c::sm_by_type_and_position;

  if (target_c::sm_by_type_and_position == m_selection_mode) {
    m_selection_track_type = 'a' == prefix[0] ? track_audio
                           : 'v' == prefix[0] ? track_video
                           : 's' == prefix[0] ? track_subtitle
                           : 'b' == prefix[0] ? track_buttons
                           :                    INVALID_TRACK_TYPE;

    if (INVALID_TRACK_TYPE == m_selection_track_type)
      throw false;
  }
}

void
target_c::parse_tags_spec(const std::string &spec) {
  m_spec                         = spec;
  std::vector<std::string> parts = split(spec, ":", 2);
  balg::to_lower(parts[0]);

  if (parts[0] == "all")
    m_tag_operation_mode = target_c::tom_all;

  else if (parts[0] == "global")
    m_tag_operation_mode = target_c::tom_global;

  else if (parts[0] == "track") {
    m_tag_operation_mode = target_c::tom_track;
    parts                = split(parts[1], ":", 2);
    parse_track_spec(balg::to_lower_copy(parts[0]));

  } else
    throw false;

  m_file_name = parts[1];
}

void
target_c::dump_info()
  const
{
  mxinfo(boost::format("  target:\n"
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
target_c::operator ==(const target_c &cmp)
  const
{
  return (typeid(*this)          == typeid(cmp))
      && (m_type                 == cmp.m_type)
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
  return !m_changes.empty()
    || (target_c::tt_tags     == m_type);
}

bool
target_c::has_add_or_set_change()
  const
{
  for (auto &change : m_changes)
    if (change_c::ct_delete != change->m_type)
      return true;

  return false;
}

void
target_c::set_level1_element(ebml_element_cptr level1_element_cp,
                             ebml_element_cptr track_headers_cp) {
  m_level1_element_cp = level1_element_cp;
  m_level1_element    = static_cast<EbmlMaster *>(m_level1_element_cp.get());

  m_track_headers_cp  = track_headers_cp;

  if (   (target_c::tt_segment_info == m_type)
      || (   (target_c::tt_tags == m_type)
          && (   (target_c::tom_all    == m_tag_operation_mode)
              || (target_c::tom_global == m_tag_operation_mode)))) {
    m_master = m_level1_element;
    return;
  }

  assert(   (target_c::tt_track == m_type)
         || (   (target_c::tt_tags   == m_type)
             && (target_c::tom_track == m_tag_operation_mode)));

  std::map<uint8, unsigned int> num_tracks_by_type;
  unsigned int num_tracks_total = 0;

  if (!track_headers_cp)
    track_headers_cp = level1_element_cp;

  EbmlMaster *track_headers = static_cast<EbmlMaster *>(track_headers_cp.get());

  size_t i;
  for (i = 0; track_headers->ListSize() > i; ++i) {
    if (!is_id((*track_headers)[i], KaxTrackEntry))
      continue;

    KaxTrackEntry *track = dynamic_cast<KaxTrackEntry *>((*track_headers)[i]);
    assert(track);

    KaxTrackType *kax_track_type     = dynamic_cast<KaxTrackType *>(FindChild<KaxTrackType>(track));
    track_type this_track_type       = !kax_track_type ? track_video : static_cast<track_type>(uint8(*kax_track_type));

    KaxTrackUID *kax_track_uid       = dynamic_cast<KaxTrackUID *>(FindChild<KaxTrackUID>(track));
    uint64_t track_uid               = !kax_track_uid ? 0 : uint64(*kax_track_uid);

    KaxTrackNumber *kax_track_number = dynamic_cast<KaxTrackNumber *>(FindChild<KaxTrackNumber>(track));

    ++num_tracks_total;
    ++num_tracks_by_type[this_track_type];

    if (debugging_requested("track_matching"))
      mxinfo(boost::format("Testing match (mode %1% param %2% track_type %3%) (ntt %4% ntbt %5% type %7% uid %6%)\n")
             % m_selection_mode % m_selection_param % m_selection_track_type
             % num_tracks_total % num_tracks_by_type[this_track_type] % track_uid % this_track_type);

    bool track_matches = target_c::sm_by_uid      == m_selection_mode ? m_selection_param == track_uid
                       : target_c::sm_by_position == m_selection_mode ? m_selection_param == num_tracks_total
                       : target_c::sm_by_number   == m_selection_mode ? kax_track_number  && (m_selection_param == uint64(*kax_track_number))
                       :                                                (this_track_type == m_selection_track_type) && (m_selection_param == num_tracks_by_type[this_track_type]);

    if (!track_matches)
      continue;

    m_track_uid  = track_uid;
    m_track_type = this_track_type;
    m_master     = track;
    m_sub_master = track_video == m_track_type ? dynamic_cast<EbmlMaster *>(FindChild<KaxTrackVideo>(track))
                 : track_audio == m_track_type ? dynamic_cast<EbmlMaster *>(FindChild<KaxTrackAudio>(track))
                 :                               nullptr;

    if (   !m_sub_master
        && (   (track_video == m_track_type)
            || (track_audio == m_track_type))
        && has_add_or_set_change()) {
      m_sub_master = track_video == m_track_type ? static_cast<EbmlMaster *>(new KaxTrackVideo) : static_cast<EbmlMaster *>(new KaxTrackAudio);
      m_master->PushElement(*m_sub_master);
    }

    if (target_c::tt_tags == m_type) {
      m_master     = m_level1_element;
      m_sub_master = track;
    }

    return;
  }

  mxerror(boost::format(Y("No track corresponding to the edit specification '%1%' was found. %2%\n")) % m_spec % FILE_NOT_MODIFIED);
}

void
target_c::execute() {
  if (target_c::tt_tags == m_type) {
    add_or_replace_tags();
    return;
  }

  for (auto &change : m_changes)
    change->execute(m_master, m_sub_master);
}

void
target_c::add_or_replace_tags() {
  kax_tags_cptr new_tags;

  if (!m_file_name.empty())
    new_tags = mtx::xml::ebml_tags_converter_c::parse_file(m_file_name);

  if (target_c::tom_all == m_tag_operation_mode)
    add_or_replace_all_master_elements(new_tags.get());

  else if (target_c::tom_global == m_tag_operation_mode)
    add_or_replace_global_tags(new_tags.get());

  else if (target_c::tom_track == m_tag_operation_mode)
    add_or_replace_track_tags(new_tags.get());

  else
    assert(false);

  if (m_level1_element->ListSize()) {
    fix_mandatory_tag_elements(m_level1_element);
    if (!m_level1_element->CheckMandatory())
      mxerror(boost::format(Y("Error parsing the tags in '%1%': some mandatory elements are missing.\n")) % m_file_name);
  }
}

void
target_c::add_or_replace_all_master_elements(EbmlMaster *source) {
  size_t idx;
  for (idx = 0; m_level1_element->ListSize() > idx; ++idx)
    delete (*m_level1_element)[idx];
  m_level1_element->RemoveAll();

  if (source) {
    size_t idx;
    for (idx = 0; source->ListSize() > idx; ++idx)
      m_level1_element->PushElement(*(*source)[idx]);
    source->RemoveAll();
  }
}

void
target_c::add_or_replace_global_tags(KaxTags *tags) {
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
target_c::add_or_replace_track_tags(KaxTags *tags) {
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
