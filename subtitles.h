/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  subtitles.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: subtitles.h,v 1.3 2003/05/18 20:57:08 mosu Exp $
    \brief class definition for the subtitle helper
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __SUBTITLES_H
#define __SUBTITLES_H


#include "p_textsubs.h"

typedef struct sub_t {
  int64_t start, end;
  char *subs;
  sub_t *next;
} sub_t;

class subtitles_c {
private:
  sub_t *first, *last;
public:
  subtitles_c();
  ~subtitles_c();
  
  void add(int64_t, int64_t, char *);
  int check();
  void process(textsubs_packetizer_c *);
  sub_t *get_next();
};

#endif // __SUBTITLES_H
