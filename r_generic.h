/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_generic.h
  class definitions for the generic reader

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __R_GENERIC_H__
#define __R_GENERIC_H__

#include <stdio.h>

typedef struct {
  char      *data;
  int        length;
  u_int64_t  timestamp;
  int        is_key;
} packet_t;

typedef class generic_reader_c {
//  protected:
//    vorbis_comment *chapter_info;
  public:
    generic_reader_c() {};
    virtual ~generic_reader_c() {};
    virtual int              read() = 0;
    virtual packet_t        *get_packet() = 0;
    virtual int              display_priority() = 0;
    virtual void             display_progress() = 0;
//    virtual void             set_chapter_info(vorbis_comment *info);
} generic_reader_c;

#endif  /* __R_GENERIC_H__ */
