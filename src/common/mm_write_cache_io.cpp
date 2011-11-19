/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/mm_write_cache_io.h"

mm_write_cache_io_c::mm_write_cache_io_c(mm_io_c *p_out,
                                         size_t cache_size,
                                         bool p_delete_out)
  : mm_proxy_io_c(p_out, p_delete_out)
  , m_af_cache(memory_c::alloc(cache_size))
  , m_cache_pos(0)
  , m_cache_size(cache_size)
  , m_cache(m_af_cache->get_buffer())
{
}

mm_write_cache_io_c::~mm_write_cache_io_c() {
  close();
}

mm_io_cptr
mm_write_cache_io_c::open(const std::string &file_name,
                          size_t cache_size) {
  return mm_io_cptr(new mm_write_cache_io_c(new mm_file_io_c(file_name, MODE_CREATE), cache_size));
}

uint64
mm_write_cache_io_c::getFilePointer() {
  return mm_proxy_io_c::getFilePointer() + m_cache_pos;
}

void
mm_write_cache_io_c::setFilePointer(int64 offset,
                                    seek_mode mode) {
  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? get_size()       - offset
    :                          getFilePointer() + offset;

  if (new_pos == static_cast<int64_t>(getFilePointer()))
    return;

  flush_cache();
  mm_proxy_io_c::setFilePointer(offset, mode);
}

void
mm_write_cache_io_c::flush() {
  flush_cache();
  mm_proxy_io_c::flush();
}

void
mm_write_cache_io_c::close() {
  flush_cache();
  mm_proxy_io_c::close();
}

uint32
mm_write_cache_io_c::_read(void *,
                           size_t) {
  throw mtx::mm_io::wrong_read_write_access_x();
  return 0;
}

size_t
mm_write_cache_io_c::_write(const void *buffer,
                            size_t size) {
  size_t bytes_written = size;
  size_t buffer_offset = 0;

  while (0 != size) {
    size_t bytes_to_write = std::min(size, m_cache_size - m_cache_pos);
    memcpy(m_cache + m_cache_pos, static_cast<const unsigned char *>(buffer) + buffer_offset, bytes_to_write);
    buffer_offset += bytes_to_write;
    m_cache_pos   += bytes_to_write;
    size          -= bytes_to_write;

    if (m_cache_pos == m_cache_size)
      flush_cache();
  }

  return bytes_written;
}

void
mm_write_cache_io_c::flush_cache() {
  if (0 == m_cache_pos)
    return;

  size_t bytes_written = mm_proxy_io_c::_write(m_cache, m_cache_pos);
  mxverb(2, boost::format("mm_write_cache_io_c::flush_cache(): requested %1% written %2%\n") % m_cache_pos % bytes_written);
  if (bytes_written != m_cache_pos)
    throw mtx::mm_io::insufficient_space_x();

  m_cache_pos = 0;
}
