/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_avi.h
    \version \$Id: r_aac.h,v 1.4 2003/05/23 06:34:57 mosu Exp $
    \brief class definitions for the AVI demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_AAC_H
#define __R_AAC_H

#include <stdio.h>

#include "mm_io.h"
#include "pr_generic.h"
#include "common.h"
#include "error.h"
#include "p_aac.h"
#include "aac_common.h"

class aac_reader_c: public generic_reader_c {
private:
  unsigned char *chunk;
  mm_io_c *mm_io;
  aac_packetizer_c *aacpacketizer;
  int64_t bytes_processed, size;

public:
  aac_reader_c(track_info_t *nti) throw (error_c);
  virtual ~aac_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_AAC_H
