/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_MM_WRITE_CACHE_IO_H
#define __MTX_COMMON_MM_WRITE_CACHE_IO_H

#include "common/common_pch.h"

#include "common/mm_io.h"

class mm_write_cache_io_c: public mm_proxy_io_c {
protected:
  memory_cptr m_af_cache;
  size_t m_cache_pos, m_cache_size;
  unsigned char *m_cache;

public:
  mm_write_cache_io_c(mm_io_c *p_out, size_t cache_size, bool p_delete_out = true);
  virtual ~mm_write_cache_io_c();

  virtual uint64 getFilePointer();
  virtual void setFilePointer(int64 offset, seek_mode mode = seek_beginning);
  virtual void flush();
  virtual void close();

  static mm_write_cache_io_c *open(const std::string &file_name, size_t cache_size);

protected:
  virtual uint32 _read(void *buffer, size_t size);
  virtual size_t _write(const void *buffer, size_t size);
  virtual void flush_cache();
};
typedef counted_ptr<mm_write_cache_io_c> mm_write_cache_io_cptr;

#endif // __MTX_COMMON_MM_WRITE_CACHED_FILE_IO_H
