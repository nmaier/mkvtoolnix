/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_ac3.h,v 1.1 2003/02/23 22:51:49 mosu Exp $
    \brief class definition for the MP3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_AC3_H
#define __P_AC3_H

#include "common.h"
#include "queue.h"
#include "ac3_common.h"

class ac3_packetizer_c: public q_c {
  private:
    int                 bps;
    u_int64_t           bytes_output, packetno;
    unsigned long       samples_per_sec;
    int                 channels;
    int                 bitrate;
    audio_sync_t        async;
    range_t             range;
    char               *packet_buffer;
    int                 buffer_size;

  public:
    ac3_packetizer_c(void *nprivate_data, int nprivate_size,
                     unsigned long nsamples_per_sec, int nchannels,
                     int nbitrate, audio_sync_t *nasync,
                     range_t *nrange) throw (error_c);
    virtual ~ac3_packetizer_c();
    
    virtual int     process(char *buf, int size, int last_frame);
    virtual void    set_header();
/*     virtual void    reset(); */
    
    virtual void    set_params(unsigned long nsamples_per_sec, int nchannels,
                               int nbitrate);
  private:
    virtual void    add_to_buffer(char *buf, int size);
    virtual char   *get_ac3_packet(unsigned long *header,
                                   ac3_header_t *ac3header);
    virtual int     ac3_packet_available();
    virtual void    remove_ac3_packet(int pos, int framesize);
};

#endif // __P_AC3_h
