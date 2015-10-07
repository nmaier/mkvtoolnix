/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   high level Matroska file reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_KAX_FILE_H
#define MTX_COMMON_KAX_FILE_H

#include "common/common_pch.h"

#include <matroska/KaxSegment.h>
#include <matroska/KaxCluster.h>

#include "common/vint.h"

using namespace libebml;
using namespace libmatroska;

class kax_file_c {
protected:
  mm_io_cptr m_in;
  bool m_resynced;
  uint64_t m_resync_start_pos, m_file_size;
  int64_t m_timecode_scale, m_last_timecode;
  std::shared_ptr<EbmlStream> m_es;

  debugging_option_c m_debug_read_next, m_debug_resync;

public:
  kax_file_c(mm_io_cptr &in);
  virtual ~kax_file_c();

  virtual bool was_resynced() const;
  virtual int64_t get_resync_start_pos() const;
  virtual bool is_level1_element_id(vint_c id) const;
  virtual bool is_global_element_id(vint_c id) const;

  virtual EbmlElement *read_next_level1_element(uint32_t wanted_id = 0, bool report_cluster_timecode = false);
  virtual KaxCluster *read_next_cluster();

  virtual EbmlElement *resync_to_level1_element(uint32_t wanted_id = 0);
  virtual KaxCluster *resync_to_cluster();

  static unsigned long get_element_size(EbmlElement *e);

  virtual void set_timecode_scale(int64_t timecode_scale);
  virtual void set_last_timecode(int64_t last_timecode);

protected:
  virtual EbmlElement *read_one_element();

  virtual EbmlElement *read_next_level1_element_internal(uint32_t wanted_id = 0);
  virtual EbmlElement *resync_to_level1_element_internal(uint32_t wanted_id = 0);
};
typedef std::shared_ptr<kax_file_c> kax_file_cptr;

#endif  // MTX_COMMON_KAX_FILE_H
