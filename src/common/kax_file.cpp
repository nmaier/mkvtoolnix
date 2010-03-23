/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   high level Matroska file reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ebml/EbmlCrc32.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>

#include "common/ebml.h"
#include "common/kax_file.h"

kax_file_c::kax_file_c(mm_io_cptr &in)
  : m_in(in)
  , m_resynced(false)
  , m_resync_start_pos(0)
  , m_file_size(m_in->get_size())
  , m_es(new EbmlStream(*m_in))
  , m_debug_read_next(debugging_requested("kax_file") || debugging_requested("kax_file_read_next"))
  , m_debug_resync(debugging_requested("kax_file") || debugging_requested("kax_file_resync"))
{
}

EbmlElement *
kax_file_c::read_next_level1_element(uint32_t wanted_id) {
  try {
    return read_next_level1_element_internal(wanted_id);
  } catch (...) {
    mxinfo("READ X\n");
    return NULL;
  }
}

kax_file_c::~kax_file_c() {
}

EbmlElement *
kax_file_c::read_next_level1_element_internal(uint32_t wanted_id) {
  m_resynced         = false;
  m_resync_start_pos = 0;

  // Read the next ID.
  int64_t search_start_pos = m_in->getFilePointer();
  vint_c actual_id         = vint_c::read_ebml_id(m_in);
  m_in->setFilePointer(search_start_pos, seek_beginning);

  if (m_debug_read_next)
    mxinfo(boost::format("kax_file::read_next_level1_element(): SEARCH AT %1% ACT ID %|2$x|\n") % search_start_pos % actual_id.m_value);

  // If no valid ID was read then re-sync right away. No other tests
  // can be run.
  if (!actual_id.is_valid())
    return resync_to_level1_element(wanted_id);


  // Easiest case: next level 1 element following the previous one
  // without any element inbetween.
  if (   (wanted_id == actual_id.m_value)
         || (   (0 == wanted_id)
                && (   is_level1_element_id(actual_id)
                       || is_global_element_id(actual_id)))) {
    EbmlElement *l1 = read_one_element();
    if (NULL != l1)
      return l1;
  }

  // If a specific level 1 is wanted then look for next ID by skipping
  // other level 1 or special elements. If that files fallback to a
  // byte-for-byte search for the ID.
  if ((0 != wanted_id) && (is_level1_element_id(actual_id) || is_global_element_id(actual_id))) {
    m_in->setFilePointer(search_start_pos, seek_beginning);
    EbmlElement *l1 = read_one_element();

    if (NULL != l1) {
      bool ok = m_in->setFilePointer2(l1->GetElementPosition() + l1->ElementSize(), seek_beginning);

      if (m_debug_read_next)
        mxinfo(boost::format("kax_file::read_next_level1_element(): other level 1 element %1% new pos %2% fsize %3% epos %4% esize %5%\n")
               % EBML_NAME(l1)  % (l1->GetElementPosition() + l1->ElementSize()) % m_file_size
               % l1->GetElementPosition() % l1->ElementSize());

      delete l1;

      return ok ? read_next_level1_element(wanted_id) : NULL;
    }
  }

  // Last case: no valid ID found. Try a byte-for-byte search for the
  // next wanted/level 1 ID. Also try to locate at least three valid
  // ID/sizes, not just one ID.
  m_in->setFilePointer(search_start_pos, seek_beginning);
  return resync_to_level1_element(wanted_id);
}

EbmlElement *
kax_file_c::read_one_element() {
  int upper_lvl_el = 0;
  EbmlElement *l1  = m_es->FindNextElement(EBML_INFO_CONTEXT(EBML_INFO(KaxSegment)), upper_lvl_el, 0xFFFFFFFFL, true);

  if (NULL == l1)
    return NULL;

  const EbmlCallbacks *callbacks = find_ebml_callbacks(EBML_INFO(KaxSegment), EbmlId(*l1));
  if (NULL == callbacks)
    callbacks = &EBML_INFO(KaxSegment);

  EbmlElement *l2 = NULL;
  l1->Read(*m_es.get_object(), EBML_INFO_CONTEXT(*callbacks), upper_lvl_el, l2, true);
  delete l2;

  return l1;
}

bool
kax_file_c::is_level1_element_id(vint_c id) const {
  const EbmlSemanticContext &context = EBML_INFO_CONTEXT(EBML_INFO(KaxSegment));
  for (int segment_idx = 0; EBML_CTX_SIZE(context) > segment_idx; ++segment_idx)
    if (EBML_ID_VALUE(EBML_SEM_ID(context.MyTable[segment_idx])) == id.m_value)
      return true;

  return false;
}

bool
kax_file_c::is_global_element_id(vint_c id) const {
  return (EBML_ID_VALUE(EBML_ID(EbmlVoid))  == id.m_value)
    ||   (EBML_ID_VALUE(EBML_ID(EbmlCrc32)) == id.m_value);
}

EbmlElement *
kax_file_c::resync_to_level1_element(uint32_t wanted_id) {
  try {
    return resync_to_level1_element_internal(wanted_id);
  } catch (...) {
    if (m_debug_resync)
      mxinfo("kax_file::resync_to_level1_element(): exception\n");
    return NULL;
  }
}

EbmlElement *
kax_file_c::resync_to_level1_element_internal(uint32_t wanted_id) {
  m_resynced         = true;
  m_resync_start_pos = m_in->getFilePointer();

  uint32_t actual_id = m_in->read_uint32_be();

  while (m_in->getFilePointer() < m_file_size) {
    actual_id = (actual_id << 8) | m_in->read_uint8();

    if (   ((0 != wanted_id) && (wanted_id != actual_id))
        || ((0 == wanted_id) && !is_level1_element_id(vint_c(actual_id, 4))))
      continue;

    int64_t current_start_pos = m_in->getFilePointer() - 4;
    int64_t element_pos       = current_start_pos;
    unsigned int num_headers  = 1;

    if (m_debug_resync)
      mxinfo(boost::format("kax_file::resync_to_level1_element(): byte-for-byte search, found level 1 ID %|2$x| at %1%\n") % current_start_pos % actual_id);

    try {
      unsigned int idx;
      for (idx = 0; 3 > idx; ++idx) {
        vint_c length = vint_c::read(m_in);

        if (m_debug_resync)
          mxinfo(boost::format("kax_file::resync_to_level1_element():   read ebml length %1%/%2% valid? %3%\n") % length.m_value % length.m_coded_size % length.is_valid());

        if (   !length.is_valid()
            || ((element_pos + length.m_value + length.m_coded_size + 2 * 4) >= m_file_size)
            || !m_in->setFilePointer2(element_pos + 4 + length.m_value + length.m_coded_size, seek_beginning))
          break;

        element_pos      = m_in->getFilePointer();
        uint32_t next_id = m_in->read_uint32_be();

        if (m_debug_resync)
          mxinfo(boost::format("kax_file::resync_to_level1_element():   next ID is %|1$x| at %2%\n") % next_id % element_pos);

        if (   ((0 != wanted_id) && (wanted_id != next_id))
            || ((0 == wanted_id) && !is_level1_element_id(vint_c(next_id, 4))))
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
  return static_cast<KaxCluster *>(resync_to_level1_element(EBML_ID_VALUE(EBML_ID(KaxCluster))));
}

KaxCluster *
kax_file_c::read_next_cluster() {
  return static_cast<KaxCluster *>(read_next_level1_element(EBML_ID_VALUE(EBML_ID(KaxCluster))));
}

bool
kax_file_c::was_resynced() const {
  return m_resynced;
}

int64_t
kax_file_c::get_resync_start_pos() const {
  return m_resync_start_pos;
}
