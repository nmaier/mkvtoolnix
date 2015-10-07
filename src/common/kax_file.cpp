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

#include <typeinfo>

#include <ebml/EbmlCrc32.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <ebml/StdIOCallback.h>

#include "common/ebml.h"
#include "common/fs_sys_helpers.h"
#include "common/kax_file.h"
#include "common/strings/formatting.h"

kax_file_c::kax_file_c(mm_io_cptr &in)
  : m_in(in)
  , m_resynced(false)
  , m_resync_start_pos(0)
  , m_file_size(m_in->get_size())
  , m_timecode_scale{TIMECODE_SCALE}
  , m_last_timecode{-1}
  , m_es(new EbmlStream(*m_in))
  , m_debug_read_next{"kax_file|kax_file_read_next"}
  , m_debug_resync{   "kax_file|kax_file_resync"}
{
}

EbmlElement *
kax_file_c::read_next_level1_element(uint32_t wanted_id,
                                     bool report_cluster_timecode) {
  try {
    auto element = read_next_level1_element_internal(wanted_id);

    if (report_cluster_timecode && (-1 != m_timecode_scale))
      mxinfo(boost::format(Y("The first cluster timecode after the resync is %1%.\n"))
             % format_timecode(FindChildValue<KaxClusterTimecode>(static_cast<KaxCluster *>(element)) * m_timecode_scale));

    return element;

  } catch (mtx::exception &e) {
    mxinfo(boost::format("\nmtx ex: %1% (type: %2%)\n") % e.error() % typeid(e).name());
  } catch (std::exception &e) {
    mxinfo(boost::format("\n\nstd ex: %1% (type: %2%)\n") % e.what() % typeid(e).name());
  } catch (...) {
    mxinfo("READ X\n");
  }
  return nullptr;
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
    mxinfo(boost::format("kax_file::read_next_level1_element(): search at %1% for %|4$x| act id %|2$x| is_valid %3%\n") % search_start_pos % actual_id.m_value % actual_id.is_valid() % wanted_id);

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
    if (l1)
      return l1;
  }

  // If a specific level 1 is wanted then look for next ID by skipping
  // other level 1 or special elements. If that files fallback to a
  // byte-for-byte search for the ID.
  if ((0 != wanted_id) && (is_level1_element_id(actual_id) || is_global_element_id(actual_id))) {
    m_in->setFilePointer(search_start_pos, seek_beginning);
    EbmlElement *l1 = read_one_element();

    if (l1) {
      int64_t element_size = get_element_size(l1);
      bool ok              = (0 != element_size) && m_in->setFilePointer2(l1->GetElementPosition() + element_size, seek_beginning);

      if (m_debug_read_next)
        mxinfo(boost::format("kax_file::read_next_level1_element(): other level 1 element %1% new pos %2% fsize %3% epos %4% esize %5%\n")
               % EBML_NAME(l1)  % (l1->GetElementPosition() + element_size) % m_file_size
               % l1->GetElementPosition() % element_size);

      delete l1;

      return ok ? read_next_level1_element(wanted_id) : nullptr;
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
  EbmlElement *l1  = m_es->FindNextElement(EBML_CLASS_CONTEXT(KaxSegment), upper_lvl_el, 0xFFFFFFFFL, true);

  if (!l1)
    return nullptr;

  const EbmlCallbacks *callbacks = find_ebml_callbacks(EBML_INFO(KaxSegment), EbmlId(*l1));
  if (!callbacks)
    callbacks = &EBML_CLASS_CALLBACK(KaxSegment);

  EbmlElement *l2 = nullptr;
  try {
    l1->Read(*m_es.get(), EBML_INFO_CONTEXT(*callbacks), upper_lvl_el, l2, true);

  } catch (libebml::CRTError &e) {
    mxdebug_if(m_debug_resync, boost::format("exception reading element data: %1% (%2%)\n") % e.what() % e.getError());
    m_in->setFilePointer(l1->GetElementPosition() + 1);
    delete l1;
    return nullptr;
  }

  unsigned long element_size = get_element_size(l1);
  if (m_debug_resync)
    mxinfo(boost::format("kax_file::read_one_element(): read element at %1% calculated size %2% stored size %3%\n")
           % l1->GetElementPosition() % element_size % (l1->IsFiniteSize() ? (boost::format("%1%") % l1->ElementSize()).str() : std::string("unknown")));
  m_in->setFilePointer(l1->GetElementPosition() + element_size, seek_beginning);

  return l1;
}

bool
kax_file_c::is_level1_element_id(vint_c id) const {
  const EbmlSemanticContext &context = EBML_CLASS_CONTEXT(KaxSegment);
  for (size_t segment_idx = 0; EBML_CTX_SIZE(context) > segment_idx; ++segment_idx)
    if (EBML_ID_VALUE(EBML_CTX_IDX_ID(context,segment_idx)) == id.m_value)
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
    return nullptr;
  }
}

EbmlElement *
kax_file_c::resync_to_level1_element_internal(uint32_t wanted_id) {
  m_resynced         = true;
  m_resync_start_pos = m_in->getFilePointer();

  uint32_t actual_id = m_in->read_uint32_be();
  int64_t start_time = get_current_time_millis();
  bool is_cluster_id = !wanted_id || (EBML_ID_VALUE(EBML_ID(KaxCluster)) == wanted_id); // 0 means: any level 1 element will do

  mxinfo(boost::format(Y("%1%: Error in the Matroska file structure at position %2%. Resyncing to the next level 1 element.\n"))
         % m_in->get_file_name() % m_resync_start_pos);

  if (is_cluster_id && (-1 != m_last_timecode)) {
    mxinfo(boost::format(Y("The last timecode processed before the error was encountered was %1%.\n")) % format_timecode(m_last_timecode));
    m_last_timecode = -1;
  }

  if (m_debug_resync)
    mxinfo(boost::format("kax_file::resync_to_level1_element(): starting at %1% potential ID %|2$08x|\n") % m_resync_start_pos % actual_id);

  while (m_in->getFilePointer() < m_file_size) {
    int64_t now = get_current_time_millis();
    if ((now - start_time) >= 10000) {
      mxinfo(boost::format("Still resyncing at position %1%.\n") % m_in->getFilePointer());
      start_time = now;
    }

    actual_id = (actual_id << 8) | m_in->read_uint8();

    if (   ((0 != wanted_id) && (wanted_id != actual_id))
        || ((0 == wanted_id) && !is_level1_element_id(vint_c(actual_id, 4))))
      continue;

    uint64_t current_start_pos = m_in->getFilePointer() - 4;
    uint64_t element_pos       = current_start_pos;
    unsigned int num_headers   = 1;
    bool valid_unknown_size    = false;

    if (m_debug_resync)
      mxinfo(boost::format("kax_file::resync_to_level1_element(): byte-for-byte search, found level 1 ID %|2$x| at %1%\n") % current_start_pos % actual_id);

    try {
      unsigned int idx;
      for (idx = 0; 3 > idx; ++idx) {
        vint_c length = vint_c::read(m_in);

        if (m_debug_resync)
          mxinfo(boost::format("kax_file::resync_to_level1_element():   read ebml length %1%/%2% valid? %3% unknown? %4%\n")
                 % length.m_value % length.m_coded_size % length.is_valid() % length.is_unknown());

        if (length.is_unknown()) {
          valid_unknown_size = true;
          break;
        }

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

    if ((4 == num_headers) || valid_unknown_size) {
      mxinfo(boost::format(Y("Resyncing successful at position %1%.\n")) % current_start_pos);
      m_in->setFilePointer(current_start_pos, seek_beginning);
      return read_next_level1_element(wanted_id, is_cluster_id);
    }

    m_in->setFilePointer(current_start_pos + 4, seek_beginning);
  }

  mxinfo(Y("Resync failed: no valid Matroska level 1 element found.\n"));

  return nullptr;
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

unsigned long
kax_file_c::get_element_size(EbmlElement *e) {
  EbmlMaster *m = dynamic_cast<EbmlMaster *>(e);

  if (!m || e->IsFiniteSize())
    return e->GetSizeLength() + EBML_ID_LENGTH(static_cast<const EbmlId &>(*e)) + e->GetSize();

  unsigned long max_end_pos = e->GetElementPosition() + EBML_ID_LENGTH(static_cast<const EbmlId &>(*e));
  unsigned int idx;
  for (idx = 0; m->ListSize() > idx; ++idx)
    max_end_pos = std::max(max_end_pos, static_cast<unsigned long>((*m)[idx]->GetElementPosition() + get_element_size((*m)[idx])));

  return max_end_pos - e->GetElementPosition();
}

void
kax_file_c::set_timecode_scale(int64_t timecode_scale) {
  m_timecode_scale = timecode_scale;
}

void
kax_file_c::set_last_timecode(int64_t last_timecode) {
  m_last_timecode = last_timecode;
}
