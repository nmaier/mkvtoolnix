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
 * class definition for the subtitle helper
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __SUBTITLES_H
#define __SUBTITLES_H

#include "os.h"

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

  void add(int64_t, int64_t, const char *);
  int check();
  void process(textsubs_packetizer_c *);
  sub_t *get_next();
};

int64_t spu_extract_duration(unsigned char *data, int buf_size,
                             int64_t timecode);

#endif // __SUBTITLES_H
