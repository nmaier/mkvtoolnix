/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   class definition for the subtitle helper
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __SUBTITLES_H
#define __SUBTITLES_H

#include "os.h"

#include "p_textsubs.h"

typedef struct sub_t {
  int64_t start, end;
  string subs;

  sub_t(int64_t _start, int64_t _end, const string &_subs):
    start(_start), end(_end), subs(_subs) {
  }

  bool operator < (const sub_t &cmp) const {
    return start < cmp.start;
  }
} sub_t;

class subtitles_c {
private:
  deque<sub_t> entries;
  deque<sub_t>::iterator current;

public:
  subtitles_c() {
    current = entries.end();
  }
  void add(int64_t start, int64_t end, const string &subs) {
    entries.push_back(sub_t(start, end, subs));
  }
  void reset() {
    current = entries.begin();
  }
  int get_num_entries() {
    return entries.size();
  }
  int get_num_processed() {
    return std::distance(entries.begin(), current);
  }
  void process(textsubs_packetizer_c *);
  void sort() {
    std::stable_sort(entries.begin(), entries.end());
    reset();
  }
  bool empty() {
    return current == entries.end();
  }
};

int64_t spu_extract_duration(unsigned char *data, int buf_size,
                             int64_t timecode);

#endif // __SUBTITLES_H
