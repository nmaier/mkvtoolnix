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
    \version \$Id: p_textsubs.h,v 1.1 2003/03/06 23:39:40 mosu Exp $
    \brief class definition for the simple text subtitle packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __P_TEXTSUBS_H
#define __P_TEXTSUBS_H

#include "common.h"
#include "pr_generic.h"
#include "queue.h"

class textsubs_packetizer_c: public q_c {
private:
  int packetno;

public:
  textsubs_packetizer_c(track_info_t *nti) throw (error_c);
  virtual ~textsubs_packetizer_c();
    
  virtual int  process(int64_t start, int64_t end, char *_subs);
  virtual void set_header();
};


#endif  // __P_TEXTSUBS_H
