/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  cluster_helper.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: cluster_helper.h,v 1.2 2003/04/18 13:08:04 mosu Exp $
    \brief class definition for the cluster helper
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __CLUSTER_HELPER_C
#define __CLUSTER_HELPER_C

#include "KaxBlock.h"
#include "KaxCluster.h"

#include "pr_generic.h"

typedef struct {
  KaxCluster  *cluster;
  packet_t   **packets;
  int          num_packets, is_referenced, rendered;
} ch_contents_t;

class cluster_helper_c {
private:
  ch_contents_t **clusters;
  int             num_clusters, cluster_content_size;
  KaxBlockGroup  *last_block_group;
  int64_t         max_timecode;
public:
  cluster_helper_c();
  virtual ~cluster_helper_c();

  void           add_cluster(KaxCluster *cluster);
  KaxCluster    *get_cluster();
  void           add_packet(packet_t *packet);
  int64_t        get_timecode();
  packet_t      *get_packet(int num);
  int            get_packet_count();
  int            render(IOCallback *out);
  int            free_ref(int64_t ref_timecode, void *source);
  int            free_clusters();
  int            get_cluster_content_size();
  int64_t        get_max_timecode();

private:
  int            find_cluster(KaxCluster *cluster);
  ch_contents_t *find_packet_cluster(int64_t ref_timecode);
  packet_t      *find_packet(int64_t ref_timecode);
  void           free_contents(ch_contents_t *clstr);
  void           check_clusters(int num);
};

extern cluster_helper_c *cluster_helper;

#endif // __CLUSTER_HELPER_C
