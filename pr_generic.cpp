/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: pr_generic.cpp,v 1.3 2003/02/27 09:35:55 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <malloc.h>

#include "KaxCluster.h"
#include "KaxClusterData.h"

#include "pr_generic.h"

generic_packetizer_c::generic_packetizer_c() {
  serialno = -1;
  track_entry = NULL;
  private_data = NULL;
  private_data_size = 0;
}

generic_packetizer_c::~generic_packetizer_c() {
  if (private_data != NULL)
    free(private_data);
}

void generic_packetizer_c::set_private_data(void *data, int size) {
  if (private_data != NULL)
    free(private_data);
  private_data = malloc(size);
  if (private_data == NULL)
    die("malloc");
  memcpy(private_data, data, size);
  private_data_size = size;
}

void generic_packetizer_c::added_packet_to_cluster(packet_t *,
                                                   cluster_helper_c *) {
}

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c() {
}

generic_reader_c::~generic_reader_c() {
}

//--------------------------------------------------------------------

cluster_helper_c::cluster_helper_c(KaxCluster *ncluster) {
  num_packets = 0;
  packet_q = NULL;
  refcount = 1;
  if (ncluster == NULL)
    cluster = new KaxCluster();
  else
    cluster = ncluster;
}

cluster_helper_c::~cluster_helper_c() {
  int i;

  if (packet_q) {
    for (i = 0; i < num_packets; i++) {
      free(packet_q[i]->data);
      delete packet_q[i]->data_buffer;
      free(packet_q[i]);
    }
    free(packet_q);
  }
  if (cluster)
    delete cluster;
}

KaxCluster *cluster_helper_c::get_cluster() {
  return cluster;
}

KaxCluster &cluster_helper_c::operator *() {
  return *cluster;
}

int cluster_helper_c::add_ref() {
  refcount++;
  return refcount;
}

int cluster_helper_c::release() {
  int ref;
  refcount--;
  ref = refcount;
  if (refcount == 0)
    delete this;
  return ref;
}

void cluster_helper_c::add_packet(packet_t *packet) {
  packet_q = (packet_t **)realloc(packet_q, sizeof(packet_t *) *
                                  (num_packets + 1));
  if (packet_q == NULL)
    die("realloc");
  packet_q[num_packets] = packet;
  if (num_packets == 0) {
    KaxClusterTimecode &timecode = GetChild<KaxClusterTimecode>(*cluster);
    *(static_cast<EbmlUInteger *>(&timecode)) = packet->timestamp;
  }
  num_packets++;
}

u_int64_t cluster_helper_c::get_timecode() {
  if (packet_q == NULL)
    return 0;
  return packet_q[0]->timestamp;
}

packet_t *cluster_helper_c::get_packet(int num) {
  if (packet_q == NULL)
    return NULL;
  if ((num < 0) || (num > num_packets))
    return NULL;
  return packet_q[num];
}

int cluster_helper_c::get_packet_count() {
  return num_packets;
}
