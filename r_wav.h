
/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_wav.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_wav.h,v 1.7 2003/04/13 15:23:03 mosu Exp $
    \brief class definitions for the WAV reader module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __R_WAV_H
#define __R_WAV_H

#include <stdio.h>

#include "common.h"
#include "error.h"
#include "queue.h"

#include "p_pcm.h"

extern "C" {
#include "avilib.h"
}

class wav_reader_c: public generic_reader_c {
private:
  unsigned char          *chunk;
  FILE                   *file;
  class pcm_packetizer_c *pcmpacketizer;
  int                     bps;
  struct wave_header      wheader;
  int64_t                 bytes_processed;
     
public:
  wav_reader_c(track_info_t *nti) throw (error_c);
  virtual ~wav_reader_c();

  virtual int       read();
  virtual packet_t *get_packet();
    
  virtual int       display_priority();
  virtual void      display_progress();

  static int        probe_file(FILE *file, int64_t size);
};

#endif // __R_WAV_H
