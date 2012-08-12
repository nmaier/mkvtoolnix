/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/output.h"
#include "common/strings/parsing.h"
#include "propedit/propedit.h"
#include "propedit/track_target.h"

using namespace libmatroska;

track_target_c::track_target_c(std::string const &spec)
  : target_c{}
  , m_selection_mode{track_target_c::sm_undefined}
  , m_selection_param{}
  , m_selection_track_type{INVALID_TRACK_TYPE}
{
  m_spec = spec;
}

track_target_c::~track_target_c() {
}

bool
track_target_c::operator ==(target_c const &cmp)
  const {
  auto other_track = dynamic_cast<track_target_c const *>(&cmp);

  return other_track
      && (m_selection_mode       == other_track->m_selection_mode)
      && (m_selection_param      == other_track->m_selection_param)
      && (m_selection_track_type == other_track->m_selection_track_type);
}

void
track_target_c::validate() {
  if (INVALID_TRACK_TYPE == m_track_type)
    return;

  auto property_table = &property_element_c::get_table_for(KaxTracks::ClassInfos,
                                                             track_audio == m_track_type ? &KaxTrackAudio::ClassInfos
                                                           : track_video == m_track_type ? &KaxTrackVideo::ClassInfos
                                                           :                               nullptr,
                                                           false);

  for (auto &change : m_changes)
    change->validate(property_table);
}

void
track_target_c::add_change(change_c::change_type_e type,
                           const std::string &spec) {
  m_changes.push_back(change_c::parse_spec(type, spec));
}

void
track_target_c::dump_info()
  const {
  mxinfo(boost::format("  track_target:\n"
                       "    selection_mode:       %1%\n"
                       "    selection_param:      %2%\n"
                       "    selection_track_type: %3%\n"
                       "    track_uid:            %4%\n"
                       "    file_name:            %5%\n")
         % static_cast<int>(m_selection_mode)
         % m_selection_param
         % m_selection_track_type
         % m_track_uid
         % m_file_name);

  for (auto &change : m_changes)
    change->dump_info();
}

bool
track_target_c::has_changes()
  const {
  return !m_changes.empty();
}

bool
track_target_c::has_add_or_set_change()
  const {
  for (auto &change : m_changes)
    if (change_c::ct_delete != change->m_type)
      return true;

  return false;
}

void
track_target_c::execute() {
  for (auto &change : m_changes)
    change->execute(m_master, m_sub_master);
}

void
track_target_c::set_level1_element(ebml_element_cptr level1_element_cp,
                                   ebml_element_cptr track_headers_cp) {
  m_level1_element_cp = level1_element_cp;
  m_level1_element    = static_cast<EbmlMaster *>(m_level1_element_cp.get());

  m_track_headers_cp  = track_headers_cp;

  if (non_track_target()) {
    m_master = m_level1_element;
    return;
  }

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

    bool track_matches = sm_by_uid      == m_selection_mode ? m_selection_param == track_uid
                       : sm_by_position == m_selection_mode ? m_selection_param == num_tracks_total
                       : sm_by_number   == m_selection_mode ? kax_track_number  && (m_selection_param == uint64(*kax_track_number))
                       :                                      (this_track_type == m_selection_track_type) && (m_selection_param == num_tracks_by_type[this_track_type]);

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

    if (sub_master_is_track()) {
      m_master     = m_level1_element;
      m_sub_master = track;
    }

    return;
  }

  mxerror(boost::format(Y("No track corresponding to the edit specification '%1%' was found. %2%\n")) % m_spec % FILE_NOT_MODIFIED);
}

void
track_target_c::parse_spec(std::string const &spec) {
  static boost::regex track_re("^([absv=@]?)(\\d+)", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(spec, matches, track_re))
    throw false;

  std::string prefix = matches[1].str();
  parse_number(matches[2].str(), m_selection_param);

  m_selection_mode = prefix.empty() ? sm_by_position
                   : prefix == "="  ? sm_by_uid
                   : prefix == "@"  ? sm_by_number
                   :                  sm_by_type_and_position;

  if (sm_by_type_and_position == m_selection_mode) {
    m_selection_track_type = 'a' == prefix[0] ? track_audio
                           : 'v' == prefix[0] ? track_video
                           : 's' == prefix[0] ? track_subtitle
                           : 'b' == prefix[0] ? track_buttons
                           :                    INVALID_TRACK_TYPE;

    if (INVALID_TRACK_TYPE == m_selection_track_type)
      throw false;
  }
}

bool
track_target_c::non_track_target()
  const {
  return false;
}

bool
track_target_c::sub_master_is_track()
  const {
  return false;
}

bool
track_target_c::requires_sub_master()
  const {
  return (   (track_video == m_track_type)
          || (track_audio == m_track_type))
      && has_add_or_set_change();
}

void
track_target_c::merge_changes(track_target_c &other) {
  m_changes.insert(m_changes.begin(), other.m_changes.begin(), other.m_changes.end());
}
