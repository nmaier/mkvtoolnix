/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the simple text subtitle packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_TEXTSUBS_H
#define __P_TEXTSUBS_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"
#include "smart_pointers.h"

class textsubs_packetizer_c: public generic_packetizer_c {
private:
  int packetno, cc_utf8;
  autofree_ptr<unsigned char> global_data;
  int global_size;
  string codec_id;
  bool recode;

public:
  textsubs_packetizer_c(generic_reader_c *_reader, const char *_codec_id,
                        const void *_global_data, int _global_size,
                        bool _recode, bool is_utf8, track_info_c &_ti)
    throw (error_c);
  virtual ~textsubs_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "text subtitle";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);
};


#endif  // __P_TEXTSUBS_H
