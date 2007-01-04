#ifndef COUNTED_PTR_H
#define COUNTED_PTR_H

template <class X> class counted_ptr {
public:
  typedef X element_type;

  explicit counted_ptr(X *p = 0): // allocate a new counter
    its_counter(0) {
    if (p)
      its_counter = new counter(p);
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
  X *get() const throw() {
    return its_counter ? its_counter->ptr : 0;
  }
  bool unique() const throw() {
    return (its_counter ? its_counter->count == 1 : true);
  }
  void clear() throw() {
    release();
    its_counter = NULL;
  }

private:
  struct counter {
    counter(X *p = 0, unsigned c = 1):
      ptr(p), count(c) {}
    X *ptr;
    unsigned count;
  }* its_counter;

  void acquire(counter* c) throw() { // increment the count
    its_counter = c;
    if (c)
      ++c->count;
  }

  void release() { // decrement the count, delete if it is 0
    if (its_counter) {
      if (--its_counter->count == 0) {
        delete its_counter->ptr;
        delete its_counter;
      }
      its_counter = 0;
    }
  }
};

#endif // COUNTED_PTR_H
