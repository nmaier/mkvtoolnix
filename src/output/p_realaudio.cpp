/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * RealAudio output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matroska.h"
#include "pr_generic.h"
#include "p_realaudio.h"

using namespace libmatroska;

ra_packetizer_c::ra_packetizer_c(generic_reader_c *nreader,
                                 unsigned long nsamples_per_sec,
                                 int nchannels,
                                 int nbits_per_sample,
                                 uint32_t nfourcc,
                                 unsigned char *nprivate_data,
                                 int nprivate_size,
                                 track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  fourcc = nfourcc;
  private_data = (unsigned char *)safememdup(nprivate_data, nprivate_size);
  private_size = nprivate_size;
  skip_to_keyframe = false;
  buffer_until_keyframe = false;
  if (initial_displacement < 0) {
    mxwarn("Track %lld/'%s': Negative '--sync' is not supported for "
           "RealAudio tracks.\n", ti->id, ti->fname.c_str());
    initial_displacement = 0;
  }

  set_track_type(track_audio);
}

ra_packetizer_c::~ra_packetizer_c() {
  safefree(private_data);
}

void
ra_packetizer_c::set_headers() {
  char codec_id[20];
  int i;

  sprintf(codec_id, "A_REAL/%c%c%c%c", (char)(fourcc >> 24),
          (char)((fourcc >> 16) & 0xff), (char)((fourcc >> 8) & 0xff),
          (char)(fourcc & 0xff));
  for (i = 0; i < strlen(codec_id); i++)
    codec_id[i] = toupper(codec_id[i]);
  set_codec_id(codec_id);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);
  set_codec_private(private_data, private_size);

  generic_packetizer_c::set_headers();
  track_entry->EnableLacing(false);
}

int
ra_packetizer_c::process(memory_c &mem,
                         int64_t timecode,
                         int64_t duration,
                         int64_t bref,
                         int64_t) {
//   bool buffer_this;
//   int64_t buffered_duration;
//   int i;

  debug_enter("ra_packetizer_c::process");

//   if ((duration <= 0) && (initial_displacement != 0))
//     mxerror("RealAudio sync wanted, but duration == 0. Not yet "
//             "implemented.\n");

//   if (skip_to_keyframe) {
//     if (bref == -1)
//       skip_to_keyframe = false;
//     else {
//       displace(-duration);
//       return FILE_STATUS_MOREDATA;
//     }
//   }

//   if (needs_negative_displacement(duration)) {
//     skip_to_keyframe = true;
//     displace(-duration);
//     return FILE_STATUS_MOREDATA;
//   } else if (needs_positive_displacement(duration)) {
//     if (!buffer_until_keyframe) {
//       buffer_until_keyframe = true;
//       buffer_this = true;
//     } else if (bref != -1)
//       buffer_this = true;
//     else {
//       buffered_duration = 0;
//       for (i = 0; i < buffered_durations.size(); i++)
//         buffered_duration += buffered_durations[i];
//       displace(buffered_duration);
//       add_packet(*buffered_packets[0], buffered_timecodes[0],
//                  buffered_durations[0]);
//       delete buffered_packets[0];
//       for (i = 1; i < buffered_durations.size(); i++) {
//         add_packet(*buffered_packets[i], buffered_timecodes[i],
//                    buffered_durations[i], buffered_durations[i - 1]);
//         delete buffered_packets[i];
//       }
//       buffered_packets.clear();
//       buffered_timecodes.clear();
//       buffered_durations.clear();
//       if (needs_positive_displacement(duration))
//       buffer_until_keyframe = false;
//       buffer_until_keyframe = false;
//     }
//     if (buffer_this) {
//       buffered_packets.push_back(mem.grab());
//       buffered_timecodes.push_back(timecode);
//       buffered_durations.push_back(duration);
//       return FILE_STATUS_MOREDATA;
//     }
//   }

  add_packet(mem, timecode + initial_displacement, duration, false,
             bref == -1 ? bref : bref + initial_displacement);

  debug_leave("ra_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
ra_packetizer_c::dump_debug_info() {
}

connection_result_e
ra_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  ra_packetizer_c *psrc;

  psrc = dynamic_cast<ra_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((samples_per_sec != psrc->samples_per_sec) ||
      (channels != psrc->channels) ||
      (bits_per_sample != psrc->bits_per_sample))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
