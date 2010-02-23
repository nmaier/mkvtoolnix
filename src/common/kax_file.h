/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   high level Matroska file reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_KAX_FILE_H
#define __MTX_COMMON_KAX_FILE_H

#include "os.h"

#include <matroska/KaxSegment.h>
#include <matroska/KaxCluster.h>

#include "common/mm_io.h"
#include "common/vint.h"

using namespace libebml;
using namespace libmatroska;

class kax_file_c {
protected:
  mm_io_cptr m_in;
  bool m_resynced;
  int64_t m_resync_start_pos, m_file_size;
  counted_ptr<EbmlStream> m_es;

public:
  kax_file_c(mm_io_cptr &in);

  virtual bool was_resynced() const;
  virtual int64_t get_resync_start_pos() const;
  virtual bool is_level1_element_id(vint_c id) const;
  virtual bool is_global_element_id(vint_c id) const;

  virtual EbmlElement *read_next_level1_element(uint32_t wanted_id = 0);
  virtual KaxCluster *read_next_cluster();

  virtual EbmlElement *resync_to_level1_element(uint32_t wanted_id = 0);
  virtual KaxCluster *resync_to_cluster();

protected:
  virtual EbmlElement *read_one_element();

  virtual EbmlElement *read_next_level1_element_internal(uint32_t wanted_id = 0);
  virtual EbmlElement *resync_to_level1_element_internal(uint32_t wanted_id = 0);
};
typedef counted_ptr<kax_file_c> kax_file_cptr;

#endif  // __MTX_COMMON_KAX_FILE_H
