/*
  ogmmerge -- utility for splicing together ogg bitstreams
  from component media subtypes

  r_ac3.cpp
  AC3 demultiplexer module

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/
#ifndef __R_AC3_H
#define __R_AC3_H

#include <stdio.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"
#include "queue.h"

#include "p_ac3.h"
#include "ac3_common.h"

class ac3_reader_c: public generic_reader_c {
  private:
    unsigned char          *chunk;
    FILE                   *file;
    class ac3_packetizer_c *ac3packetizer;
    u_int64_t               bytes_processed;
    u_int64_t               size;
     
  public:
    ac3_reader_c(char *fname, audio_sync_t *nasync, range_t *nrange,
                 char **ncomments) throw (error_c);
    virtual ~ac3_reader_c();

    virtual int              read();
    virtual int              serial_in_use(int);
    virtual ogmmerge_page_t *get_page();
    virtual ogmmerge_page_t *get_header_page(int header_type =
                                             PACKET_TYPE_HEADER);
    virtual void             reset();
    virtual int              display_priority();
    virtual void             display_progress();

    static int               probe_file(FILE *file, u_int64_t size);
};

#endif
