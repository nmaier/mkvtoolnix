/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_wav.cpp
  WAVE demultiplexer module (supports raw / uncompressed PCM audio only,
  type 0x0001)

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

extern "C" {
#include <avilib.h> // for wave_header
}

#include "ogmmerge.h"
#include "ogmstreams.h"
#include "queue.h"
#include "r_wav.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int wav_reader_c::probe_file(FILE *file, u_int64_t size) {
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

wav_reader_c::wav_reader_c(char *fname, audio_sync_t *nasync,
                           range_t *nrange, char **ncomments) throw (error_c) {
  u_int64_t size;
  
  if ((file = fopen(fname, "r")) == NULL)
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
                                       wheader.common.wBitsPerSample, nasync,
                                       nrange, ncomments);
  if (verbose)
    fprintf(stderr, "Using WAV demultiplexer for %s.\n+-> Using " \
            "PCM output module for audio stream.\n", fname);
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
  int nread, last_frame;
    
  if (pcmpacketizer->page_available())
    return EMOREDATA;
  
  nread = fread(chunk, 1, bps, file);
  if (nread <= 0) {
/*
 * In case that the WAVE header is not set correctly or any other error
 * occurs. The 'normal' end-of-stream should be handled by comparing the
 * number of bytes read to the WAVE header fields.
 */
    *chunk = 0;
    pcmpacketizer->process((char *)chunk, 1, 1);
    pcmpacketizer->flush_pages();
    return 0;
  }
  last_frame = 0;
  if ((bytes_processed + nread) >= (wheader.riff.len - sizeof(wave_header) + 8))
    last_frame = 1;
  pcmpacketizer->process((char *)chunk, nread, last_frame);
  bytes_processed += nread;

  if (last_frame)
    return 0;
  else
    return EMOREDATA;
}

int wav_reader_c::serial_in_use(int serial) {
  return pcmpacketizer->serial_in_use(serial);
}

ogmmerge_page_t *wav_reader_c::get_header_page(int header_type) {
  return pcmpacketizer->get_header_page(header_type);
}

ogmmerge_page_t *wav_reader_c::get_page() {
  return pcmpacketizer->get_page();
}

int wav_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void wav_reader_c::reset() {
  if (pcmpacketizer != NULL)
    pcmpacketizer->reset();
}

void wav_reader_c::display_progress() {
  int samples = (wheader.riff.len - sizeof(wheader) + 8) / bps;
  fprintf(stdout, "progress: %d/%d seconds (%d%%)\r",
          (int)(bytes_processed / bps), (int)samples,
          (int)(bytes_processed * 100L / bps / samples));
  fflush(stdout);
}

