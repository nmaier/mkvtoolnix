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
    \version \$Id: p_textsubs.h,v 1.10 2003/05/25 15:35:39 mosu Exp $
    \brief class definition for the simple text subtitle packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_TEXTSUBS_H
#define __P_TEXTSUBS_H

#include "common.h"
#include "pr_generic.h"
#include "pr_generic.h"

class textsubs_packetizer_c: public generic_packetizer_c {
private:
  int packetno, cc_utf8;

public:
  textsubs_packetizer_c(generic_reader_c *nreader, track_info_t *nti)
    throw (error_c);
  virtual ~textsubs_packetizer_c();

  virtual int  process(unsigned char *_subs, int size, int64_t start = -1,
                       int64_t length = -1, int64_t bref = -1,
                       int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};


#endif  // __P_TEXTSUBS_H
