/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the VobBtn output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VOBBTN_H
#define __P_VOBBTN_H

#include "os.h"

#include "common.h"
#include "compression.h"
#include "pr_generic.h"

class vobbtn_packetizer_c: public generic_packetizer_c {
protected:
  int64_t previous_timecode;
  int width, height;

public:
  vobbtn_packetizer_c(generic_reader_c *_reader, int _width, int _height,
                      track_info_c &_ti) throw (error_c);
  virtual ~vobbtn_packetizer_c();

  virtual int process(memory_c &mem,
                      int64_t old_timecode = -1, int64_t duration = -1,
                      int64_t bref = -1, int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "VobBtn";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);
};

#endif // __P_VOBBTN_H
