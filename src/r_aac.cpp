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
    \version $Id$
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

int aac_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  char buf[4096];
  aac_header_t aacheader;

  if (size < 4096)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, 4096) != 4096)
      mm_io->setFilePointer(0, seek_beginning);
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (parse_aac_adif_header((unsigned char *)buf, 4096, &aacheader))
    return 1;
  if (find_aac_header((unsigned char *)buf, 4096, &aacheader, false) != 0)
    return 0;

  return 1;
}

#define INITCHUNKSIZE 16384
#define SINITCHUNKSIZE "16384"

aac_reader_c::aac_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int adif, i;
  aac_header_t aacheader;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
    chunk = (unsigned char *)safemalloc(INITCHUNKSIZE);
    if (mm_io->read(chunk, INITCHUNKSIZE) != INITCHUNKSIZE)
      throw error_c("aac_reader: Could not read " SINITCHUNKSIZE " bytes.");
    mm_io->setFilePointer(0, seek_beginning);
    if (parse_aac_adif_header(chunk, INITCHUNKSIZE, &aacheader)) {
      throw error_c("aac_reader: ADIF header files are not supported.");
      adif = 1;
    } else {
      if (find_aac_header(chunk, INITCHUNKSIZE, &aacheader, emphasis_present)
          != 0)
        throw error_c("aac_reader: No valid AAC packet found in the first "
                      SINITCHUNKSIZE " bytes.\n");
      guess_adts_version();
      adif = 0;
    }
    bytes_processed = 0;
    ti->id = 0;                 // ID for this track.

    for (i = 0; i < ti->aac_is_sbr->size(); i++)
      if (((*ti->aac_is_sbr)[i] == 0) || ((*ti->aac_is_sbr)[i] == -1)) {
        aacheader.profile = AAC_PROFILE_SBR;
        break;
      }
    if (aacheader.profile != AAC_PROFILE_SBR)
      mxwarn("AAC files may contain HE-AAC / AAC+ / SBR AAC audio. "
             "This can NOT be detected automatically. Therefore you have to "
             "specifiy '--aac-is-sbr 0' manually for this input file if the "
             "file actually contains SBR AAC. The file will be muxed in the "
             "WRONG way otherwise. Also read mkvmerge's documentation.\n");

    aacpacketizer = new aac_packetizer_c(this, aacheader.id, aacheader.profile,
                                         aacheader.sample_rate,
                                         aacheader.channels, ti,
                                         emphasis_present);
    if (aacheader.profile == AAC_PROFILE_SBR)
      aacpacketizer->set_audio_output_sampling_freq(aacheader.sample_rate * 2);
  } catch (exception &ex) {
    throw error_c("aac_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo("Using AAC demultiplexer for %s.\n+-> Using "
           "AAC output module for audio stream.\n", ti->fname);
}

aac_reader_c::~aac_reader_c() {
  delete mm_io;
  if (chunk != NULL)
    safefree(chunk);
  if (aacpacketizer != NULL)
    delete aacpacketizer;
}

// Try to guess if the MPEG4 header contains the emphasis field (2 bits)
void aac_reader_c::guess_adts_version() {
  int pos;
  aac_header_t aacheader;

  emphasis_present = false;

  // Due to the checks we do have an ADTS header at 0.
  find_aac_header(chunk, INITCHUNKSIZE, &aacheader, emphasis_present);
  if (aacheader.id != 0)        // MPEG2
    return;

  // Now make some sanity checks on the size field.
  if (aacheader.bytes > 8192) { 
    emphasis_present = true;    // Looks like it's borked.
    return;
  }

  // Looks ok so far. See if the next ADTS is right behind this packet.
  pos = find_aac_header(&chunk[aacheader.bytes], INITCHUNKSIZE -
                        aacheader.bytes, &aacheader, emphasis_present);
  if (pos != 0) {               // Not ok - what do we do now?
    emphasis_present = true;
    return;
  }
}

int aac_reader_c::read(generic_packetizer_c *) {
  int nread;

  nread = mm_io->read(chunk, 4096);
  if (nread <= 0)
    return 0;

  aacpacketizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

int aac_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void aac_reader_c::display_progress() {
  mxinfo("progress: %lld/%lld bytes (%d%%)\r",
         bytes_processed, size,
         (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void aac_reader_c::set_headers() {
  aacpacketizer->set_headers();
}

void aac_reader_c::identify() {
  mxinfo("File '%s': container: AAC\nTrack ID 0: audio (AAC)\n", ti->fname);
}
