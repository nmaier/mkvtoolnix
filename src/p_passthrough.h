/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_passthrough.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the pass through output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_PASSTHROUGH_H
#define __P_PASSTHROUGH_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"

class passthrough_packetizer_c: public generic_packetizer_c {
private:
  int64_t packets_processed, bytes_processed;

public:
  passthrough_packetizer_c(generic_reader_c *nreader, track_info_t *nti)
    throw (error_c);

  virtual int process(unsigned char *buf, int size, int64_t timecode = -1,
                      int64_t duration = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};

#endif // __P_PASSTHROUGH_H
