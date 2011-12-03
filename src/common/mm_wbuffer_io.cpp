/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modifications by Nils Maier <maierman@web.de>
*/

#include "common/common_pch.h"

#include "common/mm_buffered_io.h"

mm_wbuffer_io_c::mm_wbuffer_io_c(mm_io_c *p_out,
                                         size_t p_buffer_size,
                                         bool p_delete_out)
  : mm_proxy_io_c(p_out, p_delete_out)
  , m_af_buffer(memory_c::alloc(p_buffer_size))
  , m_buffer(m_af_buffer->get_buffer())
  , m_fill(0)
  , m_size(p_buffer_size)
{
}

mm_wbuffer_io_c::~mm_wbuffer_io_c() {
  close();
}

mm_io_cptr
mm_wbuffer_io_c::open(const std::string &file_name,
                          size_t p_buffer_size) {
  return mm_io_cptr(new mm_wbuffer_io_c(new mm_file_io_c(file_name, MODE_CREATE), p_buffer_size));
}

uint64
mm_wbuffer_io_c::getFilePointer() {
  return mm_proxy_io_c::getFilePointer() + m_fill;
}

void
mm_wbuffer_io_c::setFilePointer(int64 offset,
                                    seek_mode mode) {
  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? get_size()       - offset
    :                          getFilePointer() + offset;

  if (new_pos == static_cast<int64_t>(getFilePointer()))
    return;

  flush_buffer();
  mm_proxy_io_c::setFilePointer(offset, mode);
}

void
mm_wbuffer_io_c::flush() {
  flush_buffer();
  mm_proxy_io_c::flush();
}

void
mm_wbuffer_io_c::close() {
  flush_buffer();
  mm_proxy_io_c::close();
}

uint32
mm_wbuffer_io_c::_read(void *, size_t) {
  throw mtx::mm_io::wrong_read_write_access_x();
  return 0;
}

size_t
mm_wbuffer_io_c::_write(const void *buffer, size_t size) {

  size_t avail;
  char *buf = (char*)buffer;
  size_t remain = size;
  
  // whole blocks
  while (remain >= (avail = m_size - m_fill)) {
    if (m_fill) {
      // Fill the buffer in an attempt to defeat potentially 
      // lousy OS I/O scheduling
      memcpy(m_buffer + m_fill, buf, avail);
      m_fill = m_size;
      flush_buffer();
      remain -= avail;
      buf += avail;
    }
    else {
      // write whole blocks, skipping the buffer
      avail = mm_proxy_io_c::_write(buf, m_size);
      if (avail != m_size)
        throw mtx::mm_io::insufficient_space_x();
      remain -= avail;
      buf += avail;
    }
  }
  if (remain) {
    memcpy(m_buffer + m_fill, buf, remain);
    m_fill += remain;
  }
  return size;
}

void
mm_wbuffer_io_c::flush_buffer() {
  if (!m_fill)
    return;

  size_t written = mm_proxy_io_c::_write(m_buffer, m_fill), fill = m_fill;
  m_fill = 0;
  mxverb(2, boost::format("mm_wbuffer_io_c::flush_buffer(): requested %1% written %2%\n") % fill % written);
  if (written != fill)
    throw mtx::mm_io::insufficient_space_x();
}
