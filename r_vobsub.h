
/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_vobsub.h
  class definitions for the VobSub subtitle reader

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __R_VOBSUB_H
#define __R_VOBSUB_H

#include <stdio.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"

#include "p_vobsub.h"

class vobsub_reader_c: public generic_reader_c {
private:
  char chunk[2048];
  mm_io_c *mm_io, *subfile;
  vobsub_packetizer_c  *vobsub_packetizer, **all_packetizers;
  int num_packetizers, act_wchar;
  char **comments;

public:
  vobsub_reader_c(char *fname, track_info_t *nti) throw (error_c);
  virtual ~vobsub_reader_c();

  virtual int read();
  virtual int serial_in_use(int);
  virtual ogmmerge_page_t *get_page();
  virtual ogmmerge_page_t *get_header_page(int header_type =
                                           PACKET_TYPE_HEADER);

  virtual void reset();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);

private:
  virtual void add_vobsub_packetizer(int width, int height,
                                     char *palette, int langidx,
                                     char *id, int index);
};

#endif  // __R_VOBSUB_H
