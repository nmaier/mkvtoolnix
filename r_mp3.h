/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_mp3.h,v 1.2 2003/02/23 23:23:10 mosu Exp $
    \brief class definitions for the MP3 reader module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __R_MP3_H__
#define __R_MP3_H__

#include <stdio.h>

#include "common.h"
#include "error.h"
#include "queue.h"

#include "p_mp3.h"

class mp3_reader_c: public generic_reader_c {
  private:
    unsigned char          *chunk;
    FILE                   *file;
    class mp3_packetizer_c *mp3packetizer;
    u_int64_t               bytes_processed;
    u_int64_t               size;
     
  public:
    mp3_reader_c(char *fname, audio_sync_t *nasync, range_t *nrange)
      throw (error_c);
    virtual ~mp3_reader_c();

    virtual int              read();
    virtual packet_t        *get_packet();

/*     virtual void             reset(); */
    virtual int              display_priority();
    virtual void             display_progress();

    static int               probe_file(FILE *file, u_int64_t size);
};

#endif  /* __R_MP3_H__*/
