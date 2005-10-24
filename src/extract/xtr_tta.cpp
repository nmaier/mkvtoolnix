/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <time.h>

#include "checksums.h"
#include "commonebml.h"
#include "tta_common.h"

#include "xtr_tta.h"

const double xtr_tta_c::tta_frame_time = 1.04489795918367346939l;

xtr_tta_c::xtr_tta_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  last_duration(0), bps(0), channels(0), sfreq(0) {

  temp_file_name = mxsprintf("mkvextract-" LLD "-temp-tta-%u", tid,
                             (uint32_t)time(NULL));
}

void
xtr_tta_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  try {
    out = new mm_file_io_c(temp_file_name.c_str(), MODE_CREATE);
  } catch (...) {
    mxerror("Failed to create the temporary file '%s': %d (%s)\n",
            temp_file_name.c_str(), errno, strerror(errno));
  }

  bps = kt_get_a_bps(track);
  channels = kt_get_a_channels(track);
  sfreq = (int)kt_get_a_sfreq(track);
}

void
xtr_tta_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  frame_sizes.push_back(frame->get_size());
  out->write(frame->get(), frame->get_size());

  if (0 < duration)
    last_duration = duration;
}

void
xtr_tta_c::finish_file() {
  string dummy_out_name;
  tta_file_header_t tta_header;
  unsigned char *buffer;
  uint32_t crc;
  mm_io_c *in;
  int nread, k;

  delete out;
  out = NULL;

  in = NULL;
  try {
    in = new mm_file_io_c(temp_file_name);
  } catch (...) {
    mxerror(" The temporary file '%s' could not be opened for reading (%s)."
            "\n", temp_file_name.c_str(), strerror(errno));
  }

  try {
    out = new mm_file_io_c(file_name, MODE_CREATE);
  } catch (...) {
    delete in;
    mxerror(" The file '%s' could not be opened for writing (%s).\n",
            file_name.c_str(), strerror(errno));
  }

  memcpy(tta_header.signature, "TTA1", 4);
  if (bps != 3)
    put_uint16_le(&tta_header.audio_format, 1);
  else
    put_uint16_le(&tta_header.audio_format, 3);
  put_uint16_le(&tta_header.channels, channels);
  put_uint16_le(&tta_header.bits_per_sample, bps);
  put_uint32_le(&tta_header.sample_rate, sfreq);
  if (0 >= last_duration)
    last_duration = (int64_t)(TTA_FRAME_TIME * sfreq) * 1000000000ll;
  put_uint32_le(&tta_header.data_length,
                (uint32_t)(sfreq *
                           (TTA_FRAME_TIME * (frame_sizes.size() - 1) +
                            (double)last_duration / 1000000000.0l)));
  put_uint32_le(&tta_header.crc,
                calc_crc32((unsigned char *)&tta_header,
                           sizeof(tta_file_header_t) - 4));
  out->write(&tta_header, sizeof(tta_file_header_t));

  buffer = (unsigned char *)safemalloc(frame_sizes.size() * 4);
  for (k = 0; k < frame_sizes.size(); k++)
    put_uint32_le(buffer + 4 * k, frame_sizes[k]);

  out->write(buffer, frame_sizes.size() * 4);
  crc = calc_crc32(buffer, frame_sizes.size() * 4);
  out->write_uint32_le(crc);
  safefree(buffer);

  mxinfo("\nThe temporary TTA file for track ID " LLD " is being "
         "copied into the final TTA file. This may take some time.\n", tid);

  buffer = (unsigned char *)safemalloc(128000);
  do {
    nread = in->read(buffer, 128000);
    out->write(buffer, nread);
  } while (nread == 128000);

  delete in;
  delete out;
  out = NULL;
  unlink(temp_file_name.c_str());
}
