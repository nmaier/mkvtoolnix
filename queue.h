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
    \version \$Id: queue.h,v 1.15 2003/04/20 21:18:51 mosu Exp $
    \brief class definition for the queueing class
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __QUEUE_H
#define __QUEUE_H

#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

using namespace LIBMATROSKA_NAMESPACE;

typedef struct packet_t {
  DataBuffer *data_buffer;
  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, superseeded;
  int64_t timecode, bref, fref, duration;
  void *source;
} packet_t;

// q_page_t is used internally only
typedef struct q_page {
  packet_t      *pack;
  struct q_page *next;
} q_page_t;

class q_c {
private:
  static int64_t  id;
  struct q_page  *first, *current;
  
public:
  q_c();
  virtual ~q_c();
    
  virtual void add_packet(unsigned char *data, int lenth, int64_t timecode,
                          int64_t bref = -1, int64_t fref = -1,
                          int64_t duration = -1);
  virtual packet_t *get_packet();
  virtual int packet_available();
  virtual int64_t get_smallest_timecode();
    
  virtual long get_queued_bytes();
};

#endif  // __QUEUE_H
