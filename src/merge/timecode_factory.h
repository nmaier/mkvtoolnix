/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the timecode factory

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TIMECODE_FACTORY_H
#define __TIMECODE_FACTORY_H

#include "common/common_pch.h"

#include "merge/packet.h"

enum timecode_factory_application_e {
  TFA_AUTOMATIC,
  TFA_IMMEDIATE,
  TFA_SHORT_QUEUEING,
  TFA_FULL_QUEUEING
};

class timecode_range_c {
public:
  uint64_t start_frame, end_frame;
  double fps, base_timecode;

  bool operator <(const timecode_range_c &cmp) const {
    return start_frame < cmp.start_frame;
  }
};

class timecode_duration_c {
public:
  double fps;
  int64_t duration;
  bool is_gap;
};

class timecode_factory_c;
typedef counted_ptr<timecode_factory_c> timecode_factory_cptr;

class timecode_factory_c {
protected:
  std::string m_file_name, m_source_name;
  int64_t m_tid;
  int m_version;
  bool m_preserve_duration, m_debug;

public:
  timecode_factory_c(const std::string &file_name,
                     const std::string &source_name,
                     int64_t tid,
                     int version)
    : m_file_name(file_name)
    , m_source_name(source_name)
    , m_tid(tid)
    , m_version(version)
    , m_preserve_duration(false)
    , m_debug(debugging_requested("timecode_factory"))
  {
  }
  virtual ~timecode_factory_c() {
  }

  virtual void parse(mm_io_c &) {
  }
  virtual bool get_next(packet_cptr &packet) {
    // No gap is following!
    packet->assigned_timecode = packet->timecode;
    return false;
  }
  virtual double get_default_duration(double proposal) {
    return proposal;
  }

  virtual bool contains_gap() {
    return false;
  }

  virtual void set_preserve_duration(bool preserve_duration) {
    m_preserve_duration = preserve_duration;
  }

  static timecode_factory_cptr create(const std::string &file_name, const std::string &source_name, int64_t tid);
  static timecode_factory_cptr create_fps_factory(int64_t default_duration, const std::string &source_name, int64_t tid);
};

class timecode_factory_v1_c: public timecode_factory_c {
protected:
  std::vector<timecode_range_c> m_ranges;
  uint32_t m_current_range;
  uint64_t m_frameno;
  double m_default_fps;

public:
  timecode_factory_v1_c(const std::string &file_name,
                        const std::string &source_name,
                        int64_t tid)
    : timecode_factory_c(file_name, source_name, tid, 1)
    , m_current_range(0)
    , m_frameno(0)
    , m_default_fps(0.0)
  {
  }
  virtual ~timecode_factory_v1_c() {
  }

  virtual void parse(mm_io_c &in);
  virtual bool get_next(packet_cptr &packet);
  virtual double get_default_duration(double proposal) {
    return 0.0 != m_default_fps ? 1000000000.0 / m_default_fps : proposal;
  }

protected:
  virtual int64_t get_at(uint64_t frame);
};

class timecode_factory_v2_c: public timecode_factory_c {
protected:
  std::vector<int64_t> m_timecodes, m_durations;
  int64_t m_frameno;
  double m_default_fps;
  bool m_warning_printed;

public:
  timecode_factory_v2_c(const std::string &file_name,
                        const std::string &source_name,
                        int64_t tid, int version)
    : timecode_factory_c(file_name, source_name, tid, version)
    , m_frameno(0)
    , m_default_fps(0.0)
    , m_warning_printed(false)
  {
  }
  virtual ~timecode_factory_v2_c() {
  }

  virtual void parse(mm_io_c &in);
  virtual bool get_next(packet_cptr &packet);
  virtual double get_default_duration(double proposal) {
    return m_default_fps != 0.0 ? static_cast<double>(1000000000.0) / m_default_fps : proposal;
  }
};

class timecode_factory_v3_c: public timecode_factory_c {
protected:
  std::vector<timecode_duration_c> m_durations;
  size_t m_current_duration;
  int64_t m_current_timecode;
  int64_t m_current_offset;
  double m_default_fps;

public:
  timecode_factory_v3_c(const std::string &file_name,
                        const std::string &source_name,
                        int64_t tid)
    : timecode_factory_c(file_name, source_name, tid, 3)
    , m_current_duration(0)
    , m_current_timecode(0)
    , m_current_offset(0)
    , m_default_fps(0.0)
  {
  }
  virtual void parse(mm_io_c &in);
  virtual bool get_next(packet_cptr &packet);
  virtual bool contains_gap() {
    return true;
  }
};

#endif // __TIMECODE_FACTORY_H
