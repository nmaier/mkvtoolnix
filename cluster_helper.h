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
    \version \$Id$
    \brief class definition for the cluster helper
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __CLUSTER_HELPER_C
#define __CLUSTER_HELPER_C

#include "KaxBlock.h"
#include "KaxCluster.h"

#include "mm_io.h"
#include "pr_generic.h"

typedef struct {
  KaxCluster *cluster;
  packet_t **packets;
  int num_packets, is_referenced, rendered;
} ch_contents_t;

typedef struct {
  int64_t timecode, filepos, cues_size, packet_num;
  int64_t *last_packets;
} splitpoint_t;

class cluster_helper_c {
private:
  ch_contents_t **clusters;
  int num_clusters, cluster_content_size, next_splitpoint;
  KaxBlockGroup *last_block_group;
  int64_t max_timecode, last_cluster_tc, num_cue_elements, header_overhead;
  int64_t packet_num, timecode_offset, *last_packets, first_timecode;
  mm_io_c *out;
public:
  static vector<splitpoint_t *> splitpoints;

public:
  cluster_helper_c();
  virtual ~cluster_helper_c();

  void set_output(mm_io_c *nout);
  void add_cluster(KaxCluster *cluster);
  KaxCluster *get_cluster();
  void add_packet(packet_t *packet);
  int64_t get_timecode();
  packet_t *get_packet(int num);
  int get_packet_count();
  int render();
  int free_ref(int64_t ref_timecode, void *source);
  int free_clusters();
  int get_cluster_content_size();
  int64_t get_max_timecode();
  int64_t get_first_timecode();
  void find_next_splitpoint();
  int get_next_splitpoint();

private:
  int find_cluster(KaxCluster *cluster);
  ch_contents_t *find_packet_cluster(int64_t ref_timecode);
  packet_t *find_packet(int64_t ref_timecode);
  void free_contents(ch_contents_t *clstr);
  void check_clusters(int num);
};

extern cluster_helper_c *cluster_helper;

#endif // __CLUSTER_HELPER_C
