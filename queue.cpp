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
    \version \$Id: queue.cpp,v 1.11 2003/04/17 12:29:08 mosu Exp $
    \brief packet queueing class used by every packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>

#include "common.h"
#include "queue.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int64_t q_c::id = 1;

q_c::q_c(track_info_t *nti) throw (error_c) : generic_packetizer_c(nti) {
  first = NULL;
  current = NULL;
}

q_c::~q_c() {
  q_page_t *qpage, *tmppage;

  qpage = first;
  while (qpage) {
    if (qpage->pack != NULL) {
      if (qpage->pack->data != NULL)
        free(qpage->pack->data);
      free(qpage->pack);
    }
    tmppage = qpage->next;
    free(qpage);
    qpage = tmppage;
  }
}

int64_t q_c::add_packet(unsigned char  *data, int length,
                        int64_t timestamp, int64_t bref,
                        int64_t fref) {
  q_page_t *qpage;
  
  if (data == NULL)
    return 0;
  if (timestamp < 0)
    die("timecode < 0");
  qpage = (q_page_t *)malloc(sizeof(q_page_t));
  if (qpage == NULL)
    die("malloc");
  qpage->pack = (packet_t *)malloc(sizeof(packet_t));
  if (qpage->pack == NULL)
    die("malloc");
  memset(qpage->pack, 0, sizeof(packet_t));
  qpage->pack->data = (unsigned char *)malloc(length);
  if (qpage->pack->data == NULL)
    die("malloc");
  memcpy(qpage->pack->data, data, length);
  qpage->pack->length = length;
  qpage->pack->timestamp = timestamp;
  qpage->pack->bref = bref;
  qpage->pack->fref = fref;
  qpage->pack->source = this;
  qpage->pack->id = id;
  id++;
  qpage->next = NULL;
  if (current != NULL)
    current->next = qpage;
  if (first == NULL)
    first = qpage;
  current = qpage;

  return id - 1;
}

packet_t *q_c::get_packet() {
  packet_t *pack;
  q_page_t *qpage;

  if (first && first->pack) {
    pack = first->pack;
    qpage = first->next;
    if (qpage == NULL)
      current = NULL;
    free(first);
    first = qpage;
    return pack;
  }
  
  return NULL;
}

int q_c::packet_available() {
  if ((first == NULL) || (first->pack == NULL))
    return 0;
  else
    return 1;
}

stamp_t q_c::get_smallest_timestamp() {
  if (first != NULL)
    return first->pack->timestamp;
  else
    return MAX_TIMESTAMP;
}

long q_c::get_queued_bytes() {
  long      bytes;
  q_page_t *cur;
  
  bytes = 0;
  cur = first;
  while (cur != NULL) {
    if (cur->pack != NULL)
      bytes += cur->pack->length;
    cur = cur->next;
  }
  
  return bytes;
}
