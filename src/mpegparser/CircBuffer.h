/*****************************************************************************

    Circular/Ring Buffer

    Copyright(C) 2004 John Cannon <spyder@matroska.org>

    This program is free software ; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation ; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY ; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program ; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

 **/

#ifndef __CIRC_BUFFER_H__

#define __CIRC_BUFFER_H__

#include "Types.h"

class CircBuffer {
private:
  binary *m_buf;
  binary *read_ptr;
  binary *write_ptr;


  inline uint32_t bytes_left() {
    return buf_capacity - bytes_in_buf;
  }

  inline uint32_t bytes_before_wrap_read() {
    int32_t a = (int32_t) ((m_buf + buf_capacity) - read_ptr);
    if(a < 0)
      return 0;
    else
      return a;
  }

  inline uint32_t bytes_before_wrap_write() {
    int32_t a = (int32_t)((m_buf + buf_capacity) - write_ptr);
    if(a < 0)
      return 0;
    else
      return a;
  }

  inline bool can_write(uint32_t numBytes){
    return (buf_capacity - bytes_in_buf) >= numBytes;
  }

  inline bool can_read(uint32_t numBytes){
    return (bytes_in_buf) >= numBytes;
  }

  inline void move_pointer(binary** ptr, uint32_t numBytes){
    *ptr += numBytes;
    if(((m_buf + buf_capacity) - *ptr) == 0){
      *ptr = m_buf;
      //printf("Buffer wrapped\n");
    }
  }

public:

  uint32_t buf_capacity;
  uint32_t bytes_in_buf;

  CircBuffer(uint32_t size);
  ~CircBuffer();

  const binary* GetReadPtr(){
    return read_ptr;
  }

  const binary* GetWritePtr(){
    return read_ptr;
  }

  const binary* GetBufPtr(){
    return read_ptr;
  }

  binary& operator[](unsigned int i){
    if(i > bytes_in_buf){
      return read_ptr[0];
    }
    uint32_t bbw = bytes_before_wrap_read();
    if(i < bbw)
      return read_ptr[i];
    else
      return m_buf[i - bbw];
  }

  int32_t Read(binary* dest, uint32_t numBytes);
  int32_t Skip(uint32_t numBytes);
  int32_t Write(binary* data, uint32_t numBytes);

  uint32_t GetLength(){
    return bytes_in_buf;
  }

};

#endif // __CIRC_BUFFER_H__

