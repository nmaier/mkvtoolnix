/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief MP3 reader module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_mp3.h"
#include "p_mp3.h"

int mp3_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  unsigned char buf[16384];
  int pos, pos2, ptr;
  unsigned long header;
  mp3_header_t mp3header;

  if (size > 16384)
    size = 16384;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, size) != size)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }

  ptr = 0;
  while (1) {
    pos = find_mp3_header(&buf[ptr], size - ptr);
    if (pos < 0)
      return 0;
    decode_mp3_header(&buf[ptr + pos], &mp3header);
    if (!mp3header.is_tag)
      break;
    mxverb(2, "mp3_reader: Found tag at %d size %d\n", ptr + pos,
           mp3header.framesize);
    ptr += pos + mp3header.framesize;
  }

  pos2 = find_mp3_header(&buf[ptr + pos + mp3header.framesize + 4],
                         size - ptr - pos - 4 - mp3header.framesize);
  mxverb(2, "mp3_reader: Found first at %d, length %d, version %d, layer %d, "
         "second at %d\n", pos, mp3header.framesize + 4, mp3header.version,
         mp3header.layer, pos2);
         
  if (pos2 != 0)
    return 0;

  return 1;
}

mp3_reader_c::mp3_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int pos, this_size, ptr;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
    if (size > 16384)
      this_size = 16384;
    else
      this_size = size;
    chunk = (unsigned char *)safemalloc(this_size);
    if (mm_io->read(chunk, this_size) != this_size)
      throw error_c("mp3_reader: Could not read 4096 bytes.");
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("mp3_reader: Could not open the source file.");
  }
  ptr = 0;
  while (1) {
    pos = find_mp3_header(&chunk[ptr], this_size - ptr);
    if (pos < 0)
      throw error_c("mp3_reader: No valid MP3 packet found in the first "
                    "4096 bytes.\n");
    decode_mp3_header(&chunk[ptr + pos], &mp3header);
    if (!mp3header.is_tag)
      break;
    ptr += pos + mp3header.framesize;
  }

  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.
  mp3packetizer = new mp3_packetizer_c(this, mp3header.sampling_frequency,
                                       mp3header.channels, mp3header.layer,
                                       ti);
  if (verbose)
    mxinfo("Using MP2/MP3 demultiplexer for %s.\n+-> Using "
           "MPEG audio output module for audio stream.\n", ti->fname);
}

mp3_reader_c::~mp3_reader_c() {
  delete mm_io;
  if (chunk != NULL)
    safefree(chunk);
  if (mp3packetizer != NULL)
    delete mp3packetizer;
}

int mp3_reader_c::read(generic_packetizer_c *) {
  int nread;

  nread = mm_io->read(chunk, 4096);
  if (nread <= 0)
    return 0;

  mp3packetizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

int mp3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void mp3_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %lld/%lld bytes (100%%)\r", size, size);
  else
    mxinfo("progress: %lld/%lld bytes (%d%%)\r", bytes_processed, size,
           (int)(bytes_processed * 100L / size));
}

void mp3_reader_c::set_headers() {
  mp3packetizer->set_headers();
}

void mp3_reader_c::identify() {
  mxinfo("File '%s': container: MP2/MP3\nTrack ID 0: audio (MPEG-%s layer "
         "%d)\n", ti->fname,
         mp3header.version == 1 ? "1" : mp3header.version == 2 ? "2" : "2.5",
         mp3header.layer);
}
