/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  queue.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: queue.h,v 1.5 2003/02/27 09:52:37 mosu Exp $
    \brief class definition for the queueing class
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __QUEUE_H
#define __QUEUE_H

#include "error.h"
#include "pr_generic.h"

// q_page_t is used internally only
typedef struct q_page {
  packet_t      *pack;
  struct q_page *next;
} q_page_t;

class q_c: public generic_packetizer_c {
private:
  u_int64_t      id;
  struct q_page *first;
  struct q_page *current;
  
public:
  q_c() throw (error_c);
  virtual ~q_c();
    
  virtual u_int64_t        add_packet(char *data, int lenth,
                                      u_int64_t timestamp, u_int64_t ref = 0);
  virtual packet_t        *get_packet();
  virtual int              packet_available();
  virtual stamp_t          get_smallest_timestamp();
    
  virtual long             get_queued_bytes();
};

#endif  // __QUEUE_H
