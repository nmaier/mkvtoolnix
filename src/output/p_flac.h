/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the Flac packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#ifndef __P_FLAC_H
#define __P_FLAC_H

#include "os.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include <FLAC/format.h>

#include "common.h"
#include "pr_generic.h"
#include "smart_pointers.h"

class flac_packetizer_c: public generic_packetizer_c {
private:
  memory_cptr m_header;
  int64_t m_num_packets;
  FLAC__StreamMetadata_StreamInfo m_stream_info;

public:
  flac_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, unsigned char *header, int l_header) throw (error_c);
  virtual ~flac_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "Flac";
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);
};

#endif  // HAVE_FLAC_STREAM_DECODER_H
#endif  // __P_FLAC_H
