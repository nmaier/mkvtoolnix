/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the cluster helper

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __CLUSTER_HELPER_C
#define __CLUSTER_HELPER_C

#include <vector>

#include "os.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>

#include "mm_io.h"
#include "pr_generic.h"

using namespace std;

#define RND_TIMECODE_SCALE(a) (irnd((double)(a) / \
                                    (double)((int64_t)timecode_scale)) * \
                               (int64_t)timecode_scale)

struct ch_contents_t {
  KaxCluster *cluster;
  vector<packet_t *> packets;
  bool is_referenced, rendered;

  ch_contents_t():
    cluster(NULL),
    is_referenced(false),
    rendered(false) {
  }
};

typedef struct {
  vector<KaxBlockGroup *> groups;
  vector<int64_t> durations;
  generic_packetizer_c *source;
  bool more_data, duration_mandatory;
} render_groups_t;

class cluster_helper_c {
private:
  vector<ch_contents_t *> clusters;
  int cluster_content_size;
  int64_t max_timecode_and_duration;
  int64_t last_cluster_tc, num_cue_elements, header_overhead;
  int64_t packet_num, timecode_offset, *last_packets, first_timecode;
  int64_t bytes_in_file, first_timecode_in_file;
  mm_io_c *out;

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
  int render(bool flush = false);
  int free_ref(int64_t ref_timecode, generic_packetizer_c *source);
  int free_clusters();
  int get_cluster_content_size();
  int64_t get_duration();
  int64_t get_first_timecode();
  int64_t get_timecode_offset();

private:
  int find_cluster(KaxCluster *cluster);
  ch_contents_t *find_packet_cluster(int64_t ref_timecode,
                                     generic_packetizer_c *source);
  packet_t *find_packet(int64_t ref_timecode,
                        generic_packetizer_c *source);
  void free_contents(ch_contents_t *clstr);
  void check_clusters(int num);
  bool all_references_resolved(ch_contents_t *cluster);
  void set_duration(render_groups_t *rg);
  int render_cluster(ch_contents_t *clstr);
};

extern cluster_helper_c *cluster_helper;

#endif // __CLUSTER_HELPER_C
