/*
  ogmmerge -- utility for splicing together ogg bitstreams
  from component media subtypes

  r_ac3.cpp
  AC3 demultiplexer module

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
#include "r_ac3.h"
#include "ac3_common.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int ac3_reader_c::probe_file(FILE *file, u_int64_t size) { 
  char         buf[4096];
  int          pos;
  ac3_header_t ac3header;
  
  if (size < 4096)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(buf, 1, 4096, file) != 4096) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  
  pos = find_ac3_header((unsigned char *)buf, 4096, &ac3header);
  if (pos < 0)
    return 0;
  
  return 1;    
}

ac3_reader_c::ac3_reader_c(char *fname, audio_sync_t *nasync,
                           range_t *nrange, char **ncomments) throw (error_c) {
  int          pos;
  ac3_header_t ac3header;
  
  
  if ((file = fopen(fname, "r")) == NULL)
    throw error_c("ac3_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("ac3_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("ac3_reader: Could not seek to beginning of file.");
  chunk = (unsigned char *)malloc(4096);
  if (chunk == NULL)
    die("malloc");
  if (fread(chunk, 1, 4096, file) != 4096)
    throw error_c("ac3_reader: Could not read 4096 bytes.");
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("ac3_reader: Could not seek to beginning of file.");
  pos = find_ac3_header(chunk, 4096, &ac3header);
  if (pos < 0)
    throw error_c("ac3_reader: No valid AC3 packet found in the first " \
                  "4096 bytes.\n");
  bytes_processed = 0;
  ac3packetizer = new ac3_packetizer_c(ac3header.sample_rate,
                                       ac3header.channels,
                                       ac3header.bit_rate / 1000,
                                       nasync, nrange, ncomments);
  if (verbose)
    fprintf(stderr, "Using AC3 demultiplexer for %s.\n+-> Using " \
            "AC3 output module for audio stream.\n", fname);
}

ac3_reader_c::~ac3_reader_c() {
  if (file != NULL)
    fclose(file);
  if (chunk != NULL)
    free(chunk);
  if (ac3packetizer != NULL)
    delete ac3packetizer;
}

int ac3_reader_c::read() {
  int nread, last_frame;
  
  do {
    if (ac3packetizer->page_available())
      return EMOREDATA;

    nread = fread(chunk, 1, 4096, file);
    if (nread <= 0) {
      ac3packetizer->produce_eos_packet();
      return 0;
    }
    last_frame = (nread == 4096 ? 0 : 1);
    ac3packetizer->process((char *)chunk, nread, last_frame);
    bytes_processed += nread;

    if (last_frame)
      return 0;
  } while (1);
}

int ac3_reader_c::serial_in_use(int serial) {
  return ac3packetizer->serial_in_use(serial);
}

ogmmerge_page_t *ac3_reader_c::get_header_page(int header_type) {
  return ac3packetizer->get_header_page(header_type);
}

ogmmerge_page_t *ac3_reader_c::get_page() {
  return ac3packetizer->get_page();
}

void ac3_reader_c::reset() {
  if (ac3packetizer != NULL)
    ac3packetizer->reset();
}

int ac3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void ac3_reader_c::display_progress() {
  fprintf(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

