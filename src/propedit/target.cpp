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
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/output.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "propedit/propedit.h"
#include "propedit/target.h"

using namespace libmatroska;

target_c::target_c()
  : m_level1_element{}
  , m_master{}
  , m_sub_master{}
  , m_track_uid{}
  , m_track_type(INVALID_TRACK_TYPE)
{
}

target_c::~target_c() {
}

bool
target_c::operator !=(target_c const &cmp)
  const {
  return !(*this == cmp);
}

void
target_c::add_change(change_c::change_type_e,
                     std::string const &) {
}

void
target_c::set_level1_element(ebml_element_cptr level1_element_cp,
                             ebml_element_cptr track_headers_cp) {
  m_level1_element_cp = level1_element_cp;
  m_level1_element    = static_cast<EbmlMaster *>(m_level1_element_cp.get());

  m_track_headers_cp  = track_headers_cp;
  m_master            = m_level1_element;
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

std::string const &
target_c::get_spec()
  const {
  return m_spec;
}

uint64_t
target_c::get_track_uid()
  const {
  return m_track_uid;
}

EbmlMaster *
target_c::get_level1_element()
  const {
  return m_level1_element;
}
