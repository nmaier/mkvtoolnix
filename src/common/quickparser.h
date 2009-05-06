/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   quick Matroska file parsing

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_QUICKPARSER_H
#define __MTX_COMMON_QUICKPARSER_H

#include "os.h"

#include <vector>

#include "common/common.h"
#include "common/mm_io.h"

using namespace libebml;

typedef struct segment_child_t {
  int64_t pos;
  int64_t size;
  EbmlId id;

  segment_child_t(): id((uint32_t)0, 0) {};
} segment_child_t;

class kax_quickparser_c {
private:
  std::vector<segment_child_t> children;
  std::vector<segment_child_t>::iterator current_child;
  mm_io_c &in;

public:
  kax_quickparser_c(mm_io_c &_in, bool parse_fully = false);
  virtual ~kax_quickparser_c() {};

  virtual int num_elements(const EbmlId &id) const;
  virtual segment_child_t *get_next(const EbmlId &id);
  virtual void reset();
  virtual EbmlMaster *read_all(const EbmlCallbacks &callbacks);
};

#endif
