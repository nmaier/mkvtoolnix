/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <boost/regex.hpp>
#include <cassert>
#include <stdexcept>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/common_pch.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "propedit/propedit.h"
#include "propedit/target.h"

using namespace libmatroska;

#define INVALID_TRACK_TYPE static_cast<track_type>(0)

target_c::target_c(target_c::target_type_e type)
  : m_type(type)
  , m_selection_mode(target_c::sm_undefined)
  , m_selection_param(0)
  , m_selection_track_type(INVALID_TRACK_TYPE)
  , m_level1_element(NULL)
  , m_master(NULL)
  , m_sub_master(NULL)
  , m_track_uid(0)
  , m_track_type(INVALID_TRACK_TYPE)
{
}

void
target_c::validate() {
  assert(target_c::tt_undefined != m_type);

  std::vector<property_element_c> *property_table
    = NULL == m_level1_element ? NULL
    :                            &property_element_c::get_table_for(target_c::tt_segment_info == m_type ? KaxInfo::ClassInfos : KaxTracks::ClassInfos,
                                                                      track_audio == m_track_type ? &KaxTrackAudio::ClassInfos
                                                                    : track_video == m_track_type ? &KaxTrackVideo::ClassInfos
                                                                    :                               NULL,
                                                                    false);

  std::vector<change_cptr>::iterator change_it;
  mxforeach(change_it, m_changes)
    (*change_it)->validate(property_table);
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
  spec   = downcase(spec);

  if ((spec == "segment_info") || (spec == "segmentinfo") || (spec == "info")) {
    m_type = target_c::tt_segment_info;
    return;
  }

  std::string prefix("track:");
  if (starts_with_case(spec, prefix)) {
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
  parse_uint(matches[2].str(), m_selection_param);

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
target_c::dump_info()
  const
{
  mxinfo(boost::format("  target:\n"
                       "    type:                 %1%\n"
                       "    selection_mode:       %2%\n"
                       "    selection_param:      %3%\n"
                       "    selection_track_type: %4%\n"
                       "    track_uid:            %5%\n")
         % static_cast<int>(m_type)
         % static_cast<int>(m_selection_mode)
         % m_selection_param
         % m_selection_track_type
         % m_track_uid);

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

bool
target_c::has_add_or_set_change()
  const
{
  std::vector<change_cptr>::const_iterator change_it;
  mxforeach(change_it, m_changes)
    if (change_c::ct_delete != (*change_it)->m_type)
      return true;

  return false;
}

void
target_c::set_level1_element(EbmlMaster *level1_element) {
  m_level1_element = level1_element;

  if (target_c::tt_segment_info == m_type) {
    m_master = m_level1_element;
    return;
  }

  if (target_c::tt_track != m_type)
    assert(false);

  std::map<uint8, int> num_tracks_by_type;
  int num_tracks_total = 0;

  int i;
  for (i = 0; level1_element->ListSize() > i; ++i) {
    if (!is_id((*level1_element)[i], KaxTrackEntry))
      continue;

    KaxTrackEntry *track = dynamic_cast<KaxTrackEntry *>((*level1_element)[i]);
    assert(NULL != track);

    KaxTrackType *kax_track_type     = dynamic_cast<KaxTrackType *>(FINDFIRST(track, KaxTrackType));
    track_type this_track_type       = NULL == kax_track_type ? track_video : static_cast<track_type>(uint8(*kax_track_type));

    KaxTrackUID *kax_track_uid       = dynamic_cast<KaxTrackUID *>(FINDFIRST(track, KaxTrackUID));
    uint64_t track_uid               = NULL == kax_track_uid ? 0 : uint64(*kax_track_uid);

    KaxTrackNumber *kax_track_number = dynamic_cast<KaxTrackNumber *>(FINDFIRST(track, KaxTrackNumber));

    ++num_tracks_total;
    ++num_tracks_by_type[this_track_type];

    if (debugging_requested("track_matching"))
      mxinfo(boost::format("Testing match (mode %1% param %2% track_type %3%) (ntt %4% ntbt %5% type %7% uid %6%)\n")
             % m_selection_mode % m_selection_param % m_selection_track_type
             % num_tracks_total % num_tracks_by_type[this_track_type] % track_uid % this_track_type);

    bool track_matches = target_c::sm_by_uid      == m_selection_mode ? m_selection_param == track_uid
                       : target_c::sm_by_position == m_selection_mode ? m_selection_param == num_tracks_total
                       : target_c::sm_by_number   == m_selection_mode ? (NULL            != kax_track_number)       && (m_selection_param == uint64(*kax_track_number))
                       :                                                (this_track_type == m_selection_track_type) && (m_selection_param == num_tracks_by_type[this_track_type]);

    if (!track_matches)
      continue;

    m_track_uid  = track_uid;
    m_track_type = this_track_type;
    m_master     = track;
    m_sub_master = track_video == m_track_type ? dynamic_cast<EbmlMaster *>(FINDFIRST(track, KaxTrackVideo))
                 : track_audio == m_track_type ? dynamic_cast<EbmlMaster *>(FINDFIRST(track, KaxTrackAudio))
                 :                               NULL;

    if (   (NULL == m_sub_master)
        && (   (track_video == m_track_type)
            || (track_audio == m_track_type))
        && has_add_or_set_change()) {
      m_sub_master = track_video == m_track_type ? static_cast<EbmlMaster *>(new KaxTrackVideo) : static_cast<EbmlMaster *>(new KaxTrackAudio);
      m_master->PushElement(*m_sub_master);
    }

   return;
  }

  mxerror(boost::format(Y("No track corresponding to the edit specification '%1%' was found. %2%\n")) % m_spec % FILE_NOT_MODIFIED);
}

void
target_c::execute() {
  std::vector<change_cptr>::iterator change_it;
  mxforeach(change_it, m_changes)
    (*change_it)->execute(m_master, m_sub_master);
}

