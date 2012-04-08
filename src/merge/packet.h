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

#include "common/common_pch.h"

namespace libmatroska {
  class KaxBlock;
  class KaxBlockBlob;
  class KaxCluster;
};

using namespace libmatroska;

class generic_packetizer_c;

class packet_extension_c {
public:
  enum packet_extension_type_e {
    MULTIPLE_TIMECODES,
    SUBTITLE_NUMBER,
  };

public:
  packet_extension_c() {
  }

  virtual ~packet_extension_c() {
  }

  virtual packet_extension_type_e get_type() = 0;
};
typedef std::shared_ptr<packet_extension_c> packet_extension_cptr;

struct packet_t {
  static int64_t sm_packet_number_counter;

  memory_cptr data;
  std::vector<memory_cptr> data_adds;
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

  std::vector<packet_extension_cptr> extensions;

  packet_t()
    : group(nullptr)
    , block(nullptr)
    , cluster(nullptr)
    , ref_priority(0)
    , time_factor(1)
    , timecode(0)
    , bref(0)
    , fref(0)
    , duration(-1)
    , packet_num(sm_packet_number_counter++)
    , assigned_timecode(0)
    , timecode_before_factory(0)
    , unmodified_assigned_timecode(0)
    , unmodified_duration(0)
    , duration_mandatory(false)
    , superseeded(false)
    , gap_following(false)
    , factory_applied(false)
    , source(nullptr)
  {
  }

  packet_t(memory_cptr p_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(p_memory)
    , group(nullptr)
    , block(nullptr)
    , cluster(nullptr)
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
    , source(nullptr)
  {
  }

  packet_t(memory_c *n_memory,
           int64_t p_timecode = -1,
           int64_t p_duration = -1,
           int64_t p_bref     = -1,
           int64_t p_fref     = -1)
    : data(memory_cptr(n_memory))
    , group(nullptr)
    , block(nullptr)
    , cluster(nullptr)
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
    , source(nullptr)
  {
  }

  ~packet_t();

  bool has_timecode() {
    return 0 <= timecode;
  }

  bool has_bref() {
    return 0 <= bref;
  }

  bool has_fref() {
    return 0 <= fref;
  }

  bool has_duration() {
    return 0 <= duration;
  }

  bool is_key_frame() {
    return !has_bref() && !has_fref();
  }

  bool is_p_frame() {
    return (has_bref() || has_fref()) && (has_bref() != has_fref());
  }

  bool is_b_frame() {
    return has_bref() && has_fref();
  }

  packet_extension_c *find_extension(packet_extension_c::packet_extension_type_e type) {
    for (auto &extension : extensions)
      if (extension->get_type() == type)
        return extension.get();

    return nullptr;
  }

  void normalize_timecodes();
};
typedef std::shared_ptr<packet_t> packet_cptr;

#endif // __PACKET_H
