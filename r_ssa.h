/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ssa.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the SSA/ASS subtitle parser
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_SSA_H
#define __R_SSA_H

#include "os.h"

#include <stdio.h>

#include "mm_io.h"
#include "common.h"
#include "pr_generic.h"

#include "p_textsubs.h"

class ssa_reader_c: public generic_reader_c {
private:
  mm_io_c *mm_io;
  textsubs_packetizer_c *textsubs_packetizer;
  int act_wchar;

public:
  ssa_reader_c(track_info_t *nti) throw (error_c);
  virtual ~ssa_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual void set_headers();
  virtual void identify();

  virtual int display_priority();
  virtual void display_progress();

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  virtual int64_t parse_time(string &time);
};

#endif  // __R_SSA_H
