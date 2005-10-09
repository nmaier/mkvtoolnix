/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Subripper subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "pr_generic.h"
#include "p_textsubs.h"
#include "matroska.h"

using namespace libmatroska;

textsubs_packetizer_c::textsubs_packetizer_c(generic_reader_c *_reader,
                                             const char *_codec_id,
                                             const void *_global_data,
                                             int _global_size,
                                             bool _recode,
                                             bool is_utf8,
                                             track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  packetno(0), cc_utf8(0), global_data(safememdup(_global_data, _global_size)),
  global_size(_global_size), codec_id(_codec_id), recode(_recode) {

  if (recode) {
    if ((ti.sub_charset != "") || !is_utf8)
      cc_utf8 = utf8_init(ti.sub_charset);
    else
      cc_utf8 = utf8_init("UTF-8");
  }

  set_track_type(track_subtitle);
  if (codec_id == MKV_S_TEXTUSF)
    set_default_compression_method(COMPRESSION_ZLIB);
}

textsubs_packetizer_c::~textsubs_packetizer_c() {
}

void
textsubs_packetizer_c::set_headers() {
  set_codec_id(codec_id);
  if (NULL != global_data.get())
    set_codec_private((unsigned char *)global_data, global_size);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
textsubs_packetizer_c::process(packet_cptr packet) {
  int num_newlines;
  char *subs, *idx1, *idx2;
  int64_t end;

  end = packet->timecode + packet->duration;
  // Adjust the start and end values according to the audio adjustment.
  packet->timecode += initial_displacement;
  packet->timecode = (int64_t)(ti.async.linear * packet->timecode);
  end += initial_displacement;
  end = (int64_t)(ti.async.linear * end);

  if (end < 0)
    return FILE_STATUS_MOREDATA;
  else if (packet->timecode < 0)
    packet->timecode = 0;

  if (packet->duration < 0) {
    mxwarn("textsubs_packetizer: Ignoring an entry which starts after it ends."
           "\n");
    return FILE_STATUS_MOREDATA;
  }

  // Count the number of lines.
  idx1 = (char *)packet->data->get();
  subs = NULL;
  num_newlines = 0;
  while (*idx1 != 0) {
    if (*idx1 == '\n')
      num_newlines++;
    idx1++;
  }
  subs = (char *)safemalloc(strlen((char *)packet->data->get()) +
                            num_newlines * 2 + 1);

  // Unify the new lines into DOS style newlines.
  idx1 = (char *)packet->data->get();
  idx2 = subs;
  while (*idx1 != 0) {
    if (*idx1 == '\n') {
      *idx2 = '\r';
      idx2++;
      *idx2 = '\n';
      idx2++;
    } else if (*idx1 != '\r') {
      *idx2 = *idx1;
      idx2++;
    }
    idx1++;
  }
  if (idx2 != subs) {
    while (((idx2 - 1) != subs) && iscr(*(idx2 - 1))) {
      *idx2 = 0;
      idx2--;
    }
  }
  *idx2 = 0;

  packet->duration_mandatory = true;
  if (recode) {
    string utf8_subs;

    utf8_subs = to_utf8(cc_utf8, subs);
    safefree(subs);
    packet->data =
      memory_cptr(new memory_c((unsigned char *)utf8_subs.c_str(),
                               utf8_subs.length(), false));
    add_packet(packet);

  } else {
    packet->data =
      memory_cptr(new memory_c((unsigned char *)subs, strlen(subs), true));
    add_packet(packet);
  }

  return FILE_STATUS_MOREDATA;
}

void
textsubs_packetizer_c::dump_debug_info() {
  mxdebug("textsubs_packetizer_c: queue: %u\n",
          (unsigned int)packet_queue.size());
}

connection_result_e
textsubs_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                      string &error_message) {
  textsubs_packetizer_c *psrc;

  psrc = dynamic_cast<textsubs_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  return CAN_CONNECT_YES;
}
