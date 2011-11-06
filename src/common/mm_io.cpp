/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   IO callback class implementation

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modifications by Peter Niemayer <niemayer@isg.de>.
*/

#include "common/common_pch.h"

#include <errno.h>
#if HAVE_POSIX_FADVISE
# include <fcntl.h>
# include <sys/utsname.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include "common/endian.h"
#include "common/error.h"
#include "common/fs_sys_helpers.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"

#if !defined(SYS_WINDOWS)
static std::string
get_errno_msg() {
  return g_cc_local_utf8->utf8(strerror(errno));
}

# if HAVE_POSIX_FADVISE
static const unsigned long s_read_using_willneed   = 16 * 1024 * 1024;
static const unsigned long s_write_before_dontneed =  8 * 1024 * 1024;
bool mm_file_io_c::ms_use_posix_fadvise            = false;

bool
operator <(const file_id_t &file_id_1,
           const file_id_t &file_id_2) {
  return (file_id_1.m_dev < file_id_2.m_dev) || ((file_id_1.m_dev == file_id_2.m_dev) && (file_id_1.m_ino < file_id_2.m_ino));
}

bool
operator ==(const file_id_t &file_id_1,
            const file_id_t &file_id_2) {
  return (file_id_1.m_dev == file_id_2.m_dev) && (file_id_1.m_ino == file_id_2.m_ino);
}

static std::map<file_id_t, int> s_fadvised_file_count_by_id;
static std::map<file_id_t, mm_file_io_c *> s_fadvised_file_object_by_id;
# endif

#define FADVISE_WARNING                                                         \
  "mm_file_io_c: Could not posix_fadvise() file '%1%' (errno = %2%, %3%). "     \
  "This only means that access to this file might be slower than it could be. " \
  "Data integrity is not in danger."

mm_file_io_c::mm_file_io_c(const std::string &path,
                           const open_mode mode)
  : m_file_name(path)
  , m_file(NULL)
#if HAVE_POSIX_FADVISE
  , m_read_count(0)
  , m_write_count(0)
  , m_use_posix_fadvise_here(ms_use_posix_fadvise)
#endif
{
  const char *cmode;
# if HAVE_POSIX_FADVISE
  int advise = 0;
# endif

  switch (mode) {
    case MODE_READ:
      cmode = "rb";
# if HAVE_POSIX_FADVISE
      advise = POSIX_FADV_WILLNEED;
# endif
      break;
    case MODE_WRITE:
      cmode = "r+b";
# if HAVE_POSIX_FADVISE
      advise = POSIX_FADV_DONTNEED;
# endif
      break;
    case MODE_CREATE:
      cmode = "w+b";
# if HAVE_POSIX_FADVISE
      advise = POSIX_FADV_DONTNEED;
# endif
      break;
    case MODE_SAFE:
      cmode = "rb";
# if HAVE_POSIX_FADVISE
      advise = POSIX_FADV_WILLNEED;
# endif
      break;
    default:
      throw mtx::invalid_parameter_x();
  }

  if ((MODE_WRITE == mode) || (MODE_CREATE == mode))
    prepare_path(path);
  std::string local_path = g_cc_local_utf8->native(path);

  struct stat st;
  if ((0 == stat(local_path.c_str(), &st)) && S_ISDIR(st.st_mode))
    throw mtx::mm_io::open_x();

  m_file = (FILE *)fopen(local_path.c_str(), cmode);

  if (NULL == m_file)
    throw mtx::mm_io::open_x();

# if HAVE_POSIX_FADVISE
  if (POSIX_FADV_WILLNEED == advise)
    setup_fadvise(local_path);
# endif
}

# if HAVE_POSIX_FADVISE
void
mm_file_io_c::setup_fadvise(const std::string &local_path) {
  struct stat st;
  if (0 != stat(local_path.c_str(), &st))
    throw mtx::mm_io::open_x();

  m_file_id.initialize(st);

  if (!map_has_key(s_fadvised_file_count_by_id, m_file_id))
    s_fadvised_file_count_by_id[m_file_id] = 0;

  s_fadvised_file_count_by_id[m_file_id]++;

  if (1 < s_fadvised_file_count_by_id[m_file_id]) {
    m_use_posix_fadvise_here = false;
    if (map_has_key(s_fadvised_file_object_by_id, m_file_id))
      s_fadvised_file_object_by_id[m_file_id]->m_use_posix_fadvise_here = false;

  } else
    s_fadvised_file_object_by_id[m_file_id] = this;
}
#endif  // HAVE_POSIX_FADVISE

void
mm_file_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  int whence = mode == seek_beginning ? SEEK_SET
             : mode == seek_end       ? SEEK_END
             :                          SEEK_CUR;

  if (fseeko((FILE *)m_file, offset, whence) != 0)
    throw mtx::mm_io::seek_x();

  if (mode == seek_beginning)
    m_current_position = offset;
  else if (mode == seek_end)
    m_current_position = ftello((FILE *)m_file);
  else
    m_current_position += offset;
}

size_t
mm_file_io_c::_write(const void *buffer,
                     size_t size) {
  size_t bwritten = fwrite(buffer, 1, size, (FILE *)m_file);
  if (ferror((FILE *)m_file) != 0)
    mxerror(boost::format(Y("Could not write to the output file: %1% (%2%)\n")) % errno % get_errno_msg());

# if HAVE_POSIX_FADVISE
  m_write_count += bwritten;
  if (ms_use_posix_fadvise && m_use_posix_fadvise_here && (m_write_count > s_write_before_dontneed)) {
    uint64 pos    = getFilePointer();
    m_write_count = 0;

    if (0 != posix_fadvise(fileno((FILE *)m_file), 0, pos, POSIX_FADV_DONTNEED)) {
      mxverb(2, boost::format(FADVISE_WARNING) % m_file_name % errno % get_errno_msg());
      m_use_posix_fadvise_here = false;
    }
  }
# endif

  m_current_position += bwritten;
  m_cached_size       = -1;

  return bwritten;
}

uint32
mm_file_io_c::_read(void *buffer,
                    size_t size) {
  int64_t bread = fread(buffer, 1, size, (FILE *)m_file);

# if HAVE_POSIX_FADVISE
  if (ms_use_posix_fadvise && m_use_posix_fadvise_here && (0 <= bread)) {
    m_read_count += bread;
    if (m_read_count > s_read_using_willneed) {
      uint64 pos   = getFilePointer();
      int fd       = fileno((FILE *)m_file);
      m_read_count = 0;

      if (0 != posix_fadvise(fd, 0, pos, POSIX_FADV_DONTNEED)) {
        mxverb(2, boost::format(FADVISE_WARNING) % m_file_name % errno % get_errno_msg());
        m_use_posix_fadvise_here = false;
      }

      if (m_use_posix_fadvise_here && (0 != posix_fadvise(fd, pos, pos + s_read_using_willneed, POSIX_FADV_WILLNEED))) {
        mxverb(2, boost::format(FADVISE_WARNING) % m_file_name % errno % get_errno_msg());
        m_use_posix_fadvise_here = false;
      }
    }
  }
# endif

  m_current_position += bread;

  return bread;
}

void
mm_file_io_c::close() {
  if (NULL != m_file) {
    fclose((FILE *)m_file);
    m_file = NULL;

# if HAVE_POSIX_FADVISE
    if (m_file_id.m_initialized) {
      s_fadvised_file_count_by_id[m_file_id]--;
      if (0 == s_fadvised_file_count_by_id[m_file_id])
        s_fadvised_file_object_by_id.erase(m_file_id);

      m_file_id.m_initialized = false;
    }
# endif
  }
}

bool
mm_file_io_c::eof() {
  return feof((FILE *)m_file) != 0;
}

int
mm_file_io_c::truncate(int64_t pos) {
  m_cached_size = -1;
  return ftruncate(fileno((FILE *)m_file), pos);
}

/** \brief OS and kernel dependant setup

   The \c posix_fadvise call can improve read/write performance a lot.
   Unfortunately it is pretty new and a buggy on a couple of Linux kernels
   in the 2.4.x series. So only enable its usage for 2.6.x kernels.
*/
void
mm_file_io_c::setup() {
# if HAVE_POSIX_FADVISE
  struct utsname un;

  ms_use_posix_fadvise = false;
  if ((0 == uname(&un)) && !strcasecmp(un.sysname, "Linux")) {
    std::vector<std::string> versions = split(un.release, ".");
    int major, minor;

    if ((2 <= versions.size()) && parse_int(versions[0], major) && parse_int(versions[1], minor) && ((2 < major) || (6 <= minor)))
      ms_use_posix_fadvise = true;
  }
# endif // HAVE_POSIX_FADVISE
}

#endif // !defined(SYS_WINDOWS)

void
mm_file_io_c::prepare_path(const std::string &path) {
  std::string local_path = path; // contains copy of given path
  std::string cur_path;

#if defined(SYS_WINDOWS)
  const std::string SEPARATOR ("\\");
  // convert separators for current OS
  std::replace(local_path.begin(), local_path.end(), '/', '\\');

  if (local_path.substr(0, 2) == "\\\\") {
    // UNC paths -- don't try to create the host name as a directory.

    // Find the host name
    std::string::size_type position = local_path.find(SEPARATOR, 2);
    if (std::string::npos == position)
      return;

    // Find the share name
    position = local_path.find(SEPARATOR, position + 1);
    if (std::string::npos == position)
      return;

    cur_path = local_path.substr(0, position);
    local_path.erase(0, position + 1);

  } else if ((local_path.size() > 1) && (local_path[1] == ':')) {
    // Skip drive letters.
    cur_path = local_path.substr(0, 2);
    local_path.erase(0, 3);

  }

#else  // SYS_WINDOWS
  const std::string SEPARATOR ("/");
  // convert separators for current OS
  std::replace(local_path.begin(), local_path.end(), '\\', '/');
#endif // SYS_WINDOWS

  if (local_path.substr(0, SEPARATOR.length()) == SEPARATOR)
    cur_path = SEPARATOR;

  std::vector<std::string> parts = split(local_path, SEPARATOR);

  // The file name is the last element -- remove it.
  parts.pop_back();

  for (size_t i = 0; parts.size() > i; ++i) {
    // Ignore empty path components.
    if (parts[i].empty())
      continue;

    if (!cur_path.empty() && (cur_path != SEPARATOR))
      cur_path += SEPARATOR;
    cur_path   += parts[i];

    if (!fs_entry_exists(cur_path.c_str()))
      create_directory(cur_path.c_str());
  }
}

uint64
mm_file_io_c::getFilePointer() {
  return m_current_position;
}

mm_file_io_c::~mm_file_io_c() {
  close();
  m_file_name = "";
}

void
mm_file_io_c::cleanup() {
#if HAVE_POSIX_FADVISE
  s_fadvised_file_count_by_id.clear();
  s_fadvised_file_object_by_id.clear();
#endif
}

/*
   Abstract base class.
*/

std::string
mm_io_c::getline() {
  char c;
  std::string s;

  if (eof())
    throw mtx::mm_io::end_of_file_x();

  while (read(&c, 1) == 1) {
    if (c == '\r')
      continue;
    if (c == '\n')
      return s;
    s += c;
  }

  return s;
}

bool
mm_io_c::getline2(std::string &s) {
  try {
    s = getline();
  } catch(...) {
    return false;
  }

  return true;
}

bool
mm_io_c::setFilePointer2(int64 offset, seek_mode mode) {
  try {
    setFilePointer(offset, mode);
    return true;
  } catch(...) {
    return false;
  }
}

size_t
mm_io_c::puts(const std::string &s) {
#if defined(SYS_WINDOWS)
  bool is_windows    = true;
#else
  bool is_windows    = false;
#endif
  size_t num_written = 0;
  const char *cs     = s.c_str();
  int prev_pos       = 0;
  int cur_pos;

  for (cur_pos = 0; cs[cur_pos] != 0; ++cur_pos) {
    bool keep_char = is_windows || ('\r' != cs[cur_pos]) || ('\n' != cs[cur_pos + 1]);
    bool insert_cr = is_windows && ('\n' == cs[cur_pos]) && ((0 == cur_pos) || ('\r' != cs[cur_pos - 1]));

    if (keep_char && !insert_cr)
      continue;

    if (prev_pos < cur_pos)
      num_written += write(&cs[prev_pos], cur_pos - prev_pos);

    if (insert_cr)
      num_written += write("\r", 1);

    prev_pos = cur_pos + (keep_char ? 0 : 1);
  }

  if (prev_pos < cur_pos)
    num_written += write(&cs[prev_pos], cur_pos - prev_pos);

  return num_written;
}

uint32_t
mm_io_c::read(void *buffer,
              size_t size) {
  return _read(buffer, size);
}

uint32_t
mm_io_c::read(std::string &buffer,
              size_t size) {
  char *cbuffer = new char[size + 1];
  int nread     = read(buffer, size);

  if (0 > nread)
    buffer = "";
  else {
    cbuffer[nread] = 0;
    buffer = cbuffer;
  }
  delete [] cbuffer;

  return nread;
}

unsigned char
mm_io_c::read_uint8() {
  unsigned char value;

  if (read(&value, 1) != 1)
    throw mtx::mm_io::end_of_file_x();

  return value;
}

uint16_t
mm_io_c::read_uint16_le() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw mtx::mm_io::end_of_file_x();

  return get_uint16_le(buffer);
}

uint32_t
mm_io_c::read_uint24_le() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw mtx::mm_io::end_of_file_x();

  return get_uint24_le(buffer);
}

uint32_t
mm_io_c::read_uint32_le() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw mtx::mm_io::end_of_file_x();

  return get_uint32_le(buffer);
}

uint64_t
mm_io_c::read_uint64_le() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw mtx::mm_io::end_of_file_x();

  return get_uint64_le(buffer);
}

uint16_t
mm_io_c::read_uint16_be() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw mtx::mm_io::end_of_file_x();

  return get_uint16_be(buffer);
}

uint32_t
mm_io_c::read_uint24_be() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw mtx::mm_io::end_of_file_x();

  return get_uint24_be(buffer);
}

uint32_t
mm_io_c::read_uint32_be() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw mtx::mm_io::end_of_file_x();

  return get_uint32_be(buffer);
}

uint64_t
mm_io_c::read_uint64_be() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw mtx::mm_io::end_of_file_x();

  return get_uint64_be(buffer);
}

uint32_t
mm_io_c::read(memory_cptr &buffer,
              size_t size,
              int offset) {
  if (-1 == offset)
    offset = buffer->get_size();

  if (buffer->get_size() <= (size + static_cast<size_t>(offset)))
    buffer->resize(size + offset);

  if (read(buffer->get_buffer() + offset, size) != size)
    throw mtx::mm_io::end_of_file_x();

  buffer->set_size(size + offset);

  return size;
}

int
mm_io_c::write_uint8(unsigned char value) {
  return write(&value, 1);
}

int
mm_io_c::write_uint16_le(uint16_t value) {
  uint16_t buffer;

  put_uint16_le(&buffer, value);
  return write(&buffer, sizeof(uint16_t));
}

int
mm_io_c::write_uint32_le(uint32_t value) {
  uint32_t buffer;

  put_uint32_le(&buffer, value);
  return write(&buffer, sizeof(uint32_t));
}

int
mm_io_c::write_uint64_le(uint64_t value) {
  uint64_t buffer;

  put_uint64_le(&buffer, value);
  return write(&buffer, sizeof(uint64_t));
}

int
mm_io_c::write_uint16_be(uint16_t value) {
  uint16_t buffer;

  put_uint16_be(&buffer, value);
  return write(&buffer, sizeof(uint16_t));
}

int
mm_io_c::write_uint32_be(uint32_t value) {
  uint32_t buffer;

  put_uint32_be(&buffer, value);
  return write(&buffer, sizeof(uint32_t));
}

int
mm_io_c::write_uint64_be(uint64_t value) {
  uint64_t buffer;

  put_uint64_be(&buffer, value);
  return write(&buffer, sizeof(uint64_t));
}

size_t
mm_io_c::write(const void *buffer,
               size_t size) {
  return _write(buffer, size);
}

size_t
mm_io_c::write(const memory_cptr &buffer,
               size_t size,
               size_t offset) {
  size = std::min(buffer->get_size() - offset, size);

  if (write(buffer->get_buffer() + offset, size) != size)
    throw mtx::mm_io::end_of_file_x();
  return size;
}

void
mm_io_c::skip(int64 num_bytes) {
  uint64_t pos = getFilePointer();
  setFilePointer(pos + num_bytes);
  if ((pos + num_bytes) != getFilePointer())
    throw mtx::mm_io::end_of_file_x();
}

void
mm_io_c::save_pos(int64_t new_pos) {
  m_positions.push(getFilePointer());

  if (-1 != new_pos)
    setFilePointer(new_pos);
}

bool
mm_io_c::restore_pos() {
  if (m_positions.empty())
    return false;

  setFilePointer(m_positions.top());
  m_positions.pop();

  return true;
}

bool
mm_io_c::write_bom(const std::string &charset) {
  static const unsigned char utf8_bom[3]    = {0xef, 0xbb, 0xbf};
  static const unsigned char utf16le_bom[2] = {0xff, 0xfe};
  static const unsigned char utf16be_bom[2] = {0xfe, 0xff};
  static const unsigned char utf32le_bom[4] = {0xff, 0xfe, 0x00, 0x00};
  static const unsigned char utf32be_bom[4] = {0x00, 0x00, 0xff, 0xfe};
  const unsigned char *bom;
  unsigned int bom_len;

  if (charset.empty())
    return false;

  if ((charset =="UTF-8") || (charset =="UTF8")) {
    bom_len = 3;
    bom     = utf8_bom;
  } else if ((charset =="UTF-16") || (charset =="UTF-16LE") || (charset =="UTF16") || (charset =="UTF16LE")) {
    bom_len = 2;
    bom     = utf16le_bom;
  } else if ((charset =="UTF-16BE") || (charset =="UTF16BE")) {
    bom_len = 2;
    bom     = utf16be_bom;
  } else if ((charset =="UTF-32") || (charset =="UTF-32LE") || (charset =="UTF32") || (charset =="UTF32LE")) {
    bom_len = 4;
    bom     = utf32le_bom;
  } else if ((charset =="UTF-32BE") || (charset =="UTF32BE")) {
    bom_len = 4;
    bom     = utf32be_bom;
  } else
    return false;

  setFilePointer(0, seek_beginning);

  return (write(bom, bom_len) == bom_len);
}

int64_t
mm_io_c::get_size() {
  if (-1 == m_cached_size) {
    save_pos();
    setFilePointer(0, seek_end);
    m_cached_size = getFilePointer();
    restore_pos();
  }

  return m_cached_size;
}

int
mm_io_c::getch() {
  unsigned char c;

  if (read(&c, 1) != 1)
    return -1;

  return c;
}

/*
   Proxy class that does I/O on a mm_io_c handed over in the ctor.
   Useful for e.g. doing text I/O on other I/Os (file, mem).
*/

void
mm_proxy_io_c::close() {
  if ((NULL != m_proxy_io) && m_proxy_delete_io)
    delete m_proxy_io;

  m_proxy_io = NULL;
}


uint32
mm_proxy_io_c::_read(void *buffer,
                     size_t size) {
  return m_proxy_io->read(buffer, size);
}

size_t
mm_proxy_io_c::_write(const void *buffer,
                      size_t size) {
  return m_proxy_io->write(buffer, size);
}

/*
   Dummy class for output to /dev/null. Needed for two pass stuff.
*/

mm_null_io_c::mm_null_io_c()
  : m_pos(0)
{
}

uint64
mm_null_io_c::getFilePointer() {
  return m_pos;
}

void
mm_null_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  m_pos = seek_beginning == mode ? offset
        : seek_end       == mode ? 0
        :                          m_pos + offset;
}

uint32
mm_null_io_c::_read(void *buffer,
                    size_t size) {
  memset(buffer, 0, size);
  m_pos += size;

  return size;
}

size_t
mm_null_io_c::_write(const void *buffer,
                     size_t size) {
  m_pos += size;

  return size;
}

void
mm_null_io_c::close() {
}

/*
   IO callback class working on memory
*/
mm_mem_io_c::mm_mem_io_c(unsigned char *mem,
                         uint64_t mem_size,
                         int increase)
  : m_pos(0)
  , m_mem_size(mem_size)
  , m_allocated(mem_size)
  , m_increase(increase)
  , m_mem(mem)
  , m_ro_mem(NULL)
  , m_read_only(false)
{
  if (0 >= m_increase)
    throw mtx::invalid_parameter_x();

  if ((NULL == m_mem) && (0 < m_increase)) {
    if (0 == mem_size)
      m_allocated = increase;

    m_mem      = safemalloc(m_allocated);
    m_free_mem = true;

  } else
    m_free_mem = false;
}

mm_mem_io_c::mm_mem_io_c(const unsigned char *mem,
                         uint64_t mem_size)
  : m_pos(0)
  , m_mem_size(mem_size)
  , m_allocated(mem_size)
  , m_increase(0)
  , m_mem(NULL)
  , m_ro_mem(mem)
  , m_free_mem(false)
  , m_read_only(true)
{
  if (NULL == m_ro_mem)
    throw mtx::invalid_parameter_x();
}

mm_mem_io_c::~mm_mem_io_c() {
  close();
}

uint64
mm_mem_io_c::getFilePointer() {
  return m_pos;
}

void
mm_mem_io_c::setFilePointer(int64 offset,
                            seek_mode mode) {
  if ((NULL == m_mem) && (NULL == m_ro_mem) && (0 == m_mem_size))
    throw mtx::invalid_parameter_x();

  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? m_mem_size - offset
    :                          m_pos      + offset;

  if ((0 <= new_pos) && (static_cast<int64_t>(m_mem_size) >= new_pos))
    m_pos = new_pos;
  else
    throw mtx::mm_io::seek_x();
}

uint32
mm_mem_io_c::_read(void *buffer,
                   size_t size) {
  size_t rbytes = std::min(size, m_mem_size - m_pos);
  if (m_read_only)
    memcpy(buffer, &m_ro_mem[m_pos], rbytes);
  else
    memcpy(buffer, &m_mem[m_pos], rbytes);
  m_pos += rbytes;

  return rbytes;
}

size_t
mm_mem_io_c::_write(const void *buffer,
                    size_t size) {
  if (m_read_only)
    throw mtx::mm_io::wrong_read_write_access_x();

  int64_t wbytes;
  if ((m_pos + size) >= m_allocated) {
    if (m_increase) {
      int64_t new_allocated  = m_pos + size - m_allocated;
      new_allocated          = ((new_allocated / m_increase) + 1 ) * m_increase;
      m_allocated           += new_allocated;
      m_mem                  = (unsigned char *)saferealloc(m_mem, m_allocated);
      wbytes                 = size;
    } else
      wbytes                 = m_allocated - m_pos;

  } else
    wbytes = size;

  if ((m_pos + size) > m_mem_size)
    m_mem_size = m_pos + size;

  memcpy(&m_mem[m_pos], buffer, wbytes);
  m_pos         += wbytes;
  m_cached_size  = -1;

  return wbytes;
}

void
mm_mem_io_c::close() {
  if (m_free_mem)
    safefree(m_mem);
  m_mem       = NULL;
  m_ro_mem    = NULL;
  m_read_only = true;
  m_free_mem  = false;
  m_mem_size  = 0;
  m_increase  = 0;
  m_pos       = 0;
}

bool
mm_mem_io_c::eof() {
  return m_pos >= m_mem_size;
}

unsigned char *
mm_mem_io_c::get_and_lock_buffer() {
  m_free_mem = false;
  return m_mem;
}

/*
   Class for handling UTF-8/UTF-16/UTF-32 text files.
*/

mm_text_io_c::mm_text_io_c(mm_io_c *in,
                           bool delete_in)
  : mm_proxy_io_c(in, delete_in)
  , m_byte_order(BO_NONE)
  , m_bom_len(0)
  , m_uses_carriage_returns(false)
  , m_uses_newlines(false)
  , m_eol_style_detected(false)
{
  in->setFilePointer(0, seek_beginning);

  unsigned char buffer[4];
  int num_read = in->read(buffer, 4);
  if (2 > num_read) {
    in->setFilePointer(0, seek_beginning);
    return;
  }

  detect_byte_order_marker(buffer, num_read, m_byte_order, m_bom_len);

  in->setFilePointer(m_bom_len, seek_beginning);
}

void
mm_text_io_c::detect_eol_style() {
  if (m_eol_style_detected)
    return;

  m_eol_style_detected = true;
  bool found_cr_or_nl  = false;

  save_pos();

  while (1) {
    char utf8char[9];
    size_t len = read_next_char(utf8char);
    if (0 == len)
      break;

    if ((1 == len) && ('\r' == utf8char[0])) {
      found_cr_or_nl          = true;
      m_uses_carriage_returns = true;

    } else if ((1 == len) && ('\n' == utf8char[0])) {
      found_cr_or_nl  = true;
      m_uses_newlines = true;

    } else if (found_cr_or_nl)
      break;
  }

  restore_pos();
}

bool
mm_text_io_c::detect_byte_order_marker(const unsigned char *buffer,
                                       unsigned int size,
                                       byte_order_e &byte_order,
                                       unsigned int &bom_length) {
  if ((3 <= size) && (buffer[0] == 0xef) && (buffer[1] == 0xbb) && (buffer[2] == 0xbf)) {
    byte_order = BO_UTF8;
    bom_length = 3;
  } else if ((4 <= size) && (buffer[0] == 0xff) && (buffer[1] == 0xfe) && (buffer[2] == 0x00) && (buffer[3] == 0x00)) {
    byte_order = BO_UTF32_LE;
    bom_length = 4;
  } else if ((4 <= size) && (buffer[0] == 0x00) && (buffer[1] == 0x00) && (buffer[2] == 0xfe) && (buffer[3] == 0xff)) {
    byte_order = BO_UTF32_BE;
    bom_length = 4;
  } else if ((2 <= size) && (buffer[0] == 0xff) && (buffer[1] == 0xfe)) {
    byte_order = BO_UTF16_LE;
    bom_length = 2;
  } else if ((2 <= size) && (buffer[0] == 0xfe) && (buffer[1] == 0xff)) {
    byte_order = BO_UTF16_BE;
    bom_length = 2;
  } else {
    byte_order = BO_NONE;
    bom_length = 0;
  }

  return BO_NONE != byte_order;
}

bool
mm_text_io_c::has_byte_order_marker(const std::string &string) {
  byte_order_e byte_order;
  unsigned int bom_length;
  return detect_byte_order_marker(reinterpret_cast<const unsigned char *>(string.c_str()), string.length(), byte_order, bom_length);
}

// 1 byte: 0xxxxxxx,
// 2 bytes: 110xxxxx 10xxxxxx,
// 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx

int
mm_text_io_c::read_next_char(char *buffer) {
  if (BO_NONE == m_byte_order)
    return read(buffer, 1);

  unsigned char stream[6];
  size_t size = 0;
  if (BO_UTF8 == m_byte_order) {
    if (read(stream, 1) != 1)
      return 0;

    size = ((stream[0] & 0x80) == 0x00) ?  1
         : ((stream[0] & 0xe0) == 0xc0) ?  2
         : ((stream[0] & 0xf0) == 0xe0) ?  3
         : ((stream[0] & 0xf8) == 0xf0) ?  4
         : ((stream[0] & 0xfc) == 0xf8) ?  5
         : ((stream[0] & 0xfe) == 0xfc) ?  6
         :                                99;

    if (99 == size)
      mxerror(boost::format(Y("mm_text_io_c::read_next_char(): Invalid UTF-8 char. First byte: 0x%|1$02x|")) % (unsigned int)stream[0]);

    if ((1 < size) && (read(&stream[1], size - 1) != (size - 1)))
      return 0;

    memcpy(buffer, stream, size);

    return size;

  } else if ((BO_UTF16_LE == m_byte_order) || (BO_UTF16_BE == m_byte_order))
    size = 2;
  else
    size = 4;

  if (read(stream, size) != size)
    return 0;

  unsigned long data = 0;
  size_t i;
  if ((BO_UTF16_LE == m_byte_order) || (BO_UTF32_LE == m_byte_order))
    for (i = 0; i < size; i++) {
      data <<= 8;
      data  |= stream[size - i - 1];
    }
  else
    for (i = 0; i < size; i++) {
      data <<= 8;
      data  |= stream[i];
    }

  if (data < 0x80) {
    buffer[0] = data;
    return 1;
  }

  if (data < 0x800) {
    buffer[0] = 0xc0 | (data >> 6);
    buffer[1] = 0x80 | (data & 0x3f);
    return 2;
  }

  if (data < 0x10000) {
    buffer[0] = 0xe0 |  (data >> 12);
    buffer[1] = 0x80 | ((data >> 6) & 0x3f);
    buffer[2] = 0x80 |  (data       & 0x3f);
    return 3;
  }

  mxerror(Y("mm_text_io_c: UTF32_* is not supported at the moment.\n"));

  return 0;
}

std::string
mm_text_io_c::getline() {
  if (eof())
    throw mtx::mm_io::end_of_file_x();

  if (!m_eol_style_detected)
    detect_eol_style();

  std::string s;
  char utf8char[9];
  bool previous_was_carriage_return = false;

  while (1) {
    memset(utf8char, 0, 9);

    int len = read_next_char(utf8char);
    if (0 == len)
      return s;

    if ((1 == len) && (utf8char[0] == '\r')) {
      if (previous_was_carriage_return && !m_uses_newlines) {
        setFilePointer(-1, seek_current);
        return s;
      }

      previous_was_carriage_return = true;
      continue;
    }

    if ((1 == len) && (utf8char[0] == '\n') && (!m_uses_carriage_returns || previous_was_carriage_return))
      return s;

    if (previous_was_carriage_return) {
      setFilePointer(-len, seek_current);
      return s;
    }

    previous_was_carriage_return  = false;
    s                            += utf8char;
  }
}

byte_order_e
mm_text_io_c::get_byte_order() {
  return m_byte_order;
}

void
mm_text_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  mm_proxy_io_c::setFilePointer(((0 == offset) && (seek_beginning == mode)) ? m_bom_len : offset, mode);
}

/*
   Class for reading from stdin & writing to stdout.
*/

mm_stdio_c::mm_stdio_c() {
}

uint64
mm_stdio_c::getFilePointer() {
  return 0;
}

void
mm_stdio_c::setFilePointer(int64,
                           seek_mode) {
}

uint32
mm_stdio_c::_read(void *buffer,
                  size_t size) {
  return fread(buffer, 1, size, stdin);
}

#if !defined(SYS_WINDOWS)
size_t
mm_stdio_c::_write(const void *buffer,
                   size_t size) {
  m_cached_size = -1;

  return fwrite(buffer, 1, size, stdout);
}
#endif // defined(SYS_WINDOWS)

void
mm_stdio_c::close() {
}

void
mm_stdio_c::flush() {
  fflush(stdout);
}
