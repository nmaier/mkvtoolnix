/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_aac.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_aac.cpp,v 1.6 2003/05/22 16:14:29 mosu Exp $
    \brief AAC demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_aac.h"
#include "p_aac.h"

int aac_reader_c::probe_file(FILE *file, int64_t size) {
  char buf[4096];
  aac_header_t aacheader;

  if (size < 4096)
    return 0;
  if (fseeko(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread(buf, 1, 4096, file) != 4096) {
    fseeko(file, 0, SEEK_SET);
    return 0;
  }
  fseeko(file, 0, SEEK_SET);

  if (parse_aac_adif_header((unsigned char *)buf, 4096, &aacheader))
    return 1;
  if (find_aac_header((unsigned char *)buf, 4096, &aacheader) < 0)
    return 0;

  return 1;
}

aac_reader_c::aac_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int adif;
  aac_header_t aacheader;

  if ((file = fopen(ti->fname, "rb")) == NULL)
    throw error_c("aac_reader: Could not open source file.");
  if (fseeko(file, 0, SEEK_END) != 0)
    throw error_c("aac_reader: Could not seek to end of file.");
  size = ftello(file);
  if (fseeko(file, 0, SEEK_SET) != 0)
    throw error_c("aac_reader: Could not seek to beginning of file.");
  chunk = (unsigned char *)safemalloc(4096);
  if (fread(chunk, 1, 4096, file) != 4096)
    throw error_c("aac_reader: Could not read 4096 bytes.");
  if (fseeko(file, 0, SEEK_SET) != 0)
    throw error_c("aac_reader: Could not seek to beginning of file.");
  if (parse_aac_adif_header(chunk, 4096, &aacheader)) {
    throw error_c("aac_reader: ADIF header files are not supported.");
    adif = 1;
  } else if (find_aac_header(chunk, 4096, &aacheader) < 0)
    throw error_c("aac_reader: No valid AAC packet found in the first " \
                  "4096 bytes.\n");
  else
    adif = 0;
  bytes_processed = 0;
  aacpacketizer = new aac_packetizer_c(this, aacheader.id, aacheader.profile,
                                       aacheader.sample_rate,
                                       aacheader.channels, ti);
  if (verbose)
    fprintf(stdout, "Using AAC demultiplexer for %s.\n+-> Using " \
            "AAC output module for audio stream.\n", ti->fname);
}

aac_reader_c::~aac_reader_c() {
  if (file != NULL)
    fclose(file);
  if (chunk != NULL)
    safefree(chunk);
  if (aacpacketizer != NULL)
    delete aacpacketizer;
}

int aac_reader_c::read() {
  int nread;

  nread = fread(chunk, 1, 4096, file);
  if (nread <= 0)
    return 0;

  aacpacketizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

packet_t *aac_reader_c::get_packet() {
  return aacpacketizer->get_packet();
}

int aac_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void aac_reader_c::display_progress() {
  fprintf(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void aac_reader_c::set_headers() {
  aacpacketizer->set_headers();
}
