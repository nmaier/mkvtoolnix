/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: pr_generic.h,v 1.6 2003/02/27 09:52:37 mosu Exp $
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include <stdio.h>

#include "common.h"
#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

using namespace LIBMATROSKA_NAMESPACE;

extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern int track_number;

struct packet_t;

class cluster_helper_c {
private:
  int          refcount;
  KaxCluster  *cluster;
  packet_t   **packet_q;
  int          num_packets;
public:
  cluster_helper_c(KaxCluster *ncluster = NULL);
  virtual ~cluster_helper_c();

  KaxCluster *get_cluster();
  int         add_ref();
  void        add_packet(packet_t *packet);
  u_int64_t   get_timecode();
  packet_t   *get_packet(int num);
  int         get_packet_count();
  int         release();
  KaxCluster &operator *();
};

class generic_packetizer_c {
protected:
  int serialno;
  void *private_data;
  int private_data_size;
public:
  KaxTrackEntry *track_entry;

  generic_packetizer_c();
  virtual ~generic_packetizer_c();
  virtual int       packet_available() = 0;
  virtual packet_t *get_packet() = 0;
  virtual void      set_header() = 0;
  virtual stamp_t   get_smallest_timestamp() = 0;
  virtual void      set_private_data(void *data, int size);
  virtual void      added_packet_to_cluster(packet_t *packet,
                                            cluster_helper_c *helper);
};
 
class generic_reader_c {
public:
  generic_reader_c();
  virtual ~generic_reader_c();
  virtual int       read() = 0;
  virtual packet_t *get_packet() = 0;
  virtual int       display_priority() = 0;
  virtual void      display_progress() = 0;
};

typedef struct packet_t {
  DataBuffer          *data_buffer;
  KaxBlockGroup       *group;
  KaxBlock            *block;
  KaxCluster          *cluster;
  char                *data;
  int                  length;
  u_int64_t            timestamp, id, ref;
  generic_packetizer_c *source;
} packet_t;

#endif  // __PR_GENERIC_H
