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

class MTX_DLL_API memory_c;
typedef counted_ptr<memory_c> memory_cptr;
typedef std::vector<memory_cptr> memories_c;

class MTX_DLL_API memory_c {
public:
  typedef unsigned char X;

  explicit memory_c(X *p = NULL, int s = 0, bool f = false): // allocate a new counter
    its_counter(NULL) {
    if (p)
      its_counter = new counter(p, s, f);
  }

  ~memory_c() {
    release();
  }

  memory_c(const memory_c &r) throw() {
    acquire(r.its_counter);
  }

  memory_c &operator=(const memory_c &r) {
    if (this != &r) {
      release();
      acquire(r.its_counter);
    }
    return *this;
  }

  X *get() const throw() {
    return its_counter ? its_counter->ptr : NULL;
  }

  int get_size() const throw() {
    return its_counter ? its_counter->size : 0;
  }

  void set_size(int new_size) throw() {
    if (its_counter)
      its_counter->size = new_size;
  }

  bool unique() const throw() {
    return (its_counter ? its_counter->count == 1 : true);
  }

  memory_c *clone() const {
    return new memory_c((unsigned char *)safememdup(get(), get_size()),
                        get_size(), true);
  }

  void grab() {
    if (!its_counter || its_counter->is_free)
      return;

    its_counter->ptr = (unsigned char *)safememdup(get(), get_size());
    its_counter->is_free = true;
  }

  void lock() {
    if (its_counter)
      its_counter->is_free = false;
  }

  void resize(int new_size) throw();

private:
  struct counter {
    counter(X *p = NULL, int s = 0, bool f = false, unsigned c = 1):
      ptr(p), size(s), is_free(f), count(c) {}

    X *ptr;
    int size;
    bool is_free;
    unsigned count;
  } *its_counter;

  void acquire(counter *c) throw() { // increment the count
    its_counter = c;
    if (c)
      ++c->count;
  }

  void release() { // decrement the count, delete if it is 0
    if (its_counter) {
      if (--its_counter->count == 0) {
        if (its_counter->is_free)
          free(its_counter->ptr);
        delete its_counter;
      }
      its_counter = 0;
    }
  }
};

struct buffer_t {
  unsigned char *m_buffer;
  int m_size;

  buffer_t();
  buffer_t(unsigned char *buffer, int m_size);
  ~buffer_t();
};

memory_cptr MTX_DLL_API lace_memory_xiph(const vector<memory_cptr> &blocks);
vector<memory_cptr> MTX_DLL_API unlace_memory_xiph(memory_cptr &buffer);

#endif // __COMMON_MEMORY_H
