/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * IO callback class implementation
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

#include <exception>

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(SYS_WINDOWS)
#include <windows.h>
#else
#include <unistd.h>
#endif // SYS_WINDOWS

#include "mm_io.h"
#include "common.h"

using namespace std;

#if !defined(SYS_WINDOWS)
mm_file_io_c::mm_file_io_c(const char *path,
                           const open_mode mode) {
  char *cmode;

  switch (mode) {
    case MODE_READ:
      cmode = "rb";
      break;
    case MODE_WRITE:
      cmode = "wb";
      break;
    case MODE_CREATE:
      cmode = "wb+";
      break;
    case MODE_SAFE:
      cmode = "r+b";
      break;
    default:
      throw 0;
  }

  file = (FILE *)fopen(path, cmode);

  if (file == NULL)
    throw exception();

  file_name = safestrdup(path);
  dos_style_newlines = false;
}

uint64
mm_file_io_c::getFilePointer() {
  return ftello((FILE *)file);
}

void
mm_file_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  int whence;

  if (mode == seek_beginning)
    whence = SEEK_SET;
  else if (mode == seek_end)
    whence = SEEK_END;
  else
    whence = SEEK_CUR;

  if (fseeko((FILE *)file, offset, whence) != 0)
    throw exception();
}

size_t
mm_file_io_c::write(const void *buffer,
                    size_t size) {
  size_t bwritten;

  bwritten = fwrite(buffer, 1, size, (FILE *)file);
  if (ferror((FILE *)file) != 0)
    mxerror("Cound not write to the output file: %d (%s)\n", errno,
            strerror(errno));

  return bwritten;
}

uint32
mm_file_io_c::read(void *buffer,
                   size_t size) {
  return fread(buffer, 1, size, (FILE *)file);
}

void
mm_file_io_c::close() {
  if (file != NULL)
    fclose((FILE *)file);
}

bool
mm_file_io_c::eof() {
  return feof((FILE *)file) != 0 ? true : false;
}

int
mm_file_io_c::truncate(int64_t pos) {
  return ftruncate(fileno((FILE *)file), pos);
}

#else // SYS_UNIX

mm_file_io_c::mm_file_io_c(const char *path,
                           const open_mode mode) {
  DWORD access_mode, share_mode, disposition;

  switch (mode) {
    case MODE_READ:
      access_mode = GENERIC_READ;
      share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
      disposition = OPEN_EXISTING;
      break;
    case MODE_WRITE:
      access_mode = GENERIC_WRITE;
      share_mode = 0;
      disposition = OPEN_ALWAYS;
      break;
    case MODE_SAFE:
      access_mode = GENERIC_WRITE | GENERIC_READ;
      share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
      disposition = OPEN_ALWAYS;
      break;
    case MODE_CREATE:
      access_mode = GENERIC_WRITE;
      share_mode = 0;
      disposition = CREATE_ALWAYS;
      break;
    default:
      throw exception();
  }

  file = (void *)CreateFile(path, access_mode, share_mode, NULL, disposition,
                            0, NULL);
  _eof = false;
  if ((HANDLE)file == (HANDLE)0xFFFFFFFF)
    throw exception();

  file_name = safestrdup(path);
  dos_style_newlines = true;
}

void
mm_file_io_c::close() {
  if (file != NULL)
    CloseHandle((HANDLE)file);
}

uint64
mm_file_io_c::getFilePointer() {
  LONG high = 0;
  DWORD low;
  
  low = SetFilePointer((HANDLE)file, 0, &high, FILE_CURRENT);
  if ((low == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    return (uint64)-1;

  return (((uint64)high) << 32) | (uint64)low;
}

void
mm_file_io_c::setFilePointer(int64 offset,
                             seek_mode mode) {
  DWORD method;
  LONG high;

  switch (mode) {
    case seek_beginning:
      method = FILE_BEGIN;
      break;
    case seek_current:
      method = FILE_CURRENT;
      break;
    case seek_end:
      method = FILE_END;
      break;
  }

  high = (LONG)(offset >> 32);
  SetFilePointer((HANDLE)file, (LONG)(offset & 0xffffffff), &high, method);
}

uint32
mm_file_io_c::read(void *buffer,
                   size_t size) {
  DWORD bytes_read;

  if (!ReadFile((HANDLE)file, buffer, size, &bytes_read, NULL)) {
    _eof = true;
    return 0;
  }

  if (size != bytes_read)
    _eof = true;

  return bytes_read;
}

size_t
mm_file_io_c::write(const void *buffer,
                    size_t size) {
  DWORD bytes_written;

  if (!WriteFile((HANDLE)file, buffer, size, &bytes_written, NULL))
    bytes_written = 0;

  if (bytes_written != size) {
    DWORD error;
    char *error_msg;

    error = GetLastError();
    error_msg = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                  error,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&error_msg, 0, NULL);
    if (error_msg != NULL) {
      int idx;
      idx = strlen(error_msg);
      idx--;
      while ((idx >= 0) &&
             ((error_msg[idx] == '\n') || (error_msg[idx] == '\r'))) {
        error_msg[idx] = 0;
        idx--;
      }
    }
    mxerror("Cound not write to the output file: %d (%s)\n", (int)error,
            error_msg != NULL ? error_msg : "unknown");
    if (error_msg != NULL)
      LocalFree(error_msg);
  }

  return bytes_written;
}

bool
mm_file_io_c::eof() {
  return _eof;
}

int
mm_file_io_c::truncate(int64_t pos) {
  bool result;

  save_pos();
  if (setFilePointer2(pos)) {
    result = SetEndOfFile((HANDLE)file);
    restore_pos();
    if (result)
      return 0;
    return -1;
  }
  restore_pos();
  return -1;
}

#endif // SYS_UNIX

// mm_file_io_c::mm_file_io_c():
//   mm_file_io() {
//   file_name = NULL;
//   file = NULL;
// }

mm_file_io_c::~mm_file_io_c() {
  close();
  safefree(file_name);
}

const char *
mm_file_io_c::get_file_name() const {
  return file_name;
}


/*
 * Abstract base class.
 */

string
mm_io_c::getline() {
  char c;
  string s;

  if (eof())
    throw exception();

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
mm_io_c::getline2(string &s) {
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
mm_io_c::puts_unl(const char *s) {
  int i;
  size_t bytes_written;

  bytes_written = 0;
  for (i = 0; i < strlen(s); i++)
    if (s[i] != '\r')
      bytes_written += write(&s[i], 1);

  return bytes_written;
}

uint32_t
mm_io_c::read(string &buffer,
              size_t size) {
  char *cbuffer = new char[size + 1];
  int nread;

  nread = read(buffer, size);
  if (nread < 0)
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
    throw exception();

  return value;
}

uint16_t
mm_io_c::read_uint16() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw exception();

  return get_uint16(buffer);
}

uint32_t
mm_io_c::read_uint24() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw exception();

  return get_uint24(buffer);
}

uint32_t
mm_io_c::read_uint32() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw exception();

  return get_uint32(buffer);
}

uint64_t
mm_io_c::read_uint64() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw exception();

  return get_uint64(buffer);
}

uint16_t
mm_io_c::read_uint16_be() {
  unsigned char buffer[2];

  if (read(buffer, 2) != 2)
    throw exception();

  return get_uint16_be(buffer);
}

uint32_t
mm_io_c::read_uint24_be() {
  unsigned char buffer[3];

  if (read(buffer, 3) != 3)
    throw exception();

  return get_uint24_be(buffer);
}

uint32_t
mm_io_c::read_uint32_be() {
  unsigned char buffer[4];

  if (read(buffer, 4) != 4)
    throw exception();

  return get_uint32_be(buffer);
}

uint64_t
mm_io_c::read_uint64_be() {
  unsigned char buffer[8];

  if (read(buffer, 8) != 8)
    throw exception();

  return get_uint64_be(buffer);
}

int
mm_io_c::write_uint8(unsigned char value) {
  return write(&value, 1);
}

int
mm_io_c::write_uint16(uint16_t value) {
  uint16_t buffer;

  put_uint16(&buffer, value);
  return write(&buffer, sizeof(uint16_t));
}

int
mm_io_c::write_uint32(uint32_t value) {
  uint32_t buffer;

  put_uint32(&buffer, value);
  return write(&buffer, sizeof(uint32_t));
}

int
mm_io_c::write_uint64(uint64_t value) {
  uint64_t buffer;

  put_uint64(&buffer, value);
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

void
mm_io_c::skip(int64 num_bytes) {
  int64_t pos;

  pos = getFilePointer();
  setFilePointer(pos + num_bytes);
  if ((pos + num_bytes) != getFilePointer())
    throw exception();
}

void
mm_io_c::save_pos(int64_t new_pos) {
  positions.push(getFilePointer());

  if (new_pos != -1)
    setFilePointer(new_pos);
}

bool
mm_io_c::restore_pos() {
  if (positions.size() == 0)
    return false;

  setFilePointer(positions.top());
  positions.pop();

  return true;
}

bool
mm_io_c::write_bom(const char *charset) {
  const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
  const unsigned char utf16le_bom[2] = {0xff, 0xfe};
  const unsigned char utf16be_bom[2] = {0xfe, 0xff};
  const unsigned char utf32le_bom[4] = {0xff, 0xfe, 0x00, 0x00};
  const unsigned char utf32be_bom[4] = {0x00, 0x00, 0xff, 0xfe};
  const unsigned char *bom;
  int bom_len;

  if (charset == NULL)
    return false;

  if (!strcmp(charset, "UTF-8") || !strcmp(charset, "UTF8")) {
    bom_len = 3;
    bom = utf8_bom;
  } else if (!strcmp(charset, "UTF-16") || !strcmp(charset, "UTF-16LE") ||
             !strcmp(charset, "UTF16") || !strcmp(charset, "UTF16LE")) {
    bom_len = 2;
    bom = utf16le_bom;
  } else if (!strcmp(charset, "UTF-16BE") || !strcmp(charset, "UTF16BE")) {
    bom_len = 2;
    bom = utf16be_bom;
  } else if (!strcmp(charset, "UTF-32") || !strcmp(charset, "UTF-32LE") ||
             !strcmp(charset, "UTF32") || !strcmp(charset, "UTF32LE")) {
    bom_len = 4;
    bom = utf32le_bom;
  } else if (!strcmp(charset, "UTF-32BE") || !strcmp(charset, "UTF32BE")) {
    bom_len = 4;
    bom = utf32be_bom;
  } else
    return false;

  return (write(bom, bom_len) == bom_len);
}

int64_t
mm_io_c::get_size() {
  int64_t size;

  save_pos();
  setFilePointer(0, seek_end);
  size = getFilePointer();
  restore_pos();

  return size;
}

int
mm_io_c::getch() {
  unsigned char c;

  if (read(&c, 1) != 1)
    return -1;

  return c;
}

int
mm_io_c::printf(const char *fmt,
                ...) {
  va_list ap;
  string new_fmt, s;
  char *new_string;
  int len, pos;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  len = get_varg_len(new_fmt.c_str(), ap);
  new_string = (char *)safemalloc(len + 1);
  vsprintf(new_string, new_fmt.c_str(), ap);
  va_end(ap);
  s = new_string;
  safefree(new_string);
  if (dos_style_newlines) {
    pos = 0;
    while ((pos = s.find('\n', pos)) >= 0) {
      s.replace(pos, 1, "\r\n");
      pos += 2;
    }
  }

  return write(s.c_str(), s.length());
}

/*
 * Proxy class that does I/O on a mm_io_c handed over in the ctor.
 * Useful for e.g. doing text I/O on other I/Os (file, mem).
 */

void
mm_proxy_io_c::close() {
  if (proxy_io == NULL)
    return;
  if (proxy_delete_io)
    delete proxy_io;
  proxy_io = NULL;
}

/*
 * Dummy class for output to /dev/null. Needed for two pass stuff.
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
  if (mode == seek_beginning)
    pos = offset;
  else if (mode == seek_end)
    pos = 0;
  else
    pos += offset;
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
 * IO callback class working on memory
 */
mm_mem_io_c::mm_mem_io_c(unsigned char *nmem,
                         uint64_t nsize) {
  mem = nmem;
  mem_size = nsize;
  pos = 0;
}

uint64_t
mm_mem_io_c::getFilePointer() {
  return pos;
}

void
mm_mem_io_c::setFilePointer(int64 offset,
                            seek_mode mode) {
  int64_t npos;

  if ((mem == NULL) || (mem_size == 0))
    throw exception();

  if (mode == seek_beginning)
    npos = offset;
  else if (mode == seek_end)
    npos = mem_size - offset;
  else
    npos = pos + offset;

  if (npos < 0)
    pos = 0;
  else if (npos >= mem_size)
    pos = mem_size;
  else
    pos = npos;
}

uint32
mm_mem_io_c::read(void *buffer,
                  size_t size) {
  int64_t rbytes;

  rbytes = (pos + size) >= mem_size ? mem_size - pos : size;
  memcpy(buffer, &mem[pos], rbytes);
  pos += rbytes;

  return rbytes;
}

size_t
mm_mem_io_c::write(const void *buffer,
                   size_t size) {
  int64_t wbytes;

  wbytes = (pos + size) >= mem_size ? mem_size - pos : size;
  memcpy(&mem[pos], buffer, wbytes);
  pos += wbytes;

  return wbytes;
}

void
mm_mem_io_c::close() {
  mem = NULL;
  mem_size = 0;
  pos = 0;
}

bool
mm_mem_io_c::eof() {
  return pos >= mem_size;
}

/*
 * Class for handling UTF-8/UTF-16/UTF-32 text files.
 */

mm_text_io_c::mm_text_io_c(mm_io_c *_in,
                           bool _delete_in):
  mm_proxy_io_c(_in, _delete_in) {
  unsigned char buffer[4];

  _in->setFilePointer(0, seek_beginning);
  if (_in->read(buffer, 4) != 4) {
    _in->close();
    if (_delete_in)
      delete _in;
    throw exception();
  }

  if ((buffer[0] == 0xef) && (buffer[1] == 0xbb) && (buffer[2] == 0xbf)) {
    byte_order = BO_UTF8;
    bom_len = 3;
  } else if ((buffer[0] == 0xff) && (buffer[1] == 0xfe) &&
             (buffer[2] == 0x00) && (buffer[3] == 0x00)) {
    byte_order = BO_UTF32_LE;
    bom_len = 4;
  } else if ((buffer[0] == 0x00) && (buffer[1] == 0x00) &&
             (buffer[2] == 0xfe) && (buffer[3] == 0xff)) {
    byte_order = BO_UTF32_BE;
    bom_len = 4;
  } else if ((buffer[0] == 0xff) && (buffer[1] == 0xfe)) {
    byte_order = BO_UTF16_LE;
    bom_len = 2;
  } else if ((buffer[0] == 0xfe) && (buffer[1] == 0xff)) {
    byte_order = BO_UTF16_BE;
    bom_len = 2;
  } else {
    byte_order = BO_NONE;
    bom_len = 0;
  }

  _in->setFilePointer(bom_len, seek_beginning);
}

// 1 byte: 0xxxxxxx,
// 2 bytes: 110xxxxx 10xxxxxx,
// 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx

int
mm_text_io_c::read_next_char(char *buffer) {
  unsigned char stream[6];
  unsigned long data;
  int size, i;

  if (byte_order == BO_NONE)
    return read(buffer, 1);

  size = 0;
  if (byte_order == BO_UTF8) {
    if (read(stream, 1) != 1)
      return 0;
    if ((stream[0] & 0x80) == 0)
      size = 1;
    else if ((stream[0] & 0xe0) == 0xc0)
      size = 2;
    else if ((stream[0] & 0xf0) == 0xe0)
      size = 3;
    else if ((stream[0] & 0xf8) == 0xf0)
      size = 4;
    else if ((stream[0] & 0xfc) == 0xf8)
      size = 5;
    else if ((stream[0] & 0xfe) == 0xfc)
      size = 6;
    else
      die("mm_text_io_c::read_next_char(): Invalid UTF-8 char. First byte: "
          "0x%02x", stream[0]);

    if ((size > 1) && (read(&stream[1], size - 1) != (size - 1)))
      return 0;

    memcpy(buffer, stream, size);

    return size;
  } else if ((byte_order == BO_UTF16_LE) || (byte_order == BO_UTF16_BE))
    size = 2;
  else
    size = 4;

  if (read(stream, size) != size)
    return 0;

  data = 0;
  if ((byte_order == BO_UTF16_LE) || (byte_order == BO_UTF32_LE))
    for (i = 0; i < size; i++) {
      data <<= 8;
      data |= stream[size - i - 1];
    }
  else
    for (i = 0; i < size; i++) {
      data <<= 8;
      data |= stream[i];
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
    buffer[0] = 0xe0 | (data >> 12);
    buffer[1] = 0x80 | ((data >> 6) & 0x3f);
    buffer[2] = 0x80 | (data & 0x3f);
    return 3;
  }

  die("mm_text_io_c: UTF32_* is not supported at the moment.");

  return 0;
}

string
mm_text_io_c::getline() {
  string s;
  int len;
  char utf8char[8];

  if (eof())
    throw exception();

  while (1) {
    memset(utf8char, 0, 8);

    len = read_next_char(utf8char);
    if (len == 0)
      return s;

    if ((len == 1) && (utf8char[0] == '\r'))
      continue;

    if ((len == 1) && (utf8char[0] == '\n'))
      return s;

    s += utf8char;
  }
}

byte_order_e
mm_text_io_c::get_byte_order() {
  return byte_order;
}

void
mm_text_io_c::setFilePointer(int64_t offset,
                             seek_mode mode) {
  if ((offset == 0) && (mode == seek_beginning))
    mm_proxy_io_c::setFilePointer(bom_len, seek_beginning);
  else
    mm_proxy_io_c::setFilePointer(offset, seek_beginning);
}

/*
 * Class for reading from stdin & writing to stdout.
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
  return fwrite(buffer, 1, size, stdout);
}

void
mm_stdio_c::close() {
}
