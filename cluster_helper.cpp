/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  cluster_helper.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: cluster_helper.cpp,v 1.5 2003/04/18 13:12:51 mosu Exp $
    \brief cluster helper
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <assert.h>
#include <malloc.h>

#include "cluster_helper.h"
#include "common.h"
#include "mkvmerge.h"

#include "StdIOCallback.h"

//#define walk_clusters() check_clusters(__LINE__)
#define walk_clusters()

cluster_helper_c::cluster_helper_c() {
  num_clusters = 0;
  clusters = NULL;
  cluster_content_size = 0;
  last_block_group = NULL;
  max_timecode = 0;
}

cluster_helper_c::~cluster_helper_c() {
  int i;

  for (i = 0; i < num_clusters; i++)
    free_contents(clusters[i]);

  if (clusters != NULL)
    free(clusters);
}

void cluster_helper_c::free_contents(ch_contents_t *clstr) {
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
  ch_contents_t *c;

  if (clusters == NULL)
    return;

  c = clusters[num_clusters - 1];
  c->packets = (packet_t **)realloc(c->packets, sizeof(packet_t *) *
                                    (c->num_packets + 1));
  if (c->packets == NULL)
    die("realloc");

  c->packets[c->num_packets] = packet;
  c->num_packets++;
  cluster_content_size += packet->length;

  if (packet->timecode > max_timecode)
    max_timecode = packet->timecode;

  walk_clusters();
}

int64_t cluster_helper_c::get_timecode() {
  if (clusters == NULL)
    return 0;
  if (clusters[num_clusters - 1]->packets == NULL)
    return 0;
  return clusters[num_clusters - 1]->packets[0]->timecode;
}

packet_t *cluster_helper_c::get_packet(int num) {
  ch_contents_t *c;

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
  ch_contents_t *c;

  if (find_cluster(cluster) != -1)
    return;
  c = (ch_contents_t *)malloc(sizeof(ch_contents_t));
  if (c == NULL)
    die("malloc");
  clusters = (ch_contents_t **)realloc(clusters, sizeof(ch_contents_t *) *
                                       (num_clusters + 1));
  if (clusters == NULL)
    die("realloc");
  memset(c, 0, sizeof(ch_contents_t));
  clusters[num_clusters] = c;
  num_clusters++;
  c->cluster = cluster;
  cluster_content_size = 0;
  cluster->SetParent(*kax_segment);
  cluster->SetPreviousTimecode(0);
}

int cluster_helper_c::get_cluster_content_size() {
  return cluster_content_size;
}

int cluster_helper_c::render(IOCallback *out) {
  KaxCluster *cluster;
  KaxBlockGroup *new_group;
  int i;
  ch_contents_t *clstr;
  packet_t *pack, *bref_packet, *fref_packet;

  if ((clusters == NULL) || (num_clusters == 0))
    return 0;

  walk_clusters();
  clstr = clusters[num_clusters - 1];
  cluster = clstr->cluster;

  for (i = 0; i < clstr->num_packets; i++) {
    pack = clstr->packets[i];

    pack->data_buffer = new DataBuffer((binary *)pack->data, pack->length);
    KaxTrackEntry &track_entry =
      static_cast<KaxTrackEntry &>
      (*((generic_packetizer_c *)pack->source)->get_track_entry());

    if (pack->bref != 0) {      // P and B frames: add backward reference.
      bref_packet = find_packet(pack->bref);
      assert(bref_packet != NULL);
      assert(bref_packet->group != NULL);
      if (pack->fref != 0) {    // It's even a B frame: add forward reference.
        fref_packet = find_packet(pack->fref);
        assert(fref_packet != NULL);
        assert(fref_packet->group != NULL);
        cluster->AddFrame(track_entry, pack->timecode * 1000000LL,
                          *pack->data_buffer, new_group, *bref_packet->group,
                          *fref_packet->group);
      } else {
        cluster->AddFrame(track_entry, pack->timecode * 1000000LL,
                          *pack->data_buffer, new_group, *bref_packet->group);
      }
    } else {                    // This is a key frame. No references.
      cluster->AddFrame(track_entry, pack->timecode * 1000000LL,
                        *pack->data_buffer, new_group);
      // All packets with an ID smaller than this packet's ID are not
      // needed anymore. Be happy!
      free_ref(pack->timecode, pack->source);
    }
    if (new_group == NULL)
      new_group = last_block_group;
    else
      kax_cues->AddBlockGroup(*new_group);
    pack->group = new_group;
    last_block_group = new_group;
  }

  cluster->Render(static_cast<StdIOCallback &>(*out), *kax_cues);

  for (i = 0; i < clstr->num_packets; i++) {
    pack = clstr->packets[i];
    free(pack->data);
    pack->data = NULL;
  }

  clstr->rendered = 1;

  free_clusters();

  return 1;
}

ch_contents_t *cluster_helper_c::find_packet_cluster(int64_t ref_timecode) {
  int i, k;

  if (clusters == NULL)
    return NULL;

  for (i = 0; i < num_clusters; i++)
    for (k = 0; k < clusters[i]->num_packets; k++)
      if (clusters[i]->packets[k]->timecode == ref_timecode)
        return clusters[i];

  return NULL;
}

packet_t *cluster_helper_c::find_packet(int64_t ref_timecode) {
  int i, k;

  if (clusters == NULL)
    return NULL;

  for (i = 0; i < num_clusters; i++)
    for (k = 0; k < clusters[i]->num_packets; k++)
      if (clusters[i]->packets[k]->timecode == ref_timecode)
        return clusters[i]->packets[k];

  return NULL;
}

void cluster_helper_c::check_clusters(int num) {
  int i, k;
  packet_t *p;
  ch_contents_t *clstr;

  for (i = 0; i < num_clusters; i++) {
    for (k = 0; k < clusters[i]->num_packets; k++) {
      p = clusters[i]->packets[k];
      if (clusters[i]->rendered && p->superseeded)
        continue;
      if (p->bref == 0)
        continue;
      clstr = find_packet_cluster(p->bref);
      if (clstr == NULL) {
        fprintf(stderr, "Error: backward refenrece could not be resolved "
                "(%lld -> %lld). Called from line %d.\n",
                p->timecode, p->bref, num);
        die("internal error");
      }
    }
  }
}

int cluster_helper_c::free_clusters() {
  int i, k, idx;
  packet_t *p;
  ch_contents_t *clstr, **new_clusters;

  if (clusters == NULL)
    return 0;

  for (i = 0; i < num_clusters; i++)
    clusters[i]->is_referenced = 0;

  // Part 1 - Mark all packets superseeded for which their source has
  // an appropriate free_refs entry.
  for (i = 0; i < num_clusters; i++) {
    for (k = 0; k < clusters[i]->num_packets; k++) {
      p = clusters[i]->packets[k];
      if (((generic_packetizer_c *)p->source)->get_free_refs() > p->timecode)
        p->superseeded = 1;
    }
  }

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
                  "(%lld).\n", p->bref);
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
    new_clusters = (ch_contents_t **)malloc(sizeof(ch_contents_t *) * k);
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

#if 0
  fprintf(stdout, "numcl: %8d ", num_clusters);
  for (i = 0; i < num_clusters; i++)
    fprintf(stdout, "#");
  fprintf(stdout, "\n");
#endif

  return 1;
}

int cluster_helper_c::free_ref(int64_t ref_timecode, void *source) {
  ((generic_packetizer_c *)source)->set_free_refs(ref_timecode);

  return 1;
}

int64_t cluster_helper_c::get_max_timecode() {
  return max_timecode;
}
