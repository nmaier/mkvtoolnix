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
    \version \$Id: pr_generic.cpp,v 1.6 2003/02/28 14:50:04 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <assert.h>
#include <malloc.h>

#include "IOCallback.h"
#include "StdIOCallback.h"
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

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c() {
}

generic_reader_c::~generic_reader_c() {
}

//--------------------------------------------------------------------

//#define walk_clusters() check_clusters(__LINE__)
#define walk_clusters()

cluster_helper_c::cluster_helper_c() {
  num_clusters = 0;
  clusters = NULL;
  cluster_content_size = 0;
}

cluster_helper_c::~cluster_helper_c() {
  int i;

  for (i = 0; i < num_clusters; i++)
    free_contents(clusters[i]);

  if (clusters != NULL)
    free(clusters);
}

void cluster_helper_c::free_contents(ch_contents *clstr) {
  packet_t *p;
  int i;

  assert(clstr != NULL);
  assert(clstr->cluster != NULL);
  delete clstr->cluster;

  assert(!((clstr->num_packets != 0) && (clstr->packets == NULL)));
  for (i = 0; i < clstr->num_packets; i++) {
    p = clstr->packets[i];
    if (p->data != NULL)
      free(p->data);
    if (p->data_buffer != NULL)
      delete p->data_buffer;
//     fprintf(stdout, "* deleted %llu\n", p->id);
    free(p);
  }
  if (clstr->packets != NULL)
    free(clstr->packets);
  free(clstr);
}

KaxCluster *cluster_helper_c::get_cluster() {
  if (clusters != NULL)
    return clusters[num_clusters - 1]->cluster;
  return NULL;
}

void cluster_helper_c::add_packet(packet_t *packet) {
  ch_contents *c;

  if (clusters == NULL)
    return;

  c = clusters[num_clusters - 1];
  c->packets = (packet_t **)realloc(c->packets, sizeof(packet_t *) *
                                    (c->num_packets + 1));
  if (c->packets == NULL)
    die("realloc");

  c->packets[c->num_packets] = packet;
  if (c->num_packets == 0) {
    KaxClusterTimecode &timecode = GetChild<KaxClusterTimecode>(*c->cluster);
    *(static_cast<EbmlUInteger *>(&timecode)) = packet->timestamp;
  }

  c->num_packets++;

  cluster_content_size += packet->length;

//   fprintf(stdout, "& new %llu\n", packet->id);

  walk_clusters();
}

u_int64_t cluster_helper_c::get_timecode() {
  if (clusters == NULL)
    return 0;
  if (clusters[num_clusters - 1]->packets == NULL)
    return 0;
  return clusters[num_clusters - 1]->packets[0]->timestamp;
}

packet_t *cluster_helper_c::get_packet(int num) {
  ch_contents *c;
  if (clusters == NULL)
    return NULL;
  c = clusters[num_clusters - 1];
  if (c->packets == NULL)
    return NULL;
  if ((num < 0) || (num > c->num_packets))
    return NULL;
  return c->packets[num];
}

int cluster_helper_c::get_packet_count() {
  if (clusters == NULL)
    return -1;
  return clusters[num_clusters - 1]->num_packets;
}

int cluster_helper_c::find_cluster(KaxCluster *cluster) {
  int i;

  if (clusters == NULL)
    return -1;
  for (i = 0; i < num_clusters; i++)
    if (clusters[i]->cluster == cluster)
      return i;
  return -1;
}

void cluster_helper_c::add_cluster(KaxCluster *cluster) {
  ch_contents *c;

  if (find_cluster(cluster) != -1)
    return;
  c = (ch_contents *)malloc(sizeof(ch_contents));
  if (c == NULL)
    die("malloc");
  clusters = (ch_contents **)realloc(clusters, sizeof(ch_contents *) *
                                     (num_clusters + 1));
  if (clusters == NULL)
    die("realloc");
  memset(c, 0, sizeof(ch_contents));
  clusters[num_clusters] = c;
  num_clusters++;
  c->cluster = cluster;
  cluster_content_size = 0;
}

int cluster_helper_c::get_cluster_content_size() {
  return cluster_content_size;
}

int cluster_helper_c::render(IOCallback *out) {
  KaxCues dummy_cues;
  KaxBlockGroup *last_group = NULL;
  KaxCluster *cluster;
  int i;
  u_int64_t cluster_timecode;
  ch_contents *clstr;
  packet_t *pack, *bref_packet, *fref_packet;

  if ((clusters == NULL) || (num_clusters == 0))
    return 0;

  walk_clusters();
  clstr = clusters[num_clusters - 1];
  cluster = clstr->cluster;
  cluster_timecode = get_timecode();

  for (i = 0; i < clstr->num_packets; i++) {
    pack = clstr->packets[i];

//     if (last_group == NULL)
//       pack->group = &GetChild<KaxBlockGroup>(*cluster);
//     else
//       pack->group = &GetNextChild<KaxBlockGroup>(*cluster, *last_group);
//     last_group = pack->group;
    pack->group = &cluster->GetNewBlock();
    pack->data_buffer = new DataBuffer((binary *)pack->data, pack->length);
    KaxTrackEntry &track_entry =
      static_cast<KaxTrackEntry &>(*pack->source->track_entry);

    if (pack->bref != 0) {      // P and B frames: add backward reference.
      bref_packet = find_packet(pack->bref);
      assert(bref_packet != NULL);
      assert(bref_packet->group != NULL);
      if (pack->fref != 0) {    // It's even a B frame: add forward reference.
        fref_packet = find_packet(pack->fref);
        assert(fref_packet != NULL);
        assert(fref_packet->group != NULL);
        pack->group->AddFrame(track_entry, pack->timestamp - cluster_timecode,
                              *pack->data_buffer, *bref_packet->group,
                              *fref_packet->group);
      } else
        pack->group->AddFrame(track_entry, pack->timestamp - cluster_timecode,
                              *pack->data_buffer, *bref_packet->group);
    } else {                    // This is a key frame. No references.
      pack->group->AddFrame(track_entry, pack->timestamp - cluster_timecode,
                            *pack->data_buffer);
      // All packets with an ID smaller than this packet's ID are not
      // needed anymore. Be happy!
      free_ref(pack->id - 1, pack->source);
    }
  }

  cluster->Render(static_cast<StdIOCallback &>(*out), dummy_cues);

  for (i = 0; i < clstr->num_packets; i++) {
    pack = clstr->packets[i];
    free(pack->data);
    pack->data = NULL;
  }

  clstr->rendered = 1;

  free_clusters();

  return 1;
}

ch_contents *cluster_helper_c::find_packet_cluster(u_int64_t pid) {
  int i, k;

  if (clusters == NULL)
    return NULL;

  for (i = 0; i < num_clusters; i++)
    for (k = 0; k < clusters[i]->num_packets; k++)
      if (clusters[i]->packets[k]->id == pid)
        return clusters[i];

  return NULL;
}

packet_t *cluster_helper_c::find_packet(u_int64_t pid) {
  int i, k;

  if (clusters == NULL)
    return NULL;

  for (i = 0; i < num_clusters; i++)
    for (k = 0; k < clusters[i]->num_packets; k++)
      if (clusters[i]->packets[k]->id == pid)
        return clusters[i]->packets[k];

  return NULL;
}

void cluster_helper_c::check_clusters(int num) {
  int i, k;
  packet_t *p;
  ch_contents *clstr;

  for (i = 0; i < num_clusters; i++) {
    for (k = 0; k < clusters[i]->num_packets; k++) {
      p = clusters[i]->packets[k];
      if (p->bref == 0)
        continue;
      clstr = find_packet_cluster(p->bref);
      if (clstr == NULL) {
        fprintf(stderr, "Error: backward refenrece could not be resolved "
                "(%llu). Called from %d.\n", p->bref, num);
        die("internal error");
      }
      clstr->is_referenced = 1;
    }
  }
}

int cluster_helper_c::free_clusters() {
  int i, k, idx; //, prior;
  packet_t *p;
  ch_contents *clstr, **new_clusters;

  if (clusters == NULL)
    return 0;

//   prior = num_clusters;

  for (i = 0; i < num_clusters; i++)
    clusters[i]->is_referenced = 0;

  // Part 2 - Mark all clusters that are still referenced.
  for (i = 0; i < num_clusters; i++) {
    for (k = 0; k < clusters[i]->num_packets; k++) {
      p = clusters[i]->packets[k];
      if (!p->superseeded) {
        clusters[i]->is_referenced = 1;
        if (p->bref == 0)
          continue;
        clstr = find_packet_cluster(p->bref);
        if (clstr == NULL) {
          fprintf(stderr, "Error: backward refenrece could not be resolved "
                  "(%llu).\n", p->bref);
          die("internal error");
        }
        clstr->is_referenced = 1;
      }
    }
  }

  // Part 3 - remove all clusters and the data belonging to them that
  // are not referenced anymore and that have already been rendered.
  // Also count the number of clusters that are still referenced.
  k = 0;
  for (i = 0; i < num_clusters; i++) {
    if (!clusters[i]->rendered) {
      k++;
      continue;
    }

    if (!clusters[i]->is_referenced) {
      free_contents(clusters[i]);
      clusters[i] = NULL;
    } else
      k++;
  }

  // Part 4 - prune the cluster list and remove all the entries freed in
  // part 3.
  if (k == 0) {
    free(clusters);
    num_clusters = 0;
    add_cluster(new KaxCluster());
  } else if (k != num_clusters) {
    new_clusters = (ch_contents **)malloc(sizeof(ch_contents *) * k);
    if (new_clusters == NULL)
      die("malloc");

    idx = 0;
    for (i = 0; i < num_clusters; i++)
      if (clusters[i] != NULL) {
        new_clusters[idx] = clusters[i];
        idx++;
      }

    free(clusters);
    clusters = new_clusters;
    num_clusters = k;
  }

//   fprintf(stdout, "freed %d of %d clusters; new %d\n", prior - num_clusters,
//           prior, num_clusters);

  return 1;
}

int cluster_helper_c::free_ref(u_int64_t pid, void *source) {
  int i, k;
  packet_t *p;

  for (i = 0; i < num_clusters; i++)
    for (k = 0; k < clusters[i]->num_packets; k++) {
      p = clusters[i]->packets[k];
      if ((source == p->source) && (p->id <= pid))
        p->superseeded = 1;
    }

//   free_clusters();

  return 1;
}
