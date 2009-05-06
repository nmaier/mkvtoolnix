/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the packet structure

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PACKET_H
#define __PACKET_H

#include "common/os.h"

#include <vector>

#include "common/memory.h"
#include "common/smart_pointers.h"

namespace libmatroska {
  class KaxBlock;
  class KaxBlockBlob;
  class KaxCluster;
};

using namespace libmatroska;
using namespace std;

class generic_packetizer_c;

class packet_extension_c {
public:
  enum packet_extension_type_e {
    MULTIPLE_TIMECODES,
  };

public:
  packet_extension_c() {
  }

  virtual ~packet_extension_c() {
  }

  virtual packet_extension_type_e get_type() = 0;
};
typedef counted_ptr<packet_extension_c> packet_extension_cptr;

struct packet_t {
  static int64_t sm_packet_number_counter;

  memory_cptr data;
  vector<memory_cptr> data_adds;
  memory_cptr codec_state;

  KaxBlockBlob *group;
  KaxBlock *block;
  KaxCluster *cluster;
  int ref_priority, time_factor;
  int64_t timecode, bref, fref, duration, packet_num, assigned_timecode;
  int64_t timecode_before_factory;
  int64_t unmodified_assigned_timecode, unmodified_duration;
  bool duration_mandatory, superseeded, gap_following, factory_applied;
  generic_packetizer_c *source;

  vector<packet_extension_cptr> extensions;

  packet_t()
    : group(NULL)
    , block(NULL)
    , cluster(NULL)
    , ref_priority(0)
    , time_factor(1)
    , timecode(0)
    , bref(0)
    , fref(0)
    , duration(0)
    , packet_num(sm_packet_number_counter++)
    , assigned_timecode(0)
    , timecode_before_factory(0)
    , unmodified_assigned_timecode(0)
    , unmodified_duration(0)
    , duration_mandatory(false)
    , superseeded(false)
    , gap_following(false)
    , factory_applied(false)
    , source(NULL)
  {
  }

  packet_t(memory_cptr p_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(p_memory)
    , group(NULL)
    , block(NULL)
    , cluster(NULL)
    , ref_priority(0)
    , time_factor(1)
    , timecode(p_timecode)
    , bref(p_bref)
    , fref(p_fref)
    , duration(p_duration)
    , packet_num(sm_packet_number_counter++)
    , assigned_timecode(0)
    , timecode_before_factory(0)
    , unmodified_assigned_timecode(0)
    , unmodified_duration(0)
    , duration_mandatory(false)
    , superseeded(false)
    , gap_following(false)
    , factory_applied(false)
    , source(NULL)
  {
  }

  packet_t(memory_c *n_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(memory_cptr(n_memory))
    , group(NULL)
    , block(NULL)
    , cluster(NULL)
    , ref_priority(0)
    , time_factor(1)
    , timecode(p_timecode)
    , bref(p_bref)
    , fref(p_fref)
    , duration(p_duration)
    , packet_num(sm_packet_number_counter++)
    , assigned_timecode(0)
    , timecode_before_factory(0)
    , unmodified_assigned_timecode(0)
    , unmodified_duration(0)
    , duration_mandatory(false)
    , superseeded(false)
    , gap_following(false)
    , factory_applied(false)
    , source(NULL)
  {
  }

  ~packet_t();
};
typedef counted_ptr<packet_t> packet_cptr;

#endif // __PACKET_H
