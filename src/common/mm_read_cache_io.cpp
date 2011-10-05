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

#include "common/mm_read_cache_io.h"

mm_probe_cache_io_c::mm_probe_cache_io_c(mm_io_c *p_in,
                                       size_t cache_size,
                                       bool p_delete_in)
  : mm_proxy_io_c(p_in, p_delete_in)
  , m_af_cache(memory_c::alloc(cache_size))
  , m_cache_pos(0)
  , m_cache_fill_pos(0)
  , m_cache_size(cache_size)
  , m_cache(m_af_cache->get_buffer())
{
  m_cache_size = std::min(static_cast<int64_t>(m_cache_size), p_in->get_size());
  setFilePointer(0, seek_beginning);
}

mm_probe_cache_io_c::~mm_probe_cache_io_c() {
  close();
}

mm_probe_cache_io_c *
mm_probe_cache_io_c::open(const std::string &file_name,
                         size_t cache_size) {
  return new mm_probe_cache_io_c(new mm_file_io_c(file_name, MODE_READ), cache_size);
}

uint64
mm_probe_cache_io_c::getFilePointer() {
  return m_cache_pos;
}

void
mm_probe_cache_io_c::setFilePointer(int64 offset,
                                   seek_mode mode) {
  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? m_proxy_io->get_size() - offset
    :                          m_cache_pos            + offset;

  if ((0 > new_pos) || ((0 != new_pos) && (new_pos >= static_cast<int64_t>(m_cache_size))))
    throw mm_io_seek_error_c();

  m_cache_pos = new_pos;
}

int64_t
mm_probe_cache_io_c::get_size() {
  return m_proxy_io->get_size();
}

uint32
mm_probe_cache_io_c::_read(void *buffer,
                          size_t size) {
  size = std::min(m_cache_pos + size, m_cache_size) - m_cache_pos;

  if ((m_cache_pos + size) >= m_cache_fill_pos) {
    m_proxy_io->setFilePointer(m_cache_fill_pos);

    size_t bytes_to_read  = m_cache_pos + size - m_cache_fill_pos;
    m_cache_fill_pos     += m_proxy_io->read(&m_cache[m_cache_fill_pos], bytes_to_read);
  }

  size = std::min(m_cache_pos + size, m_cache_fill_pos) - m_cache_pos;

  memcpy(buffer, &m_cache[m_cache_pos], size);
  m_cache_pos += size;

  return size;
}

size_t
mm_probe_cache_io_c::_write(const void *buffer,
                            size_t size) {
  throw mm_io_error_c("write-for-read-only-file");
  return 0;
}
