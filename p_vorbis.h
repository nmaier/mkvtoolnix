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
    \version \$Id: p_vorbis.h,v 1.10 2003/05/02 20:11:34 mosu Exp $
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
#include "pr_generic.h"

class vorbis_packetizer_c: public generic_packetizer_c {
private:
  int64_t last_bs, samples;
  int packetno;
  audio_sync_t async;
  vorbis_info vi;
  vorbis_comment vc;
  ogg_packet headers[3];
public:
  vorbis_packetizer_c(unsigned char *d_header, int l_header,
                      unsigned char *d_comments, int l_comments,
                      unsigned char *d_codecsetup, int l_codecsetup,
                      track_info_t *nti) throw (error_c);
  virtual ~vorbis_packetizer_c();
    
  virtual int process(unsigned char *data, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_header();
};


#endif  // __P_VORBIS_H

#endif // HAVE_OGGVORBIS
