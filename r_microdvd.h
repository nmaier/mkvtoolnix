/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_microdvd.h
  class definitions for the MicroDVD text subtitle reader

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __R_MICRODVD_H
#define __R_MICRODVD_H

#include <stdio.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"
#include "queue.h"

#include "p_textsubs.h"

class microdvd_reader_c: public generic_reader_c {
private:
  char chunk[2048];
  FILE *file;
  textsubs_packetizer_c *textsubspacketizer;
  int act_wchar;
     
public:
  microdvd_reader_c(char *fname, audio_sync_t *nasync) throw (error_c);
  virtual ~microdvd_reader_c();
  
  virtual int read();
  virtual int serial_in_use(int);
  virtual ogmmerge_page_t *get_page();
  virtual ogmmerge_page_t *get_header_page(int header_type =
                                           PACKET_TYPE_HEADER);
    
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();

  static int probe_file(FILE *file, int64_t size);
};

#endif  // __R_MICRODVD_H 
