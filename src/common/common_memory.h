/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for memory handling classes

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMON_MEMORY_H
#define __COMMON_MEMORY_H

#include "os.h"

#include <vector>

#include "common.h"
#include "smart_pointers.h"

class MTX_DLL_API memory_c {
public:
  unsigned char *data;
  uint32_t size;
  bool is_free;

public:
  memory_c(unsigned char *ndata, uint32_t nsize, bool nis_free):
    data(ndata), size(nsize), is_free(nis_free) {
    if (data == NULL)
      die("memory_c::memory_c: data = %p, size = %u\n", data, size);
  }

  memory_c(const memory_c &src) {
    die("memory_c::memory_c(const memory_c &) called\n");
  }

  ~memory_c() {
    release();
  }

  int lock() {
    is_free = false;
    return 0;
  }

  unsigned char *grab() {
    if (is_free) {
      is_free = false;
      return data;
    }
    return (unsigned char *)safememdup(data, size);
  }

  int release() {
    if (is_free) {
      safefree(data);
      data = NULL;
      is_free = false;
    }
    return 0;
  }

  memory_c *clone() const {
    return new memory_c((unsigned char *)safememdup(data, size), size, true);
  }
};

typedef counted_ptr<memory_c> memory_cptr;
typedef std::vector<memory_cptr> memories_c;

#endif // __COMMON_MEMORY_H
