/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  queue.cpp
  class definitions for the queueing class used by every packetizer

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "error.h"
#include "r_generic.h"
#include "p_generic.h"

// q_page_t is used internally only
typedef struct q_page {
  packet_t      *pack;
  struct q_page *next;
} q_page_t;

class q_c: public generic_packetizer_c {
  private:
    struct q_page    *first;
    struct q_page    *current;
    
  public:
    q_c() throw (error_c);
    virtual ~q_c();
    
    virtual int              add_packet(char *data, int lenth,
                                        u_int64_t timestamp, int is_key);
    virtual packet_t        *get_packet();
    virtual int              packet_available();
    virtual stamp_t          get_smallest_timestamp();
    
    virtual long             get_queued_bytes();
};

#endif  /* __QUEUE_H__ */
