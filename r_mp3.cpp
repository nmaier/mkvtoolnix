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
    \version \$Id: r_mp3.cpp,v 1.16 2003/05/05 21:55:02 mosu Exp $
    \brief MP3 reader module
    \author Moritz Bunkus         <moritz @ bunkus.org>
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

int mp3_reader_c::probe_file(FILE *file, int64_t size) { 
  unsigned char buf[4096];
  int pos;
  unsigned long header;
  mp3_header_t mp3header;
  
  if (size < 4096)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(buf, 1, 4096, file) != 4096) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  
  pos = find_mp3_header(buf, 4096, &header);
  if (pos < 0)
    return 0;
  decode_mp3_header(header, &mp3header);
  if ((4 - ((header >> 17) & 3)) != 3)
    return 0;
  pos = find_mp3_header(&buf[pos + mp3header.framesize + 4], 4096 - pos - 4 -
                        mp3header.framesize, &header);
  if (pos < 0)
    return 0;
  if ((4 - ((header >> 17) & 3)) != 3)
    return 0;
  
  return 1;    
}

mp3_reader_c::mp3_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int pos;
  unsigned long header;
  mp3_header_t mp3header;

  if ((file = fopen(ti->fname, "r")) == NULL)
    throw error_c("mp3_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("mp3_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("mp3_reader: Could not seek to beginning of file.");
  chunk = (unsigned char *)safemalloc(4096);
  if (fread(chunk, 1, 4096, file) != 4096)
    throw error_c("mp3_reader: Could not read 4096 bytes.");
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("mp3_reader: Could not seek to beginning of file.");
  pos = find_mp3_header(chunk, 4096, &header);
  if (pos < 0)
    throw error_c("mp3_reader: No valid MP3 packet found in the first " \
                  "4096 bytes.\n");
  decode_mp3_header(header, &mp3header);

  bytes_processed = 0;
  mp3packetizer = new mp3_packetizer_c(this,
                                       mp3_freqs[mp3header.sampling_frequency],
                                       mp3header.stereo ? 2 : 1, ti);
  if (verbose)
    fprintf(stdout, "Using MP3 demultiplexer for %s.\n+-> Using " \
            "MP3 output module for audio stream.\n", ti->fname);
}

mp3_reader_c::~mp3_reader_c() {
  if (file != NULL)
    fclose(file);
  if (chunk != NULL)
    safefree(chunk);
  if (mp3packetizer != NULL)
    delete mp3packetizer;
}

int mp3_reader_c::read() {
  int nread;
  
  do {
    if (mp3packetizer->packet_available())
      return EMOREDATA;

    nread = fread(chunk, 1, 4096, file);
    if (nread <= 0)
      return 0;

    mp3packetizer->process(chunk, nread);
    bytes_processed += nread;

  } while (1);
}

packet_t *mp3_reader_c::get_packet() {
  return mp3packetizer->get_packet();
}

int mp3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void mp3_reader_c::display_progress() {
  fprintf(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void mp3_reader_c::set_headers() {
  mp3packetizer->set_headers();
}
