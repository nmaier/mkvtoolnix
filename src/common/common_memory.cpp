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

#include "os.h"

#include <vector>

#include "common.h"
#include "common_memory.h"
#include "error.h"

using namespace std;

void
memory_c::resize(int new_size) throw() {
  if (!its_counter)
    its_counter = new counter(NULL, 0, false);

  if (its_counter->is_free)
    its_counter->ptr = (X *)saferealloc(its_counter->ptr, new_size);
  else {
    X *tmp = (X *)safemalloc(new_size);
    memcpy(tmp, its_counter->ptr, its_counter->size);
    its_counter->ptr = tmp;
    its_counter->is_free = true;
  }
  its_counter->size = new_size;
}

memory_cptr
lace_memory_xiph(const vector<memory_cptr> &blocks) {
  unsigned char *buffer;
  int size, i, offset, n;

  size = 1;
  for (i = 0; (blocks.size() - 1) > i; ++i)
    size += blocks[i]->get_size() / 255 + 1 + blocks[i]->get_size();
  size += blocks.back()->get_size();

  buffer = (unsigned char *)safemalloc(size);

  buffer[0] = blocks.size() - 1;
  offset = 1;
  for (i = 0; (blocks.size() - 1) > i; ++i) {
    for (n = blocks[i]->get_size(); n >= 255; n -= 255) {
      buffer[offset] = 255;
      ++offset;
    }
    buffer[offset] = n;
    ++offset;
  }
  for (i = 0; blocks.size() > i; ++i) {
    memcpy(&buffer[offset], blocks[i]->get(), blocks[i]->get_size());
    offset += blocks[i]->get_size();
  }

  return memory_cptr(new memory_c(buffer, size, true));
}

vector<memory_cptr>
unlace_memory_xiph(memory_cptr &buffer) {
  int i, num_blocks, size, last_size;
  unsigned char *ptr, *end;
  vector<memory_cptr> blocks;
  vector<int> sizes;

  if (1 > buffer->get_size())
    throw error_c("Lacing error: Buffer too small");

  ptr = buffer->get();
  end = buffer->get() + buffer->get_size();
  num_blocks = ptr[0] + 1;
  ++ptr;

  last_size = buffer->get_size();
  for (i = 0; (num_blocks - 1) > i; ++i) {
    size = 0;
    while ((ptr < end) && (*ptr == 255)) {
      size += 255;
      ++ptr;
    }

    if (ptr >= end)
      throw error_c("Lacing error: End-of-buffer while reading the block "
                    "sizes");

    size += *ptr;
    ++ptr;

    sizes.push_back(size);
    last_size -= size;
  }

  sizes.push_back(last_size - (ptr - buffer->get()));

  for (i = 0; sizes.size() > i; ++i) {
    if ((ptr + sizes[i]) > end)
      throw error_c("Lacing error: End-of-buffer while assigning the blocks");

    blocks.push_back(memory_cptr(new memory_c(ptr, sizes[i], false)));
    ptr += sizes[i];
  }

  return blocks;
}
