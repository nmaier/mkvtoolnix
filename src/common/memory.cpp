/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for memory handling classes

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/memory.h"
#include "common/error.h"

void
memory_c::resize(size_t new_size) throw() {
  if (!its_counter)
    its_counter = new counter(NULL, 0, false);

  if (its_counter->is_free) {
    its_counter->ptr  = (X *)saferealloc(its_counter->ptr, new_size + its_counter->offset);
    its_counter->size = new_size + its_counter->offset;

  } else {
    X *tmp = (X *)safemalloc(new_size);
    memcpy(tmp, its_counter->ptr + its_counter->offset, its_counter->size - its_counter->offset);
    its_counter->ptr     = tmp;
    its_counter->is_free = true;
    its_counter->size    = new_size;
  }
}

void
memory_c::add(unsigned char *new_buffer,
              size_t new_size) {
  if ((0 == new_size) || (NULL == new_buffer))
    return;

  size_t previous_size = get_size();
  resize(previous_size + new_size);
  memcpy(get_buffer() + previous_size, new_buffer, new_size);
}

memory_cptr
lace_memory_xiph(const std::vector<memory_cptr> &blocks) {
  size_t i, size = 1;
  for (i = 0; (blocks.size() - 1) > i; ++i)
    size += blocks[i]->get_size() / 255 + 1 + blocks[i]->get_size();
  size += blocks.back()->get_size();

  memory_cptr mem       = memory_c::alloc(size);
  unsigned char *buffer = mem->get_buffer();

  buffer[0]             = blocks.size() - 1;
  size_t offset         = 1;
  for (i = 0; (blocks.size() - 1) > i; ++i) {
    int n;
    for (n = blocks[i]->get_size(); n >= 255; n -= 255) {
      buffer[offset] = 255;
      ++offset;
    }
    buffer[offset] = n;
    ++offset;
  }

  for (i = 0; blocks.size() > i; ++i) {
    memcpy(&buffer[offset], blocks[i]->get_buffer(), blocks[i]->get_size());
    offset += blocks[i]->get_size();
  }

  return mem;
}

std::vector<memory_cptr>
unlace_memory_xiph(memory_cptr &buffer) {
  if (1 > buffer->get_size())
    throw error_c("Lacing error: Buffer too small");

  std::vector<int> sizes;
  unsigned char *ptr = buffer->get_buffer();
  unsigned char *end = buffer->get_buffer() + buffer->get_size();
  size_t last_size   = buffer->get_size();
  size_t num_blocks  = ptr[0] + 1;
  size_t i;
  ++ptr;

  for (i = 0; (num_blocks - 1) > i; ++i) {
    int size = 0;
    while ((ptr < end) && (*ptr == 255)) {
      size += 255;
      ++ptr;
    }

    if (ptr >= end)
      throw error_c("Lacing error: End-of-buffer while reading the block sizes");

    size += *ptr;
    ++ptr;

    sizes.push_back(size);
    last_size -= size;
  }

  sizes.push_back(last_size - (ptr - buffer->get_buffer()));

  std::vector<memory_cptr> blocks;

  for (i = 0; sizes.size() > i; ++i) {
    if ((ptr + sizes[i]) > end)
      throw error_c("Lacing error: End-of-buffer while assigning the blocks");

    blocks.push_back(memory_cptr(new memory_c(ptr, sizes[i], false)));
    ptr += sizes[i];
  }

  return blocks;
}

unsigned char *
_safememdup(const void *s,
            size_t size,
            const char *file,
            int line) {
  if (NULL == s)
    return NULL;

  unsigned char *copy = reinterpret_cast<unsigned char *>(malloc(size));
  if (NULL == copy)
    mxerror(boost::format(Y("memory.cpp/safememdup() called from file %1%, line %2%: malloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);
  memcpy(copy, s, size);

  return copy;
}

unsigned char *
_safemalloc(size_t size,
            const char *file,
            int line) {
  unsigned char *mem = reinterpret_cast<unsigned char *>(malloc(size));
  if (NULL == mem)
    mxerror(boost::format(Y("memory.cpp/safemalloc() called from file %1%, line %2%: malloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);

  return mem;
}

unsigned char *
_saferealloc(void *mem,
             size_t size,
             const char *file,
             int line) {
  if (0 == size)
    // Do this so realloc() may not return NULL on success.
    size = 1;

  mem = realloc(mem, size);
  if (NULL == mem)
    mxerror(boost::format(Y("memory.cpp/saferealloc() called from file %1%, line %2%: realloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);

  return reinterpret_cast<unsigned char *>(mem);
}
