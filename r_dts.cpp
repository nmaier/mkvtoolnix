/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_dts.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_dts.cpp,v 1.1 2003/05/15 08:58:52 mosu Exp $
    \brief DTS demultiplexer module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// extern "C" {
// #include <avilib.h>
// }

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_dts.h"
#include "p_dts.h"

int dts_reader_c::probe_file(FILE *file, int64_t size) { 
  char buf[4096];
  int pos;
  dts_header_t dtsheader;
  
  if (size < 4096)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(buf, 1, 4096, file) != 4096) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  
  pos = find_dts_header((unsigned char *)buf, 4096, &dtsheader);
  if (pos < 0)
    return 0;
  
  return 1;    
}

dts_reader_c::dts_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int pos;
  dts_header_t dtsheader;
  
  if ((file = fopen(ti->fname, "rb")) == NULL)
    throw error_c("dts_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("dts_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("dts_reader: Could not seek to beginning of file.");
  chunk = (unsigned char *)safemalloc(4096);
  if (fread(chunk, 1, 4096, file) != 4096)
    throw error_c("dts_reader: Could not read 4096 bytes.");
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("dts_reader: Could not seek to beginning of file.");
  pos = find_dts_header(chunk, 4096, &dtsheader);
  if (pos < 0)
    throw error_c("dts_reader: No valid DTS packet found in the first " \
                  "4096 bytes.\n");
  bytes_processed = 0;
  dtspacketizer = new dts_packetizer_c(this, dtsheader.sample_rate, ti);
  if (verbose)
    fprintf(stdout, "Using DTS demultiplexer for %s.\n+-> Using " \
            "DTS output module for audio stream.\n", ti->fname);
}

dts_reader_c::~dts_reader_c() {
  if (file != NULL)
    fclose(file);
  safefree(chunk);
  if (dtspacketizer != NULL)
    delete dtspacketizer;
}

int dts_reader_c::read() {
  int nread;
  
  nread = fread(chunk, 1, 4096, file);
  if (nread <= 0)
    return 0;

  dtspacketizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

packet_t *dts_reader_c::get_packet() {
  return dtspacketizer->get_packet();
}

int dts_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void dts_reader_c::display_progress() {
  fprintf(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void dts_reader_c::set_headers() {
  dtspacketizer->set_headers();
}
