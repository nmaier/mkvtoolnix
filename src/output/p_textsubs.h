/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_textsubs.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the simple text subtitle packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_TEXTSUBS_H
#define __P_TEXTSUBS_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"
#include "pr_generic.h"

class textsubs_packetizer_c: public generic_packetizer_c {
private:
  int packetno, cc_utf8;
  void *global_data;
  int global_size;
  char *codec_id;
  bool recode;

public:
  textsubs_packetizer_c(generic_reader_c *nreader, const char *ncodec_id,
                        const void *nglobal_data, int nglobal_size,
                        bool nrecode, bool is_utf8, track_info_c *nti)
    throw (error_c);
  virtual ~textsubs_packetizer_c();

  virtual int  process(memory_c &mem, int64_t start = -1,
                       int64_t length = -1, int64_t bref = -1,
                       int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};


#endif  // __P_TEXTSUBS_H
