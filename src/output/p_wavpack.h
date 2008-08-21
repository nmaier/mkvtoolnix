/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the TTA output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __P_WAVPACK_H
#define __P_WAVPACK_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"
#include "wavpack_common.h"

class wavpack_packetizer_c: public generic_packetizer_c {
private:
  int channels, sample_rate, bits_per_sample;
  int64_t samples_per_block;
  int64_t samples_output;
  bool has_correction;

public:
  wavpack_packetizer_c(generic_reader_c *_reader, wavpack_meta_t & meta,
                       track_info_c &_ti)
    throw (error_c);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "WAVPACK4";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);
};

#endif // __P_WAVPACK_H
