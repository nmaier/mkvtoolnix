/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_srt.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_srt.h,v 1.11 2003/05/20 06:30:25 mosu Exp $
    \brief class definition for the Subripper subtitle reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_SRT_H
#define __R_SRT_H

#include <stdio.h>

#include "common.h"
#include "pr_generic.h"

#include "p_textsubs.h"

class srt_reader_c: public generic_reader_c {
private:
  char chunk[2048];
  FILE *file;
  textsubs_packetizer_c *textsubs_packetizer;
  int act_wchar;

public:
  srt_reader_c(track_info_t *nti) throw (error_c);
  virtual ~srt_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual void set_headers();

  virtual int display_priority();
  virtual void display_progress();

  static int probe_file(FILE *file, int64_t size);
};

#endif  // __R_SRT_H
