/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_mp3.cpp
  MP3 demultiplexer module

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"
#include "ogmstreams.h"
#include "queue.h"
#include "r_mp3.h"
#include "mp3_common.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int mp3_reader_c::probe_file(FILE *file, u_int64_t size) { 
  char          buf[4096];
  int           pos;
  unsigned long header;
  mp3_header_t  mp3header;
  
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

mp3_reader_c::mp3_reader_c(char *fname, audio_sync_t *nasync,
                           range_t *nrange, char **ncomments) throw (error_c) {
  int           pos;
  unsigned long header;
  mp3_header_t  mp3header;
  
  
  if ((file = fopen(fname, "r")) == NULL)
    throw error_c("mp3_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("mp3_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("mp3_reader: Could not seek to beginning of file.");
  chunk = (unsigned char *)malloc(4096);
  if (chunk == NULL)
    die("malloc");
  if (fread(chunk, 1, 4096, file) != 4096)
    throw error_c("mp3_reader: Could not read 4096 bytes.");
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("mp3_reader: Could not seek to beginning of file.");
  pos = find_mp3_header((char *)chunk, 4096, &header);
  if (pos < 0)
    throw error_c("mp3_reader: No valid MP3 packet found in the first " \
                  "4096 bytes.\n");
  decode_mp3_header(header, &mp3header);

  bytes_processed = 0;
  mp3packetizer = new mp3_packetizer_c(mp3_freqs[mp3header.sampling_frequency],
                                       mp3header.stereo ? 2 : 1,
                                       mp3_tabsel[mp3header.lsf]
                                         [mp3header.bitrate_index],
                                       nasync, nrange, ncomments);
  if (verbose)
    fprintf(stderr, "Using MP3 demultiplexer for %s.\n+-> Using " \
            "MP3 output module for audio stream.\n", fname);
}

mp3_reader_c::~mp3_reader_c() {
  if (file != NULL)
    fclose(file);
  if (chunk != NULL)
    free(chunk);
  if (mp3packetizer != NULL)
    delete mp3packetizer;
}

int mp3_reader_c::read() {
  int nread, last_frame;
  
  do {
    if (mp3packetizer->page_available())
      return EMOREDATA;

    nread = fread(chunk, 1, 4096, file);
    if (nread <= 0) {
      mp3packetizer->produce_eos_packet();
      return 0;
    }
    last_frame = (nread == 4096 ? 0 : 1);
    mp3packetizer->process((char *)chunk, nread, last_frame);
    bytes_processed += nread;

    if (last_frame)
      return 0;
  } while (1);
}

int mp3_reader_c::serial_in_use(int serial) {
  return mp3packetizer->serial_in_use(serial);
}

ogmmerge_page_t *mp3_reader_c::get_header_page(int header_type) {
  return mp3packetizer->get_header_page(header_type);
}

ogmmerge_page_t *mp3_reader_c::get_page() {
  return mp3packetizer->get_page();
}

void mp3_reader_c::reset() {
  if (mp3packetizer != NULL)
    mp3packetizer->reset();
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

