/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   class definition for the timecode factory
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TIMECODE_FACTORY_H
#define __TIMECODE_FACTORY_H

#include <string>
#include <vector>

using namespace std;

class mm_io_c;

class timecode_range_c {
public:
  int64_t start_frame, end_frame;
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

class timecode_factory_c {
protected:
  string file_name, source_name;
  int64_t tid;

public:
  timecode_factory_c(const string &_file_name, const string &_source_name,
                     int64_t _tid):
    file_name(_file_name),
    source_name(_source_name),
    tid(_tid) {
  }
  virtual ~timecode_factory_c() {
  }

  virtual void parse(mm_io_c &) {
  }
  virtual bool get_next(int64_t &timecode, int64_t &duration,
                        bool peek_only = false) {
    // No gap is following!
    return false;
  }
  virtual bool peek_next(int64_t &timecode, int64_t &duration) {
    return get_next(timecode, duration, true);
  }
  virtual double get_default_duration(double proposal) {
    return proposal;
  }
  
  virtual bool contains_gap() {
    return false;
  }

  static timecode_factory_c *create(const string &_file_name,
                                    const string &_source_name,
                                    int64_t _tid);
};

class timecode_factory_v1_c: public timecode_factory_c {
protected:
  vector<timecode_range_c> ranges;
  uint32_t current_range;
  int64_t frameno;
  double default_fps;
  
public:
  timecode_factory_v1_c(const string &_file_name, const string &_source_name,
                        int64_t _tid):
    timecode_factory_c(_file_name, _source_name, _tid),
    current_range(0),
    frameno(0),
    default_fps(0.0) {
  }
  virtual ~timecode_factory_v1_c() {
  }

  virtual void parse(mm_io_c &in);
  virtual bool get_next(int64_t &timecode, int64_t &duration,
                        bool peek_only = false);
  virtual double get_default_duration(double proposal) {
    return default_fps != 0.0 ? (double)1000000000.0 / default_fps : proposal;
  }

protected:
  virtual int64_t get_at(int64_t frame);
};

class timecode_factory_v2_c: public timecode_factory_c {
protected:
  vector<int64_t> timecodes, durations;
  bool warning_printed;
  int64_t frameno;
  double default_fps;

public:
  timecode_factory_v2_c(const string &_file_name, const string &_source_name,
                        int64_t _tid):
    timecode_factory_c(_file_name, _source_name, _tid),
    warning_printed(false),
    frameno(0),
    default_fps(0.0) {
  }
  virtual ~timecode_factory_v2_c() {
  }

  virtual void parse(mm_io_c &in);
  virtual bool get_next(int64_t &timecode, int64_t &duration,
                        bool peek_only = false);
  virtual double get_default_duration(double proposal) {
    return default_fps != 0.0 ? (double)1000000000.0 / default_fps : proposal;
  }
};

class timecode_factory_v3_c: public timecode_factory_c {
protected:
  vector<timecode_duration_c> durations;
  size_t current_duration;
  int64_t current_timecode;
  int64_t current_offset;
  double default_fps;

public:
  timecode_factory_v3_c(const string &_file_name, const string &_source_name,
                        int64_t _tid):
    timecode_factory_c(_file_name, _source_name, _tid),
    current_duration(0),
    current_timecode(0),
    current_offset(0),
    default_fps(0.0) {
  }
  virtual void parse(mm_io_c &in);
  virtual bool get_next(int64_t &timecode, int64_t &duration,
                        bool peek_only = false);
  virtual bool contains_gap() {
    return true;
  }
};

#endif // __TIMECODE_FACTORY_H
