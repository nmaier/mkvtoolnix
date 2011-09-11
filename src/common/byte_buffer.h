/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Byte buffer class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_BYTE_BUFFER_H
#define __MTX_COMMON_BYTE_BUFFER_H

#include "common/common_pch.h"

#include "common/memory.h"

class byte_buffer_c {
private:
  unsigned char *data;
  int size, pos, max_size;

public:
  byte_buffer_c(int nmax_size = 128 * 1024):
    data(NULL),
    size(0),
    pos(0),
    max_size(nmax_size) {
  };
  virtual ~byte_buffer_c() {
    safefree(data);
  };

  void trim() {
    int new_size;
    unsigned char *temp_buf;

    if (pos == 0)
      return;
    new_size = size - pos;
    if (new_size == 0) {
      safefree(data);
      data = NULL;
      size = 0;
      pos = 0;
      return;
    }
    temp_buf = (unsigned char *)safemalloc(new_size);
    memcpy(temp_buf, &data[pos], new_size);
    safefree(data);
    data = temp_buf;
    size = new_size;
    pos = 0;
  }

  void add(const unsigned char *new_data, int new_size) {
    if ((pos != 0) && ((size + new_size) >= max_size))
      trim();
    data = (unsigned char *)saferealloc(data, size + new_size);
    memcpy(&data[size], new_data, new_size);
    size += new_size;
  };

  void add(memory_cptr &new_buffer) {
    add(new_buffer->get_buffer(), new_buffer->get_size());
  }

  void remove(int num) {
    if ((pos + num) > size)
      mxerror(Y("byte_buffer_c: (pos + num) > size. Should not have happened. Please file a bug report.\n"));
    pos += num;
    if (pos == size) {
      safefree(data);
      data = NULL;
      size = 0;
      pos = 0;
    }
  };

  unsigned char *get_buffer() {
    return &data[pos];
  };

  int get_size() {
    return size - pos;
  };
};

typedef counted_ptr<byte_buffer_c> byte_buffer_cptr;

#endif // __MTX_COMMON_BYTE_BUFFER_H
