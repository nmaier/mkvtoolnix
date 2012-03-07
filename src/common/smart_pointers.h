/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   smart pointers

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_SMART_POINTERS_H
#define __MTX_COMMON_SMART_POINTERS_H

#include "common/os.h"

#include <memory>

template <class X> class counted_ptr {
public:
  typedef X element_type;

  explicit counted_ptr(X *p = 0, bool use_free = false): // allocate a new counter
    its_counter(0) {
    if (p)
      its_counter = new counter(p, 1, use_free);
  }

  ~counted_ptr() {
    release();
  }

  counted_ptr(const counted_ptr& r) throw() {
    acquire(r.its_counter);
  }
  counted_ptr& operator=(const counted_ptr& r) {
    if (this != &r) {
      release();
      acquire(r.its_counter);
    }
    return *this;
  }

//   template <class Y> friend class counted_ptr<Y>;
  template <class Y> counted_ptr(const counted_ptr<Y>& r) throw() {
    acquire(r.its_counter);
  }
  template <class Y> counted_ptr& operator=(const counted_ptr<Y>& r) {
    if (this != &r) {
      release();
      acquire(r.its_counter);
    }
    return *this;
  }

  X &operator*() const throw() {
    return *its_counter->ptr;
  }
  X *operator->() const throw() {
    return its_counter->ptr;
  }
  X *get_object() const throw() {
    return its_counter ? its_counter->ptr : 0;
  }
  bool is_unique() const throw() {
    return (its_counter ? its_counter->count == 1 : true);
  }
  void clear() throw() {
    release();
    its_counter = nullptr;
  }
  bool is_set() const throw() {
    return (nullptr != its_counter) && (nullptr != its_counter->ptr);
  }
  operator bool() const throw() {
    return is_set();
  }

private:
  struct counter {
    counter(X *p = 0, unsigned c = 1, bool f = false):
      ptr(p), count(c), use_free(f) {}
    X *ptr;
    unsigned count;
    bool use_free;
  }* its_counter;

  void acquire(counter* c) throw() { // increment the count
    its_counter = c;
    if (c)
      ++c->count;
  }

  void release() { // decrement the count, delete if it is 0
    if (its_counter) {
      if (--its_counter->count == 0) {
        if (its_counter->use_free)
          free(its_counter->ptr);
        else
          delete its_counter->ptr;
        delete its_counter;
      }
      its_counter = 0;
    }
  }
};

#endif // COUNTED_PTR_H
