/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   high level Matroska file reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <ebml/EbmlStream.h>

#include "common/kax_file.h"
#include "common/vint.h"

kax_file_c::kax_file_c(mm_io_cptr &in)
  : m_in(in)
  , m_resynced(false)
  , m_resync_start_pos(0)
  , m_file_size(m_in->get_size())
  , m_es(counted_ptr<EbmlStream>(new EbmlStream(*m_in.get_object())))
{
}

EbmlElement *
kax_file_c::read_next_level1_element(uint32_t wanted_id) {
  m_resynced         = false;
  m_resync_start_pos = 0;

  bool debug_this    = debugging_requested("kax_file") || debugging_requested("kax_file_read_next");

  // Enough space left to read the ID?
  int64_t remaining_size = m_file_size - m_in->getFilePointer();
  if (4 > remaining_size)
    return NULL;

  // Easiest case: next level 1 element following the previous one
  // without any element inbetween.
  int64_t search_start_pos = m_in->getFilePointer();
  uint32_t actual_id       = m_in->read_uint32_be();
  m_in->setFilePointer(search_start_pos, seek_beginning);

  if (debug_this)
    mxinfo(boost::format("kax_file::read_next_level1_element(): SEARCH AT %1% ACT ID %|2$04x|\n") % search_start_pos % actual_id);

  if ((wanted_id == actual_id) || ((0 == wanted_id) && is_level1_element_id(actual_id))) {
    int upper_lvl_el = 0;
    EbmlElement *l1  = m_es->FindNextElement(KaxSegment::ClassInfos.Context, upper_lvl_el, 0xFFFFFFFFL, true);
    EbmlElement *l2  = NULL;

    l1->Read(*m_es.get_object(), KaxCluster::ClassInfos.Context, upper_lvl_el, l2, true);
    delete l2;

    return l1;
  }

  // If a specific level 1 is wanted then look for next ID by skipping
  // other known level 1 elements. If that files fallback to a
  // byte-for-byte search for the ID.
  if ((0 != wanted_id) && is_level1_element_id(actual_id)) {
    m_in->setFilePointer(search_start_pos, seek_beginning);
    int upper_lvl_el = 0;
    EbmlElement *l1  = m_es->FindNextElement(KaxSegment::ClassInfos.Context, upper_lvl_el, 0xFFFFFFFFL, true);

    if ((NULL != l1) && (EbmlId(*l1).Value == actual_id)) {
      // Found another level 1 element, just as we thought we
      // would. Seek to its end and restart the search from there.

      int upper_lvl_el = 0;
      EbmlElement *l2  = NULL;
      l1->Read(*m_es.get_object(), KaxSegment::ClassInfos.Context, upper_lvl_el, l2, true);
      bool ok = m_in->setFilePointer2(l1->GetElementPosition() + l1->ElementSize(), seek_beginning);
      delete l2;

      if (debug_this)
        mxinfo(boost::format("kax_file::read_next_level1_element(): other level 1 element %1% new pos %2% fsize %3% epos %4% esize %5%\n")
               % l1->Generic().DebugName  % (l1->GetElementPosition() + l1->ElementSize()) % m_file_size
               % l1->GetElementPosition() % l1->ElementSize());

      return ok ? read_next_level1_element(wanted_id) : NULL;
    }
  }

  // Last case: no valid ID found. Try a byte-for-byte search for the
  // next wanted/level 1 ID. Also try to locate at least three valid
  // ID/sizes, not just one ID.
  m_in->setFilePointer(search_start_pos, seek_beginning);
  return resync_to_level1_element(wanted_id);
}

bool
kax_file_c::is_level1_element_id(const EbmlId &id) const {
  return is_level1_element_id(static_cast<uint32_t>(id.Value));
}

bool
kax_file_c::is_level1_element_id(uint32_t id) const {
  const EbmlSemanticContext &context = KaxSegment::ClassInfos.Context;
  for (int segment_idx = 0; context.Size > segment_idx; ++segment_idx)
    if (context.MyTable[segment_idx].GetCallbacks.GlobalId.Value == id)
      return true;

  return false;
}

EbmlElement *
kax_file_c::resync_to_level1_element(uint32_t wanted_id) {
  m_resynced         = true;
  m_resync_start_pos = m_in->getFilePointer();

  bool debug_this    = debugging_requested("kax_file") || debugging_requested("kax_file_resync");
  uint32_t actual_id = m_in->read_uint32_be();

  while (m_in->getFilePointer() < m_file_size) {
    actual_id = (actual_id << 8) | m_in->read_uint8();

    if (   ((0 != wanted_id) && (wanted_id != actual_id))
        || ((0 == wanted_id) && !is_level1_element_id(actual_id)))
      continue;

    int64_t current_start_pos = m_in->getFilePointer() - 4;
    int64_t element_pos       = current_start_pos;
    unsigned int num_headers  = 1;

    if (debug_this)
      mxinfo(boost::format("kax_file::resync_to_level1_element(): byte-for-byte search, found level 1 ID %|2$04x| at %1%\n") % current_start_pos % actual_id);

    try {
      unsigned int idx;
      for (idx = 0; 3 > idx; ++idx) {
        vint_c length = vint_c::read(m_in);

        if (debug_this)
          mxinfo(boost::format("kax_file::resync_to_level1_element():   read ebml length %1%/%2% valid? %3%\n") % length.m_value % length.m_coded_size % length.is_valid());

        if (   !length.is_valid()
            || ((element_pos + length.m_value + length.m_coded_size + 2 * 4) >= m_file_size)
            || !m_in->setFilePointer2(element_pos + 4 + length.m_value + length.m_coded_size, seek_beginning))
          break;

        element_pos      = m_in->getFilePointer();
        uint32_t next_id = m_in->read_uint32_be();

        if (debug_this)
          mxinfo(boost::format("kax_file::resync_to_level1_element():   next ID is %|1$04x| at %2%\n") % next_id % element_pos);

        if (   ((0 != wanted_id) && (wanted_id != next_id))
            || ((0 == wanted_id) && !is_level1_element_id(next_id)))
          break;

        ++num_headers;
      }
    } catch (...) {
    }

    if (4 == num_headers) {
      m_in->setFilePointer(current_start_pos, seek_beginning);
      return read_next_level1_element(wanted_id);
    }

    m_in->setFilePointer(current_start_pos + 4, seek_beginning);
  }

  return NULL;
}

KaxCluster *
kax_file_c::resync_to_cluster() {
  return static_cast<KaxCluster *>(resync_to_level1_element(KaxCluster::ClassInfos.GlobalId.Value));
}

KaxCluster *
kax_file_c::read_next_cluster() {
  return static_cast<KaxCluster *>(read_next_level1_element(KaxCluster::ClassInfos.GlobalId.Value));
}

bool
kax_file_c::was_resynced() const {
  return m_resynced;
}

int64_t
kax_file_c::get_resync_start_pos() const {
  return m_resync_start_pos;
}
