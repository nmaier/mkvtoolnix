/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Rewritten by Nils Maier <maierman@web.de>
*/

#include "common/common_pch.h"

#include "common/mm_io_x.h"
#include "common/mm_read_buffer_io.h"

mm_read_buffer_io_c::mm_read_buffer_io_c(mm_io_c *in,
                                         size_t buffer_size,
                                         bool delete_in)
  : mm_proxy_io_c(in, delete_in)
  , m_af_buffer(memory_c::alloc(buffer_size))
  , m_buffer(m_af_buffer->get_buffer())
  , m_cursor(0)
  , m_eof(false)
  , m_fill(0)
  , m_offset(0)
  , m_size(buffer_size)
  , m_buffering(true)
  , m_debug_seek(debugging_requested("read_buffer_io") || debugging_requested("read_buffer_io_read"))
  , m_debug_read(debugging_requested("read_buffer_io") || debugging_requested("read_buffer_io_read"))
{
  setFilePointer(0, seek_beginning);
}

mm_read_buffer_io_c::~mm_read_buffer_io_c() {
  close();
}

uint64
mm_read_buffer_io_c::getFilePointer() {
  return m_buffering ? m_offset + m_cursor : m_proxy_io->getFilePointer();
}

void
mm_read_buffer_io_c::setFilePointer(int64 offset,
                                    seek_mode mode) {
  if (!m_buffering) {
    m_proxy_io->setFilePointer(offset, mode);
    return;
  }

  int64_t new_pos = 0;
  // FIXME int64_t overflow

  // No need to actually compute this here; _read() will do just that
  m_eof = false;

  switch (mode) {
    case seek_beginning:
      new_pos = offset;
      break;

    case seek_current:
      new_pos  = m_offset;
      new_pos += m_cursor;
      new_pos += offset;
      break;

    case seek_end:
      new_pos = -1;
      break;

    default:
      throw mtx::mm_io::seek_x();
  }

  // Still within the current buffer?
  int64_t in_buf = new_pos - m_offset;
  if ((0 <= in_buf) && (in_buf <= static_cast<int64_t>(m_fill))) {
    m_cursor = in_buf;
    return;
  }

  int64_t previous_pos = m_proxy_io->getFilePointer();

  // Actual seeking
  if (new_pos < 0)
    m_proxy_io->setFilePointer(offset, seek_end);
  else
    m_proxy_io->setFilePointer(std::min(new_pos, get_size()), seek_beginning);

  // Get the actual offset from the underlying stream
  // Better be safe than sorry and use this instead of just taking
  m_offset = m_proxy_io->getFilePointer();

  // "Drop" the buffer content
  m_cursor = m_fill = 0;

  mxdebug_if(m_debug_seek, boost::format("seek on proxy from %1% to %2% relative %3%\n") % previous_pos % m_offset % (m_offset - previous_pos));
}

int64_t
mm_read_buffer_io_c::get_size() {
  return m_proxy_io->get_size();
}

uint32
mm_read_buffer_io_c::_read(void *buffer,
                           size_t size) {
  if (!m_buffering)
    return m_proxy_io->read(buffer, size);

  char *buf    = static_cast<char *>(buffer);
  uint32_t res = 0;

  while (0 < size) {
    // TODO Directly write full blocks into the output buffer when size > m_size
    size_t avail = std::min(size, m_fill - m_cursor);
    if (avail) {
      memcpy(buf, m_buffer + m_cursor, avail);
      buf      += avail;
      res      += avail;
      size     -= avail;
      m_cursor += avail;

    } else {
      // Refill the buffer
      m_offset += m_cursor;
      m_cursor  = 0;
      m_fill    = 0;
      avail     = std::min(get_size() - m_offset, static_cast<int64_t>(m_size));

      if (!avail) {
        // must keep track of eof, as m_proxy_io->eof() will never be reached
        // because of the above eof calculation
        m_eof = true;
        break;
      }

      int64_t previous_pos = m_proxy_io->getFilePointer();

      m_fill = m_proxy_io->read(m_buffer, avail);
      mxdebug_if(m_debug_read, boost::format("physical read from position %3% for %1% returned %2%\n") % avail % m_fill % previous_pos);
      if (m_fill != avail) {
        m_eof = true;
        if (!m_fill)
          break;
      }
    }
  }

  return res;
}

size_t
mm_read_buffer_io_c::_write(const void *,
                            size_t) {
  throw mtx::mm_io::wrong_read_write_access_x();
  return 0;
}

void
mm_read_buffer_io_c::enable_buffering(bool enable) {
  m_buffering = enable;
  if (!m_buffering) {
    m_offset = 0;
    m_cursor = 0;
    m_fill   = 0;
  }
}
