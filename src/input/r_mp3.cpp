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

int
mp3_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t) {
  return (find_valid_headers(mm_io) != -1) ? 1 : 0;
}

mp3_reader_c::mp3_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  int pos;
  unsigned char buf[16384];

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    size = mm_io->get_size();

    pos = find_valid_headers(mm_io);
    if (pos < 0)
      throw error_c("Could not find a valid MP3 packet.");
    mm_io->setFilePointer(pos, seek_beginning);
    mm_io->read(buf, 16384);
    decode_mp3_header(buf, &mp3header);
    mm_io->setFilePointer(pos, seek_beginning);
    if ((pos > 0) && verbose)
      mxwarn("mp3_reader: skipping %d bytes at the beginning of '%s' (no "
                 "valid MP3 header found).\n", pos, ti->fname);
    mxverb(2, "mp3_reader: Found header at %d size %d\n", pos,
           mp3header.framesize);

    bytes_processed = 0;
    ti->id = 0;                 // ID for this track.
    if (verbose)
      mxinfo(FMT_FN "Using the MP2/MP3 demultiplexer.\n", ti->fname);
  } catch (exception &ex) {
    throw error_c("mp3_reader: Could not open the source file.");
  }
}

mp3_reader_c::~mp3_reader_c() {
  delete mm_io;
}

void
mp3_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new mp3_packetizer_c(this, mp3header.sampling_frequency,
                                      mp3header.channels, ti));
  mxinfo(FMT_TID "Using the MPEG audio output module.\n", ti->fname,
         (int64_t)0);
}

int
mp3_reader_c::read(generic_packetizer_c *) {
  int nread;

  nread = mm_io->read(chunk, 16384);
  if (nread <= 0) {
    PTZR0->flush();
    return 0;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return EMOREDATA;
}

int
mp3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void
mp3_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %lld/%lld bytes (100%%)\r", size, size);
  else
    mxinfo("progress: %lld/%lld bytes (%d%%)\r", bytes_processed, size,
           (int)(bytes_processed * 100L / size));
}

void
mp3_reader_c::identify() {
  mxinfo("File '%s': container: MP2/MP3\nTrack ID 0: audio (MPEG-%s layer "
         "%d)\n", ti->fname,
         mp3header.version == 1 ? "1" : mp3header.version == 2 ? "2" : "2.5",
         mp3header.layer);
}

int
mp3_reader_c::find_valid_headers(mm_io_c *mm_io) {
  unsigned char buf[16384];
  int pos, pos2, ptr, buf_size;
  mp3_header_t mp3header;

  ptr = 0;

  try {
    mm_io->setFilePointer(0, seek_beginning);

    while (1) {
      mm_io->setFilePointer(ptr, seek_beginning);
      buf_size = mm_io->read(buf, 16384);
      pos = find_mp3_header(buf, buf_size);
      if (pos < 0)
        return -1;
      decode_mp3_header(&buf[pos], &mp3header);
      if (!mp3header.is_tag)
        break;
      mxverb(2, "mp3_reader: Found tag at %d size %d\n", ptr + pos,
             mp3header.framesize);
      ptr += pos + mp3header.framesize;
    }

    pos2 = find_mp3_header(&buf[pos + mp3header.framesize],
                           buf_size - pos - mp3header.framesize);
    mxverb(2, "mp3_reader: Found first at %d, length %d, version %d, "
           "layer %d, second at %d\n", ptr + pos, mp3header.framesize,
           mp3header.version, mp3header.layer, pos2);
         
    if (pos2 != 0) {
      int offset;

      // Next try... maybe we've found a bogus header in the trash.
      offset = pos + 1;
      pos = find_mp3_header(&buf[offset], buf_size - offset);
      if (pos < 0)
        return -1;
      decode_mp3_header(&buf[offset + pos], &mp3header);
      mxverb(2, "mp3_reader: Second header search, first header at %d + %d "
             "(version: %d, layer: %d, sampling freq: %d, channels: %d, "
             "bitrate: %d)\n", offset, pos, mp3header.version, mp3header.layer,
             mp3header.sampling_frequency, mp3header.channels,
             mp3header.bitrate);
      pos2 = find_mp3_header(&buf[offset + pos + mp3header.framesize],
                             buf_size - offset - pos - mp3header.framesize);
      mxverb(2, "mp3_reader: Second header search, second header? %d\n", pos2);
      if (pos2 < 0)
        return -1;
      decode_mp3_header(&buf[offset + pos + mp3header.framesize + pos2],
                        &mp3header);
      mxverb(2, "mp3_reader: Second header search, second header at %d + %d "
             "(version: %d, layer: %d, sampling freq: %d, channels: %d, "
             "bitrate: %d)\n", offset + pos, pos2, mp3header.version,
             mp3header.layer, mp3header.sampling_frequency, mp3header.channels,
             mp3header.bitrate);
      if (pos2 > 0)
        return -1;
      pos += offset;
    }
  } catch (exception &ex) {
    return -1;
  }

  return ptr + pos;
}
