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

#include "common/mm_write_buffer_io.h"

mm_write_buffer_io_c::mm_write_buffer_io_c(mm_io_c *out,
                                           size_t buffer_size,
                                           bool delete_out)
  : mm_proxy_io_c(out, delete_out)
  , m_af_buffer(memory_c::alloc(buffer_size))
  , m_buffer(m_af_buffer->get_buffer())
  , m_fill(0)
  , m_size(buffer_size)
  , m_debug_seek( debugging_requested("write_buffer_io") || debugging_requested("write_buffer_io_read"))
  , m_debug_write(debugging_requested("write_buffer_io") || debugging_requested("write_buffer_io_write"))
{
}

mm_write_buffer_io_c::~mm_write_buffer_io_c() {
  close();
}

mm_io_cptr
mm_write_buffer_io_c::open(const std::string &file_name,
                           size_t buffer_size) {
  return mm_io_cptr(new mm_write_buffer_io_c(new mm_file_io_c(file_name, MODE_CREATE), buffer_size));
}

uint64
mm_write_buffer_io_c::getFilePointer() {
  return mm_proxy_io_c::getFilePointer() + m_fill;
}

void
mm_write_buffer_io_c::setFilePointer(int64 offset,
                                     seek_mode mode) {
  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? get_size()       - offset
    :                          getFilePointer() + offset;

  if (new_pos == static_cast<int64_t>(getFilePointer()))
    return;

  flush_buffer();

  if (m_debug_seek) {
    int64_t previous_pos = mm_proxy_io_c::getFilePointer();
    mxdebug(boost::format("seek from %1% to %2% diff %3%\n") % previous_pos % new_pos % (new_pos - previous_pos));
  }

  mm_proxy_io_c::setFilePointer(offset, mode);
}

void
mm_write_buffer_io_c::flush() {
  flush_buffer();
  mm_proxy_io_c::flush();
}

void
mm_write_buffer_io_c::close() {
  flush_buffer();
  mm_proxy_io_c::close();
}

uint32
mm_write_buffer_io_c::_read(void *,
                            size_t) {
  throw mtx::mm_io::wrong_read_write_access_x();
  return 0;
}

size_t
mm_write_buffer_io_c::_write(const void *buffer,
                             size_t size) {
  size_t avail;
  const char *buf = static_cast<const char *>(buffer);
  size_t remain   = size;

  // whole blocks
  while (remain >= (avail = m_size - m_fill)) {
    if (m_fill) {
      // Fill the buffer in an attempt to defeat potentially
      // lousy OS I/O scheduling
      memcpy(m_buffer + m_fill, buf, avail);
      m_fill = m_size;
      flush_buffer();
      remain -= avail;
      buf    += avail;

    } else {
      // write whole blocks, skipping the buffer
      avail = mm_proxy_io_c::_write(buf, m_size);
      if (avail != m_size)
        throw mtx::mm_io::insufficient_space_x();

      remain -= avail;
      buf    += avail;
    }
  }

  if (remain) {
    memcpy(m_buffer + m_fill, buf, remain);
    m_fill += remain;
  }

  return size;
}

void
mm_write_buffer_io_c::flush_buffer() {
  if (!m_fill)
    return;

  size_t written = mm_proxy_io_c::_write(m_buffer, m_fill);
  size_t fill    = m_fill;
  m_fill         = 0;

  mxdebug_if(m_debug_write, boost::format("flush_buffer() at %1% for %2% written %3%\n") % (mm_proxy_io_c::getFilePointer() - written) % fill % written);

  if (written != fill)
    throw mtx::mm_io::insufficient_space_x();
}
