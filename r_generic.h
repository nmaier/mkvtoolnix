/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_generic.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_generic.h,v 1.3 2003/02/16 12:17:11 mosu Exp $
    \brief class definition for the generic reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
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
