/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   class definitions for the Vob Buttons stream reader
  
   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_VOBBTN_H
#define __R_VOBBTN_H

#include "os.h"

#include <stdio.h>

#include "common.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "p_vobbtn.h"

class vobbtn_reader_c: public generic_reader_c {
private:
  mm_io_c *btn_file;
  uint32_t size;
  uint16_t width, height;
  unsigned char chunk[0x400];

public:
  vobbtn_reader_c(track_info_c *nti) throw (error_c);
  virtual ~vobbtn_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif  // __R_VOBBTN_H
