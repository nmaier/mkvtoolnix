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

#include "common/os.h"

#include <errno.h>
#if HAVE_POSIX_FADVISE
# include <fcntl.h>
# include <map>
# include <sys/utsname.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(SYS_WINDOWS)
# include <windows.h>
# include <direct.h>
#else
# include <unistd.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif // SYS_WINDOWS

#include "common/common.h"
#include "common/endian.h"
#include "common/error.h"
#include "common/locale.h"
#include "common/mm_io.h"
#include "common/string_editing.h"
#include "common/string_parsing.h"

#if !defined(SYS_WINDOWS)

static std::string
get_errno_msg() {
  return to_utf8(cc_local_utf8, strerror(errno));
}

# if HAVE_POSIX_FADVISE
static const unsigned long read_using_willneed   = 16 * 1024 * 1024;
static const unsigned long write_before_dontneed = 8 * 1024 * 1024;
bool mm_file_io_c::use_posix_fadvise             = false;

bool
operator <(const file_id_t &file_id_1,
           const file_id_t &file_id_2) {
  return (file_id_1.m_dev < file_id_2.m_dev) || (file_id_1.m_ino < file_id_2.m_ino);
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
                           const open_mode mode):
  file_name(path) {

  std::string local_path;
  const char *cmode;
# if HAVE_POSIX_FADVISE
  int advise;

  advise = 0;
  read_count = 0;
  write_count = 0;
  use_posix_fadvise_here = use_posix_fadvise;
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
      throw mm_io_error_c(Y("Unknown open mode"));
  }

  if ((MODE_WRITE == mode) || (MODE_CREATE == mode))
    prepare_path(path);
  local_path = from_utf8(cc_local_utf8, path);

  struct stat st;
  if ((0 == stat(local_path.c_str(), &st)) && S_ISDIR(st.st_mode))
    throw mm_io_open_error_c();

  file = (FILE *)fopen(local_path.c_str(), cmode);

  if (file == NULL)
    throw mm_io_open_error_c();

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
    throw mm_io_open_error_c();

  m_file_id.initialize(st);

  if (!map_has_key(s_fadvised_file_count_by_id, m_file_id))
    s_fadvised_file_count_by_id[m_file_id] = 0;

  s_fadvised_file_count_by_id[m_file_id]++;

  if (1 < s_fadvised_file_count_by_id[m_file_id]) {
    use_posix_fadvise_here = false;
    if (map_has_key(s_fadvised_file_object_by_id, m_file_id))
      s_fadvised_file_object_by_id[m_file_id]->use_posix_fadvise_here = false;

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

  if (fseeko((FILE *)file, offset, whence) != 0)
    throw mm_io_seek_error_c();

  if (mode == seek_beginning)
    m_current_position = offset;
  else if (mode == seek_end)
    m_current_position = ftello((FILE *)file);
  else
    m_current_position += offset;
}

size_t
mm_file_io_c::write(const void *buffer,
                    size_t size) {
  size_t bwritten = fwrite(buffer, 1, size, (FILE *)file);
  if (ferror((FILE *)file) != 0)
    mxerror(boost::format(Y("Could not write to the output file: %1% (%2%)\n")) % errno % get_errno_msg());

# if HAVE_POSIX_FADVISE
  write_count += bwritten;
  if (use_posix_fadvise && use_posix_fadvise_here && (write_count > write_before_dontneed)) {
    uint64 pos  = getFilePointer();
    write_count = 0;

    if (0 != posix_fadvise(fileno((FILE *)file), 0, pos, POSIX_FADV_DONTNEED)) {
      mxverb(2, boost::format(FADVISE_WARNING) % file_name % errno % get_errno_msg());
      use_posix_fadvise_here = false;
    }
  }
# endif

  m_current_position += bwritten;
  cached_size         = -1;

  return bwritten;
}

uint32
mm_file_io_c::read(void *buffer,
                   size_t size) {
  int64_t bread = fread(buffer, 1, size, (FILE *)file);

# if HAVE_POSIX_FADVISE
  if (use_posix_fadvise && use_posix_fadvise_here && (0 <= bread)) {
    read_count += bread;
    if (read_count > read_using_willneed) {
      uint64 pos = getFilePointer();
      int fd     = fileno((FILE *)file);
      read_count = 0;

      if (0 != posix_fadvise(fd, 0, pos, POSIX_FADV_DONTNEED)) {
        mxverb(2, boost::format(FADVISE_WARNING) % file_name % errno % get_errno_msg());
        use_posix_fadvise_here = false;
      }

      if (use_posix_fadvise_here && (0 != posix_fadvise(fd, pos, pos + read_using_willneed, POSIX_FADV_WILLNEED))) {
        mxverb(2, boost::format(FADVISE_WARNING) % file_name % errno % get_errno_msg());
        use_posix_fadvise_here = false;
      }
    }
  }
# endif

  m_current_position += bread;

  return bread;
}

void
mm_file_io_c::close() {
  if (NULL != file) {
    fclose((FILE *)file);
    file = NULL;

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
  return feof((FILE *)file) != 0;
}

int
mm_file_io_c::truncate(int64_t pos) {
  cached_size = -1;
  return ftruncate(fileno((FILE *)file), pos);
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

  use_posix_fadvise = false;
  if ((0 == uname(&un)) && !strcasecmp(un.sysname, "Linux")) {
    std::vector<std::string> versions = split(un.release, ".");
    int major, minor;

    if ((2 <= versions.size()) && parse_int(versions[0], major) && parse_int(versions[1], minor) && ((2 < major) || (6 <= minor)))
      use_posix_fadvise = true;
  }
# endif // HAVE_POSIX_FADVISE
}

#else // SYS_WINDOWS

# include "os_windows.h"

HANDLE
CreateFileUtf8(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile);

mm_file_io_c::mm_file_io_c(const std::string &path,
                           const open_mode mode) {
  DWORD access_mode, share_mode, disposition;

  switch (mode) {
    case MODE_READ:
      access_mode = GENERIC_READ;
      share_mode  = FILE_SHARE_READ | FILE_SHARE_WRITE;
      disposition = OPEN_EXISTING;
      break;
    case MODE_WRITE:
      access_mode = GENERIC_WRITE | GENERIC_READ;
      share_mode  = FILE_SHARE_READ;
      disposition = OPEN_EXISTING;
      break;
    case MODE_SAFE:
      access_mode = GENERIC_WRITE | GENERIC_READ;
      share_mode  = FILE_SHARE_READ;
      disposition = OPEN_ALWAYS;
      break;
    case MODE_CREATE:
      access_mode = GENERIC_WRITE;
      share_mode  = FILE_SHARE_READ;
      disposition = CREATE_ALWAYS;
      break;
    default:
      throw mm_io_error_c(Y("Unknown open mode"));
  }

  if ((MODE_WRITE == mode) || (MODE_CREATE == mode))
    prepare_path(path);

  file = (void *)CreateFileUtf8(path.c_str(), access_mode, share_mode, NULL, disposition, 0, NULL);
  _eof = false;
  if ((HANDLE)file == (HANDLE)0xFFFFFFFF)
    throw mm_io_open_error_c();

  file_name          = path;
  dos_style_newlines = true;
}

void
mm_file_io_c::close() {
  if (NULL != file) {
    CloseHandle((HANDLE)file);
    file = NULL;
  }
  file_name = "";
}

uint64
mm_file_io_c::get_real_file_pointer() {
  LONG high = 0;
  DWORD low = SetFilePointer((HANDLE)file, 0, &high, FILE_CURRENT);

  if ((low == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    return (uint64)-1;

  return (((uint64)high) << 32) | (uint64)low;
}

void
mm_file_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  DWORD method = seek_beginning == mode ? FILE_BEGIN
               : seek_current   == mode ? FILE_CURRENT
               : seek_end       == mode ? FILE_END
               :                          FILE_BEGIN;
  LONG high    = (LONG)(offset >> 32);
  DWORD low    = SetFilePointer((HANDLE)file, (LONG)(offset & 0xffffffff), &high, method);

  if ((INVALID_SET_FILE_POINTER == low) && (GetLastError() != NO_ERROR))
    throw mm_io_seek_error_c();

  m_current_position = (int64_t)low + ((int64_t)high << 32);
}

uint32
mm_file_io_c::read(void *buffer,
                   size_t size) {
  DWORD bytes_read;

  if (!ReadFile((HANDLE)file, buffer, size, &bytes_read, NULL)) {
    _eof               = true;
    m_current_position = get_real_file_pointer();

    return 0;
  }

  if (size != bytes_read)
    _eof = true;

  m_current_position += bytes_read;

  return bytes_read;
}

size_t
mm_file_io_c::write(const void *buffer,
                    size_t size) {
  DWORD bytes_written;

  if (!WriteFile((HANDLE)file, buffer, size, &bytes_written, NULL))
    bytes_written = 0;

  if (bytes_written != size) {
    std::string error_msg_utf8;

    DWORD error     = GetLastError();
    char *error_msg = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&error_msg, 0, NULL);

    if (NULL != error_msg) {
      int idx = strlen(error_msg) - 1;

      while ((0 <= idx) && ((error_msg[idx] == '\n') || (error_msg[idx] == '\r'))) {
        error_msg[idx] = 0;
        idx--;
      }

      error_msg_utf8 = to_utf8(cc_local_utf8, error_msg);
    } else
      error_msg_utf8 = Y("unknown");

    mxerror(boost::format(Y("Could not write to the output file: %1% (%2%)\n")) % error % error_msg_utf8);

    if (NULL != error_msg)
      LocalFree(error_msg);
  }

  m_current_position += bytes_written;
  cached_size         = -1;

  return bytes_written;
}

bool
mm_file_io_c::eof() {
  return _eof;
}

int
mm_file_io_c::truncate(int64_t pos) {
  cached_size = -1;

  save_pos();
  if (setFilePointer2(pos)) {
    bool result = SetEndOfFile((HANDLE)file);
    restore_pos();

    return result ? 0 : -1;
  }

  restore_pos();

  return -1;
}

void
mm_file_io_c::setup() {
}

#endif // SYS_UNIX

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

    // Fint the host name
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

  for (int i = 0; parts.size() > i; ++i) {
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
  file_name = "";
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
    throw mm_io_eof_error_c();

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
  int i;
  std::string output;

  const char *cs = s.c_str();
  for (i = 0; cs[i] != 0; i++)
    if (cs[i] != '\r') {
#if defined(SYS_WINDOWS)
      if ('\n' == cs[i])
        output += "\r";
#endif
      output += cs[i];
    } else if ('\n' != cs[i + 1])
      output += "\r";

  return write(output.c_str(), output.length());
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
    throw error_c(Y("end-of-file"));

  return value;
}

uint16_t
mm_io_c::read_uint16_le() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw error_c(Y("end-of-file"));

  return get_uint16_le(buffer);
}

uint32_t
mm_io_c::read_uint24_le() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw error_c(Y("end-of-file"));

  return get_uint24_le(buffer);
}

uint32_t
mm_io_c::read_uint32_le() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw error_c(Y("end-of-file"));

  return get_uint32_le(buffer);
}

uint64_t
mm_io_c::read_uint64_le() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw error_c(Y("end-of-file"));

  return get_uint64_le(buffer);
}

uint16_t
mm_io_c::read_uint16_be() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw error_c(Y("end-of-file"));

  return get_uint16_be(buffer);
}

uint32_t
mm_io_c::read_uint24_be() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw error_c(Y("end-of-file"));

  return get_uint24_be(buffer);
}

uint32_t
mm_io_c::read_uint32_be() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw error_c(Y("end-of-file"));

  return get_uint32_be(buffer);
}

uint64_t
mm_io_c::read_uint64_be() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw error_c(Y("end-of-file"));

  return get_uint64_be(buffer);
}

uint32_t
mm_io_c::read(memory_cptr &buffer,
              size_t size,
              int offset) {
  if (-1 == offset)
    offset = buffer->get_size();

  if (buffer->get_size() <= (size + offset))
    buffer->resize(size + offset);

  if (read(buffer->get() + offset, size) != size)
    throw mm_io_eof_error_c();

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

uint32_t
mm_io_c::write(const memory_cptr &buffer,
               int size,
               int offset) {
  if (-1 == size)
    size = buffer->get_size();
  if (write(buffer->get() + offset, size) != size)
    throw mm_io_eof_error_c();
  return size;
}

void
mm_io_c::skip(int64 num_bytes) {
  int64_t pos;

  pos = getFilePointer();
  setFilePointer(pos + num_bytes);
  if ((pos + num_bytes) != getFilePointer())
    throw error_c(Y("end-of-file"));
}

void
mm_io_c::save_pos(int64_t new_pos) {
  positions.push(getFilePointer());

  if (-1 != new_pos)
    setFilePointer(new_pos);
}

bool
mm_io_c::restore_pos() {
  if (positions.empty())
    return false;

  setFilePointer(positions.top());
  positions.pop();

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
  int bom_len;

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

  return (write(bom, bom_len) == bom_len);
}

int64_t
mm_io_c::get_size() {
  if (-1 == cached_size) {
    save_pos();
    setFilePointer(0, seek_end);
    cached_size = getFilePointer();
    restore_pos();
  }

  return cached_size;
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
  if ((NULL != proxy_io) && proxy_delete_io)
    delete proxy_io;

  proxy_io = NULL;
}

/*
   Dummy class for output to /dev/null. Needed for two pass stuff.
*/

mm_null_io_c::mm_null_io_c() {
  pos = 0;
}

uint64
mm_null_io_c::getFilePointer() {
  return pos;
}

void
mm_null_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  pos = seek_beginning == mode ? offset
      : seek_end       == mode ? 0
      :                          pos + offset;
}

uint32
mm_null_io_c::read(void *buffer,
                   size_t size) {
  memset(buffer, 0, size);
  pos += size;

  return size;
}

size_t
mm_null_io_c::write(const void *buffer,
                    size_t size) {
  pos += size;

  return size;
}

void
mm_null_io_c::close() {
}

/*
   IO callback class working on memory
*/
mm_mem_io_c::mm_mem_io_c(unsigned char *_mem,
                         uint64_t _mem_size,
                         int _increase)
  : pos(0)
  , mem_size(_mem_size)
  , allocated(_mem_size)
  , increase(_increase)
  , mem(_mem)
  , ro_mem(NULL)
  , read_only(false)
{
  if (0 >= increase)
    throw error_c(Y("wrong usage: increase < 0"));

  if ((NULL == mem) && (0 < increase)) {
    if (0 == mem_size)
      allocated = increase;

    mem      = (unsigned char *)safemalloc(allocated);
    free_mem = true;

  } else
    free_mem = false;
}

mm_mem_io_c::mm_mem_io_c(const unsigned char *_mem,
                         uint64_t _mem_size)
  : pos(0)
  , mem_size(_mem_size)
  , allocated(_mem_size)
  , increase(0)
  , mem(NULL)
  , ro_mem(_mem)
  , free_mem(false)
  , read_only(true)
{
  if (NULL == ro_mem)
    throw error_c(Y("wrong usage: read-only with NULL memory"));
}

mm_mem_io_c::~mm_mem_io_c() {
  close();
}

uint64_t
mm_mem_io_c::getFilePointer() {
  return pos;
}

void
mm_mem_io_c::setFilePointer(int64 offset,
                            seek_mode mode) {
  if ((NULL == mem) && (NULL == ro_mem) && (0 == mem_size))
    throw error_c(Y("wrong usage: read-only with NULL memory"));

  int64_t new_pos
    = seek_beginning == mode ? offset
    : seek_end       == mode ? mem_size - offset
    :                          pos      + offset;

  if ((0 <= new_pos) && (mem_size >= new_pos))
    pos = new_pos;
  else
    throw mm_io_seek_error_c();
}

uint32
mm_mem_io_c::read(void *buffer,
                  size_t size) {
  int64_t rbytes = (pos + size) >= mem_size ? mem_size - pos : size;
  if (read_only)
    memcpy(buffer, &ro_mem[pos], rbytes);
  else
    memcpy(buffer, &mem[pos], rbytes);
  pos += rbytes;

  return rbytes;
}

size_t
mm_mem_io_c::write(const void *buffer,
                   size_t size) {
  if (read_only)
    throw error_c(Y("wrong usage: writing to read-only memory"));

  int64_t wbytes;
  if ((pos + size) >= allocated) {
    if (increase) {
      int64_t new_allocated  = pos + size - allocated;
      new_allocated          = ((new_allocated / increase) + 1 ) * increase;
      allocated             += new_allocated;
      mem                    = (unsigned char *)saferealloc(mem, allocated);
      wbytes                 = size;
    } else
      wbytes = allocated - pos;

  } else
    wbytes = size;

  if ((pos + size) > mem_size)
    mem_size = pos + size;

  memcpy(&mem[pos], buffer, wbytes);
  pos         += wbytes;
  cached_size  = -1;

  return wbytes;
}

void
mm_mem_io_c::close() {
  if (free_mem)
    safefree(mem);
  mem       = NULL;
  ro_mem    = NULL;
  read_only = true;
  free_mem  = false;
  mem_size  = 0;
  increase  = 0;
  pos       = 0;
}

bool
mm_mem_io_c::eof() {
  return pos >= mem_size;
}

unsigned char *
mm_mem_io_c::get_and_lock_buffer() {
  free_mem = false;
  return mem;
}

/*
   Class for handling UTF-8/UTF-16/UTF-32 text files.
*/

mm_text_io_c::mm_text_io_c(mm_io_c *_in,
                           bool _delete_in)
  : mm_proxy_io_c(_in, _delete_in)
  , byte_order(BO_NONE)
  , bom_len(0) {

  _in->setFilePointer(0, seek_beginning);

  unsigned char buffer[4];
  int num_read = _in->read(buffer, 4);
  if (2 > num_read) {
    _in->setFilePointer(0, seek_beginning);
    return;
  }

  if ((3 <= num_read) && (buffer[0] == 0xef) && (buffer[1] == 0xbb) && (buffer[2] == 0xbf)) {
    byte_order = BO_UTF8;
    bom_len    = 3;
  } else if ((4 <= num_read) && (buffer[0] == 0xff) && (buffer[1] == 0xfe) && (buffer[2] == 0x00) && (buffer[3] == 0x00)) {
    byte_order = BO_UTF32_LE;
    bom_len    = 4;
  } else if ((4 <= num_read) && (buffer[0] == 0x00) && (buffer[1] == 0x00) && (buffer[2] == 0xfe) && (buffer[3] == 0xff)) {
    byte_order = BO_UTF32_BE;
    bom_len    = 4;
  } else if ((2 <= num_read) && (buffer[0] == 0xff) && (buffer[1] == 0xfe)) {
    byte_order = BO_UTF16_LE;
    bom_len    = 2;
  } else if ((2 <= num_read) && (buffer[0] == 0xfe) && (buffer[1] == 0xff)) {
    byte_order = BO_UTF16_BE;
    bom_len    = 2;
  }

  _in->setFilePointer(bom_len, seek_beginning);
}

// 1 byte: 0xxxxxxx,
// 2 bytes: 110xxxxx 10xxxxxx,
// 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx

int
mm_text_io_c::read_next_char(char *buffer) {
  unsigned char stream[6];
  int i;

  if (byte_order == BO_NONE)
    return read(buffer, 1);

  int size = 0;
  if (byte_order == BO_UTF8) {
    if (read(stream, 1) != 1)
      return 0;

    size = ((stream[0] & 0x80) == 0x00) ?  1
         : ((stream[0] & 0xe0) == 0xc0) ?  2
         : ((stream[0] & 0xf0) == 0xe0) ?  3
         : ((stream[0] & 0xf8) == 0xf0) ?  4
         : ((stream[0] & 0xfc) == 0xf8) ?  5
         : ((stream[0] & 0xfe) == 0xfc) ?  6
         :                                -1;

    if (-1 == size)
      mxerror(boost::format(Y("mm_text_io_c::read_next_char(): Invalid UTF-8 char. First byte: 0x%|1$02x|")) % (unsigned int)stream[0]);

    if ((1 < size) && (read(&stream[1], size - 1) != (size - 1)))
      return 0;

    memcpy(buffer, stream, size);

    return size;

  } else if ((byte_order == BO_UTF16_LE) || (byte_order == BO_UTF16_BE))
    size = 2;
  else
    size = 4;

  if (read(stream, size) != size)
    return 0;

  unsigned long data = 0;
  if ((byte_order == BO_UTF16_LE) || (byte_order == BO_UTF32_LE))
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
  std::string s;
  char utf8char[8];

  if (eof())
    throw error_c(Y("end-of-file"));

  while (1) {
    memset(utf8char, 0, 8);

    int len = read_next_char(utf8char);
    if (0 == len)
      return s;

    if ((1 == len) && (utf8char[0] == '\r'))
      continue;

    if ((1 == len) && (utf8char[0] == '\n'))
      return s;

    s += utf8char;
  }
}

byte_order_e
mm_text_io_c::get_byte_order() {
  return byte_order;
}

void
mm_text_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  mm_proxy_io_c::setFilePointer(((0 == offset) && (seek_beginning == mode)) ? bom_len : offset, seek_beginning);
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
mm_stdio_c::read(void *buffer,
                 size_t size) {
  return fread(buffer, 1, size, stdin);
}

size_t
mm_stdio_c::write(const void *buffer,
                  size_t size) {
#if defined(SYS_WINDOWS)
  int i;
  int bytes_written = 0;
  const char *s     = (const char *)buffer;
  for (i = 0; i < size; ++i)
    if (('\r' != s[i]) || ((i + 1) == size) || ('\n' != s[i + 1]))
      bytes_written += fwrite(&s[i], 1, 1, stdout);

  fflush(stdout);

  cached_size = -1;

  return bytes_written;

#else  // defined(SYS_WINDOWS)

  cached_size = -1;

  return fwrite(buffer, 1, size, stdout);
#endif // defined(SYS_WINDOWS)
}

void
mm_stdio_c::close() {
}

void
mm_stdio_c::flush() {
  fflush(stdout);
}
