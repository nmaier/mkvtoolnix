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
    \version \$Id: r_wav.cpp,v 1.10 2003/04/18 10:08:24 mosu Exp $
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
#include "queue.h"
#include "r_wav.h"
#include "p_pcm.h"

extern "C" {
#include <avilib.h> // for wave_header
}

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int wav_reader_c::probe_file(FILE *file, int64_t size) {
  wave_header wheader;
  if (size < sizeof(wave_header))
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fread((char *)&wheader, 1, sizeof(wheader), file) != sizeof(wheader)) {
    fseek(file, 0, SEEK_SET);
    return 0;
  }
  fseek(file, 0, SEEK_SET);
  if (strncmp((char *)wheader.riff.id, "RIFF", 4) ||
      strncmp((char *)wheader.riff.wave_id, "WAVE", 4) ||
      strncmp((char *)wheader.data.id, "data", 4))
    return 0;
  return 1;    
}

wav_reader_c::wav_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int64_t size;
  
  if ((file = fopen(ti->fname, "r")) == NULL)
    throw error_c("wav_reader: Could not open source file.");
  if (fseek(file, 0, SEEK_END) != 0)
    throw error_c("wav_reader: Could not seek to end of file.");
  size = ftell(file);
  if (fseek(file, 0, SEEK_SET) != 0)
    throw error_c("wav_reader: Could not seek to beginning of file.");
  if (!wav_reader_c::probe_file(file, size))
    throw error_c("wav_reader: Source is not a valid WAVE file.");
  if (fread(&wheader, 1, sizeof(wheader), file) != sizeof(wheader))
    throw error_c("wav_reader: could not read WAVE header.");
  bps = wheader.common.wChannels * wheader.common.wBitsPerSample *
        wheader.common.dwSamplesPerSec / 8;
  chunk = (unsigned char *)malloc(bps + 1);
  if (chunk == NULL)
    die("malloc");
  bytes_processed = 0;
  pcmpacketizer = new pcm_packetizer_c(wheader.common.dwSamplesPerSec,
                                       wheader.common.wChannels,
                                       wheader.common.wBitsPerSample, ti);
  if (verbose)
    fprintf(stdout, "Using WAV demultiplexer for %s.\n+-> Using " \
            "PCM output module for audio stream.\n", ti->fname);
}

wav_reader_c::~wav_reader_c() {
  if (file != NULL)
    fclose(file);
  if (chunk != NULL)
    free(chunk);
  if (pcmpacketizer != NULL)
    delete pcmpacketizer;
}

int wav_reader_c::read() {
  int nread;
    
  if (pcmpacketizer->packet_available())
    return EMOREDATA;
  
  nread = fread(chunk, 1, bps, file);
  if (nread <= 0)
    return 0;

  pcmpacketizer->process(chunk, nread);
  bytes_processed += nread;

  if (nread != bps)
    return 0;
  else
    return EMOREDATA;
}

packet_t *wav_reader_c::get_packet() {
  return pcmpacketizer->get_packet();
}

int wav_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

// void wav_reader_c::reset() {
//   if (pcmpacketizer != NULL)
//     pcmpacketizer->reset();
// }

void wav_reader_c::display_progress() {
  int samples = (wheader.riff.len - sizeof(wheader) + 8) / bps;
  fprintf(stdout, "progress: %d/%d seconds (%d%%)\r",
          (int)(bytes_processed / bps), (int)samples,
          (int)(bytes_processed * 100L / bps / samples));
  fflush(stdout);
}
