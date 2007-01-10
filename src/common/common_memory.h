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

#include <cassert>

#include <deque>
#include <vector>

#include "common.h"
#include "smart_pointers.h"

class MTX_DLL_API memory_c;
typedef counted_ptr<memory_c> memory_cptr;
typedef std::vector<memory_cptr> memories_c;

class MTX_DLL_API memory_c {
public:
  typedef unsigned char X;

  explicit memory_c(void *p = NULL, int s = 0, bool f = false): // allocate a new counter
    its_counter(NULL) {
    if (p)
      its_counter = new counter((unsigned char *)p, s, f);
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
    return its_counter ? its_counter->ptr + its_counter->offset : NULL;
  }

  int get_size() const throw() {
    return its_counter ? its_counter->size - its_counter->offset: 0;
  }

  void set_size(int new_size) throw() {
    if (its_counter)
      its_counter->size = new_size;
  }

  void set_offset(unsigned new_offset) throw() {
    if (!its_counter || (new_offset > its_counter->size))
      throw false;
    its_counter->offset = new_offset;
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
    its_counter->size -= its_counter->offset;
    its_counter->offset = 0;
  }

  void lock() {
    if (its_counter)
      its_counter->is_free = false;
  }

  void resize(int new_size) throw();

private:
  struct counter {
    counter(X *p = NULL, int s = 0, bool f = false, unsigned c = 1):
      ptr(p), size(s), is_free(f), count(c), offset(0) {}

    X *ptr;
    int size;
    bool is_free;
    unsigned count, offset;
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

class MTX_DLL_API memory_slice_cursor_c {
protected:
  int m_pos, m_pos_in_slice, m_size;
  deque<memory_cptr> m_slices;
  deque<memory_cptr>::iterator m_slice;

public:
  memory_slice_cursor_c():
    m_pos(0),
    m_pos_in_slice(0),
    m_size(0),
    m_slice(m_slices.end()) {
  };

  memory_slice_cursor_c(const memory_slice_cursor_c &) {
    die("memory_slice_cursor_c copy c'tor: Must not be used!");
  };

  ~memory_slice_cursor_c() {
  };

  void add_slice(memory_cptr slice) {
    m_slices.push_back(slice);
    m_size += slice->get_size();
    if (m_slice == m_slices.end())
      m_slice = m_slices.begin();
  };

  void add_slice(unsigned char *buffer, int size) {
    add_slice(memory_cptr(new memory_c(buffer, size, false)));
  };

  unsigned char get_char() {
    assert(m_pos < m_size);

    unsigned char c = *((*m_slice)->get() + m_pos_in_slice);

    ++m_pos_in_slice;
    ++m_pos;

    while ((m_slices.end() != m_slice) &&
           (m_pos_in_slice >= (*m_slice)->get_size())) {
      m_slice++;
      m_pos_in_slice = 0;
    }
    return c;
  };

  bool char_available() {
    return m_pos < m_size;
  };

  int get_remaining_size() {
    return m_size - m_pos;
  };

  int get_size() {
    return m_size;
  };

  int get_position() {
    return m_pos;
  };

  void reset(bool clear_slices = false) {
    if (clear_slices) {
      m_slices.clear();
      m_size = 0;
    }
    m_pos = 0;
    m_pos_in_slice = 0;
    m_slice = m_slices.begin();
  };

  void copy(unsigned char *dest, int start, int size) {
    assert((start + size) <= m_size);

    deque<memory_cptr>::iterator curr = m_slices.begin();
    int offset = 0;

    while (start > ((*curr)->get_size() + offset)) {
      offset += (*curr)->get_size();
      curr++;
      assert(m_slices.end() != curr);
    }
    offset = start - offset;

    while (0 < size) {
      int num_bytes = (*curr)->get_size() - offset;
      if (num_bytes > size)
        num_bytes = size;

      memcpy(dest, (*curr)->get() + offset, num_bytes);

      size -= num_bytes;
      dest += num_bytes;
      curr++;
      offset = 0;
    }
  };

};

inline memory_cptr
clone_memory(void *buffer,
             int size) {
  return memory_cptr(new memory_c((unsigned char *)safememdup(buffer, size),
                                  size));
}

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
