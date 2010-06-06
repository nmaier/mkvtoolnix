/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for ID3 tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/mm_io.h"

int
skip_id3v2_tag(mm_io_c &io) {
  unsigned char buffer[10];

  io.setFilePointer(0, seek_beginning);
  if (io.read(buffer, 10) != 10) {
    io.setFilePointer(0, seek_beginning);
    return -1;
  }

  if (strncmp((char *)buffer, "ID3", 3) ||
      (buffer[3] == 0xff) || (buffer[4] == 0xff) ||
      (buffer[6] >= 0x80) || (buffer[7] >= 0x80) ||
      (buffer[7] >= 0x80) || (buffer[8] >= 0x80)) {
    io.setFilePointer(0, seek_beginning);
    return 0;
  }

  size_t tag_size  = (buffer[6] << 21) | (buffer[7] << 14) | (buffer[8] << 7) | buffer[9];
  tag_size        += 10;        // tag header
  if ((buffer[5] & 0x10) != 0)
    tag_size += 10;             // footer present

  io.setFilePointer(tag_size, seek_beginning);
  if (io.getFilePointer() == tag_size)
    return tag_size;

  io.setFilePointer(0, seek_beginning);
  return -1;
}

int
id3v2_tag_present_at_end(mm_io_c &io) {
  unsigned char buffer[10];
  int tag_size;

  io.save_pos();
  io.setFilePointer(-10, seek_end);
  if (io.read(buffer, 10) != 10) {
    io.restore_pos();
    return 0;
  }

  if (strncmp((char *)buffer, "3DI", 3) ||
      (buffer[3] == 0xff) || (buffer[4] == 0xff) ||
      (buffer[6] >= 0x80) || (buffer[7] >= 0x80) ||
      (buffer[7] >= 0x80) || (buffer[8] >= 0x80)) {
    io.restore_pos();
    return 0;
  }

  tag_size = (buffer[6] << 21) | (buffer[7] << 14) | (buffer[8] << 7) |
    buffer[9];
  tag_size += 10;               // tag header
  tag_size += 10;               // tag footer

  io.restore_pos();

  return tag_size;
}

int
id3v1_tag_present_at_end(mm_io_c &io) {
  unsigned char buffer[3];

  if (io.get_size() < 128)
    return 0;
  io.save_pos();
  io.setFilePointer(-128, seek_end);
  if (io.read(buffer, 3) != 3) {
    io.restore_pos();
    return 0;
  }
  io.restore_pos();
  if (strncmp((char *)buffer, "TAG", 3) == 0)
    return 128;
  return 0;
}

int
id3_tag_present_at_end(mm_io_c &io) {
  if (id3v1_tag_present_at_end(io))
    return 128;
  return id3v2_tag_present_at_end(io);
}
