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
                                         int cache_size,
                                         bool p_delete_out)
  : mm_proxy_io_c(p_out, p_delete_out)
  , m_cache(memory_c::alloc(cache_size))
  , m_cache_pos(0)
{
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
mm_write_cache_io_c::_read(void *buffer,
                           size_t size) {
  throw mm_io_error_c("read-for-write-only-file");
  return 0;
}

size_t
mm_write_cache_io_c::_write(const void *buffer,
                            size_t size) {
  if ((size + m_cache_pos) > m_cache->get_size())
    flush_cache();

  memcpy(m_cache->get_buffer() + m_cache_pos, buffer, size);
  m_cache_pos += size;

  return size;
}

void
mm_write_cache_io_c::flush_cache() {
  if (0 == m_cache_pos)
    return;

  size_t bytes_written = mm_proxy_io_c::_write(m_cache->get_buffer(), m_cache_pos);
  mxverb(2, boost::format("mm_write_cache_io_c::flush_cache(): requested %1% written %2%\n") % m_cache_pos % bytes_written);
  if (bytes_written != m_cache_pos)
    throw mm_io_error_c((boost::format("write-error: requested %1%, written %2%") % m_cache_pos % bytes_written).str());

  m_cache_pos = 0;
}
