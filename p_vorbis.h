/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vorbis.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_vorbis.h,v 1.3 2003/03/04 09:27:05 mosu Exp $
    \brief class definition for the Vorbis packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include "config.h"

#ifdef HAVE_OGGVORBIS

#ifndef __P_VORBIS_H
#define __P_VORBIS_H

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "pr_generic.h"
#include "queue.h"

class vorbis_packetizer_c: public q_c {
private:
  u_int64_t       last_bs, samples;
  int             packetno;
  audio_sync_t    async;
  vorbis_info     vi;
  vorbis_comment  vc;
  ogg_packet      headers[3];
public:
  vorbis_packetizer_c(audio_sync_t *nasync,
                      void *d_header, int l_header, void *d_comments,
                      int l_comments, void *d_codecsetup, int l_codecsetup)
    throw (error_c);
  virtual ~vorbis_packetizer_c();
    
  virtual int   process(char *data, int size, int64_t timecode);
  virtual void  set_header();
  
private:
  virtual void  setup_displacement();
  virtual int   encode_silence(int fd);
};


#endif  // __P_VORBIS_H

#endif // HAVE_OGGVORBIS
