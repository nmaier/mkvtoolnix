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
    \version \$Id: pr_generic.h,v 1.10 2003/03/01 16:49:28 mosu Exp $
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include <stdio.h>

#include "IOCallback.h"
#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

#include "common.h"

using namespace LIBMATROSKA_NAMESPACE;

extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern int track_number;

struct packet_t;

typedef struct {
  KaxCluster  *cluster;
  packet_t   **packets;
  int          num_packets, is_referenced, rendered;
} ch_contents_t;

class cluster_helper_c {
private:
  ch_contents_t **clusters;
  int             num_clusters, cluster_content_size;
public:
  cluster_helper_c();
  virtual ~cluster_helper_c();

  void           add_cluster(KaxCluster *cluster);
  KaxCluster    *get_cluster();
  void           add_packet(packet_t *packet);
  u_int64_t      get_timecode();
  packet_t      *get_packet(int num);
  int            get_packet_count();
  int            render(IOCallback *out);
  int            free_ref(u_int64_t pid, void *source);
  int            free_clusters();
  int            get_cluster_content_size();

private:
  int            find_cluster(KaxCluster *cluster);
  ch_contents_t *find_packet_cluster(u_int64_t pid);
  packet_t      *find_packet(u_int64_t pid);
  void           free_contents(ch_contents_t *clstr);
  void           check_clusters(int num);
};

extern cluster_helper_c *cluster_helper;

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
  int                  length, superseeded;
  u_int64_t            timestamp, id, bref, fref;
  generic_packetizer_c *source;
} packet_t;

#endif  // __PR_GENERIC_H
