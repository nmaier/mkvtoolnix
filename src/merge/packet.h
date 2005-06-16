/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the packet structure

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PACKET_H
#define __PACKET_H

#include "os.h"

#include <vector>

#include "common_memory.h"
#include "smart_pointers.h"

namespace libmatroska {
  class KaxBlock;
  class KaxBlockGroup;
  class KaxCluster;
};

using namespace libmatroska;
using namespace std;

class generic_packetizer_c;

struct packet_t {
  static int64_t packet_number_counter;

  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, ref_priority, time_factor;
  int64_t timecode, bref, fref, duration, packet_num, assigned_timecode;
  int64_t unmodified_assigned_timecode, unmodified_duration;
  bool duration_mandatory, superseeded, gap_following;
  generic_packetizer_c *source;
  vector<unsigned char *> data_adds;
  vector<int> data_adds_lengths;

  memory_cptr memory;
  memories_c memory_adds;

  packet_t():
    group(NULL), block(NULL), cluster(NULL), data(NULL), length(0),
    ref_priority(0), time_factor(1),
    timecode(0), bref(0), fref(0), duration(0),
    packet_num(packet_number_counter++),
    assigned_timecode(0), unmodified_assigned_timecode(0),
    unmodified_duration(0),
    duration_mandatory(false), superseeded(false), gap_following(false),
    source(NULL) { }

  packet_t(memory_cptr n_memory,
           int64_t n_timecode = -1,
           int64_t n_duration = -1,
           int64_t n_bref = -1,
           int64_t n_fref = -1):
    group(NULL), block(NULL), cluster(NULL), data(NULL), length(0),
    ref_priority(0), time_factor(1),
    timecode(n_timecode), bref(n_bref), fref(n_fref),
    duration(n_duration),
    packet_num(packet_number_counter++),
    assigned_timecode(0), unmodified_assigned_timecode(0),
    unmodified_duration(0),
    duration_mandatory(false), superseeded(false), gap_following(false),
    source(NULL), memory(n_memory) { }

  packet_t(memory_c *n_memory,
           int64_t n_timecode = -1,
           int64_t n_duration = -1,
           int64_t n_bref = -1,
           int64_t n_fref = -1):
    group(NULL), block(NULL), cluster(NULL), data(NULL), length(0),
    ref_priority(0), time_factor(1),
    timecode(n_timecode), bref(n_bref), fref(n_fref),
    duration(n_duration),
    packet_num(packet_number_counter++),
    assigned_timecode(0), unmodified_assigned_timecode(0),
    unmodified_duration(0),
    duration_mandatory(false), superseeded(false), gap_following(false),
    source(NULL), memory(memory_cptr(n_memory)) { }

  ~packet_t();
};
typedef counted_ptr<packet_t> packet_cptr;

#endif // __PACKET_H
