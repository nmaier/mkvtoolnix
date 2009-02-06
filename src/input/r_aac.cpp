/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AAC demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <algorithm>

#include "common.h"
#include "error.h"
#include "id3_common.h"
#include "r_aac.h"
#include "p_aac.h"
#include "output_control.h"

#define PROBESIZE 8192

int
aac_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  unsigned char buf[PROBESIZE];
  aac_header_t aacheader;
  int pos;

  if (size < PROBESIZE)
    return 0;
  try {
    io->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*io);
    if (io->read(buf, PROBESIZE) != PROBESIZE)
      io->setFilePointer(0, seek_beginning);
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }

  if (parse_aac_adif_header(buf, PROBESIZE, &aacheader))
    return 1;

  pos = find_aac_header(buf, PROBESIZE, &aacheader, false);
  if ((0 > pos) || ((pos + aacheader.bytes) >= PROBESIZE)) {
    pos = find_aac_header(buf, PROBESIZE, &aacheader, true);
    if ((0 > pos) || ((pos + aacheader.bytes) >= PROBESIZE))
      return 0;
    pos = find_aac_header(&buf[pos + aacheader.bytes], PROBESIZE - pos - aacheader.bytes, &aacheader, true);
    if (0 != pos)
      return 0;
  }

  pos = find_aac_header(&buf[pos + aacheader.bytes], PROBESIZE - pos - aacheader.bytes, &aacheader, false);
  if (0 != pos)
    return 0;

  return 1;
}

#define INITCHUNKSIZE   16384
#define SINITCHUNKSIZE "16384"

aac_reader_c::aac_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  sbr_status_set(false) {

  int adif, detected_profile;

  try {
    io                 = new mm_file_io_c(ti.fname);
    size               = io->get_size();

    int tag_size_start = skip_id3v2_tag(*io);
    int tag_size_end   = id3_tag_present_at_end(*io);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      size -= tag_size_end;

    int init_read_len = std::min(size - tag_size_start, (int64_t)INITCHUNKSIZE);
    chunk             = (unsigned char *)safemalloc(INITCHUNKSIZE);

    if (io->read(chunk, init_read_len) != init_read_len)
      throw error_c(boost::format(Y("aac_reader: Could not read %1% bytes.")) % init_read_len);

    io->setFilePointer(tag_size_start, seek_beginning);

    if (parse_aac_adif_header(chunk, init_read_len, &aacheader)) {
      throw error_c(Y("aac_reader: ADIF header files are not supported."));
      adif = 1;

    } else {
      if (find_aac_header(chunk, init_read_len, &aacheader, emphasis_present) < 0)
        throw error_c(boost::format(Y("aac_reader: No valid AAC packet found in the first %1% bytes.\n")) % init_read_len);
      guess_adts_version();
      adif = 0;
    }

    ti.id            = 0;       // ID for this track.
    bytes_processed  = 0;
    detected_profile = aacheader.profile;

    if (24000 >= aacheader.sample_rate)
      aacheader.profile = AAC_PROFILE_SBR;

    if (   (map_has_key(ti.all_aac_is_sbr,  0) && ti.all_aac_is_sbr[ 0])
        || (map_has_key(ti.all_aac_is_sbr, -1) && ti.all_aac_is_sbr[-1]))
      aacheader.profile = AAC_PROFILE_SBR;

    if (   (map_has_key(ti.all_aac_is_sbr,  0) && !ti.all_aac_is_sbr[ 0])
        || (map_has_key(ti.all_aac_is_sbr, -1) && !ti.all_aac_is_sbr[-1]))
      aacheader.profile = detected_profile;

    if (   map_has_key(ti.all_aac_is_sbr,  0)
        || map_has_key(ti.all_aac_is_sbr, -1))
      sbr_status_set = true;

  } catch (...) {
    throw error_c(Y("aac_reader: Could not open the file."));
  }

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the AAC demultiplexer.\n"));
}

aac_reader_c::~aac_reader_c() {
  delete io;
  safefree(chunk);
}

void
aac_reader_c::create_packetizer(int64_t) {
  generic_packetizer_c *aacpacketizer;

  if (NPTZR() != 0)
    return;
  if (!sbr_status_set)
    mxwarn(Y("AAC files may contain HE-AAC / AAC+ / SBR AAC audio. "
             "This can NOT be detected automatically. Therefore you have to "
             "specifiy '--aac-is-sbr 0' manually for this input file if the "
             "file actually contains SBR AAC. The file will be muxed in the "
             "WRONG way otherwise. Also read mkvmerge's documentation.\n"));

  aacpacketizer = new aac_packetizer_c(this, ti, aacheader.id, aacheader.profile, aacheader.sample_rate, aacheader.channels, emphasis_present);
  add_packetizer(aacpacketizer);

  if (AAC_PROFILE_SBR == aacheader.profile)
    aacpacketizer->set_audio_output_sampling_freq(aacheader.sample_rate * 2);

  mxinfo_tid(ti.fname, 0, Y("Using the AAC output module.\n"));
}

// Try to guess if the MPEG4 header contains the emphasis field (2 bits)
void
aac_reader_c::guess_adts_version() {
  aac_header_t tmp_aacheader;

  emphasis_present = false;

  // Due to the checks we do have an ADTS header at 0.
  find_aac_header(chunk, INITCHUNKSIZE, &tmp_aacheader, emphasis_present);
  if (tmp_aacheader.id != 0)        // MPEG2
    return;

  // Now make some sanity checks on the size field.
  if (tmp_aacheader.bytes > 8192) {
    emphasis_present = true;    // Looks like it's borked.
    return;
  }

  // Looks ok so far. See if the next ADTS is right behind this packet.
  int pos = find_aac_header(&chunk[tmp_aacheader.bytes], INITCHUNKSIZE - tmp_aacheader.bytes, &tmp_aacheader, emphasis_present);
  if (0 != pos) {               // Not ok - what do we do now?
    emphasis_present = true;
    return;
  }
}

file_status_e
aac_reader_c::read(generic_packetizer_c *,
                   bool) {
  int remaining_bytes = size - io->getFilePointer();
  int read_len        = std::min(INITCHUNKSIZE, remaining_bytes);
  int num_read        = io->read(chunk, read_len);

  if (0 > num_read) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  PTZR0->process(new packet_t(new memory_c(chunk, num_read, false)));
  bytes_processed += num_read;

  if (0 < (remaining_bytes - num_read))
    return FILE_STATUS_MOREDATA;

  PTZR0->flush();

  return FILE_STATUS_DONE;
}

int
aac_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
aac_reader_c::identify() {
  string verbose_info = string("aac_is_sbr:") + string(AAC_PROFILE_SBR == aacheader.profile ? "true" : "unknown");

  id_result_container("AAC");
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "AAC", verbose_info);
}
