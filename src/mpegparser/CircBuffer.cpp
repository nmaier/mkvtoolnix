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

#include "common/common_pch.h"

#include "CircBuffer.h"
#include <string.h>
#include <cassert>

CircBuffer::CircBuffer(uint32_t size){
  m_buf = new binary[size];
  read_ptr = m_buf;
  write_ptr = m_buf;
  buf_capacity = size;
  bytes_in_buf = 0;
}

CircBuffer::~CircBuffer(){
  if(m_buf)
    delete [] m_buf;
}

int32_t CircBuffer::Skip(uint32_t numBytes){
  assert(numBytes > 0);
  if(can_read(numBytes)){
    unsigned int bbw = bytes_before_wrap_read(); //how many bytes we have before the buffer must be wrapped
    if (bbw >= numBytes) {
      move_pointer(&read_ptr, numBytes);
      bytes_in_buf -= numBytes;
    }else{
      if(bbw != 0){
        numBytes -= bbw;
        bytes_in_buf -= bbw;
        move_pointer(&read_ptr, bbw); //Reset to beginning
      }
      bytes_in_buf -= numBytes;
      move_pointer(&read_ptr, numBytes);
    }
    return 0;
  }else{
    return -1;
  }
}

int32_t CircBuffer::Read(binary* dest, uint32_t numBytes){
  if(can_read(numBytes)){
    unsigned int bbw = bytes_before_wrap_read(); //how many bytes we have before the buffer must be wrapped
    if (bbw >= numBytes) {
      memcpy(dest, read_ptr, numBytes);
      move_pointer(&read_ptr, numBytes);
      bytes_in_buf -= numBytes;
    }else{
      if(bbw != 0){
        memcpy(dest,read_ptr,bbw);
        numBytes -= bbw;
        bytes_in_buf -= bbw;
        dest += bbw;
        move_pointer(&read_ptr, bbw); //Reset to beginning
      }
      memcpy(dest, read_ptr,numBytes);
      bytes_in_buf -= numBytes;
      move_pointer(&read_ptr, numBytes);
    }
    return 0;
  }else{
    return -1;
  }
}

int32_t CircBuffer::Write(binary* data, uint32_t length){
  if(can_write(length)){
    unsigned int bbw = bytes_before_wrap_write();
    if (bbw >= length) {
      memcpy(write_ptr, data, length);
      move_pointer(&write_ptr, length);
      bytes_in_buf += length;
    }else{
      if(bbw != 0){
        memcpy(write_ptr,data,bbw);
        length -= bbw;
        bytes_in_buf += bbw;
        data += bbw;
        move_pointer(&write_ptr, bbw); //Reset to beginning
      }
      memcpy(write_ptr, data,length);
      bytes_in_buf += length;
      move_pointer(&write_ptr, length);
    }
    return 0;
  }else{
    return -1;
  }
}
