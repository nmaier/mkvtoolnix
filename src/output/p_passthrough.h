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
 * class definition for the pass through output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __P_PASSTHROUGH_H
#define __P_PASSTHROUGH_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"

class passthrough_packetizer_c: public generic_packetizer_c {
private:
  int64_t packets_processed, bytes_processed;
  bool sync_to_keyframe, sync_complete_group;

public:
  passthrough_packetizer_c(generic_reader_c *nreader, track_info_c *nti)
    throw (error_c);

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t duration = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual int process(memory_c &mem, int64_t timecode,
                      int64_t duration, int64_t bref, int64_t fref,
                      bool duration_mandatory);
  virtual void set_headers();
  virtual void always_sync_complete_group(bool sync);

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "passthrough";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);
};

#endif // __P_PASSTHROUGH_H
