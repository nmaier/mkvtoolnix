/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   quick Matroska file parsing

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_KAX_ANALYZER_H
#define __MTX_COMMON_KAX_ANALYZER_H

#include "common/common_pch.h"

#include <matroska/KaxSegment.h>

#include "common/ebml.h"
#include "common/matroska.h"
#include "common/mm_io.h"

using namespace libebml;
using namespace libmatroska;

class kax_analyzer_data_c;
typedef std::shared_ptr<kax_analyzer_data_c> kax_analyzer_data_cptr;

class kax_analyzer_data_c {
public:
  EbmlId m_id;
  uint64_t m_pos;
  int64_t m_size;

public:                         // Static functions
  static kax_analyzer_data_cptr create(const EbmlId id, uint64_t pos, int64_t size) {
    return kax_analyzer_data_cptr(new kax_analyzer_data_c(id, pos, size));
  }

public:
  kax_analyzer_data_c(const EbmlId id, uint64_t pos, int64_t size)
    : m_id(id)
    , m_pos(pos)
    , m_size(size)
  {
  }

  std::string to_string() const;
};

bool operator <(const kax_analyzer_data_cptr &d1, const kax_analyzer_data_cptr &d2);

namespace mtx {
  class kax_analyzer_x: public exception {
  protected:
    std::string m_message;
  public:
    kax_analyzer_x(const std::string &message)  : m_message(message)       { }
    kax_analyzer_x(const boost::format &message): m_message(message.str()) { }
    virtual ~kax_analyzer_x() throw() { }

    virtual const char *what() const throw() {
      return m_message.c_str();
    }
  };
}

class kax_analyzer_c {
public:
  enum update_element_result_e {
    uer_success,
    uer_error_segment_size_for_element,
    uer_error_segment_size_for_meta_seek,
    uer_error_meta_seek,
    uer_error_not_indexable,
    uer_error_unknown,
  };

  enum parse_mode_e {
    parse_mode_fast,
    parse_mode_full,
  };

  enum placement_strategy_e {
    ps_anywhere,
    ps_end,
  };

public:
  std::vector<kax_analyzer_data_cptr> m_data;

private:
  std::string m_file_name;
  mm_file_io_c *m_file;
  bool m_close_file;
  std::shared_ptr<KaxSegment> m_segment;
  std::map<int64_t, bool> m_meta_seeks_by_position;
  EbmlStream *m_stream;
  bool m_debugging_requested;

public:                         // Static functions
  static bool probe(std::string file_name);

public:
  kax_analyzer_c(std::string file_name);
  kax_analyzer_c(mm_file_io_c *file);
  virtual ~kax_analyzer_c();

  virtual update_element_result_e update_element(EbmlElement *e, bool write_defaults = false);
  virtual update_element_result_e update_element(ebml_element_cptr e, bool write_defaults = false) {
    return update_element(e.get(), write_defaults);
  }
  virtual update_element_result_e remove_elements(EbmlId id);
  virtual ebml_master_cptr read_all(const EbmlCallbacks &callbacks);

  virtual ebml_element_cptr read_element(kax_analyzer_data_c *element_data);
  virtual ebml_element_cptr read_element(kax_analyzer_data_cptr element_data) {
    return read_element(element_data.get());
  }
  virtual ebml_element_cptr read_element(unsigned int pos) {
    return read_element(m_data[pos]);
  }

  virtual int find(const EbmlId &id) {
    unsigned int i;

    for (i = 0; m_data.size() > i; i++)
      if (id == m_data[i]->m_id)
        return i;

    return -1;
  }

  virtual bool process(parse_mode_e parse_mode = parse_mode_full, const open_mode mode = MODE_WRITE);

  virtual void show_progress_start(int64_t /* size */) {
  }
  virtual bool show_progress_running(int /* percentage */) {
    return true;
  }
  virtual void show_progress_done() {
  }

  virtual void log_debug_message(const std::string &message) {
    _log_debug_message(message);
  }
  virtual void log_debug_message(const boost::format &message) {
    _log_debug_message(message.str());
  }
  virtual void debug_abort_process() {
    mxexit(1);
  }

  virtual void close_file();
  virtual void reopen_file(const open_mode = MODE_WRITE);

  static placement_strategy_e get_placement_strategy_for(EbmlElement *e);
  static placement_strategy_e get_placement_strategy_for(ebml_element_cptr e) {
    return get_placement_strategy_for(e.get());
  }

protected:
  virtual void _log_debug_message(const std::string &message);

  virtual void remove_from_meta_seeks(EbmlId id);
  virtual void overwrite_all_instances(EbmlId id);
  virtual void merge_void_elements();
  virtual void write_element(EbmlElement *e, bool write_defaults, placement_strategy_e strategy);
  virtual void add_to_meta_seek(EbmlElement *e);

  virtual void adjust_segment_size();
  virtual bool handle_void_elements(size_t data_idx);

  virtual bool analyzer_debugging_requested(const std::string &section);
  virtual void debug_dump_elements();
  virtual void debug_dump_elements_maybe(const std::string &hook_name);
  virtual void validate_data_structures(const std::string &hook_name);
  virtual void verify_data_structures_against_file(const std::string &hook_name);

  virtual void read_all_meta_seeks();
  virtual void read_meta_seek(uint64_t pos, std::map<int64_t, bool> &positions_found);
  virtual void fix_element_sizes(uint64_t file_size);
};
typedef std::shared_ptr<kax_analyzer_c> kax_analyzer_cptr;

class console_kax_analyzer_c: public kax_analyzer_c {
private:
  bool m_show_progress;
  int m_previous_percentage;

public:
  console_kax_analyzer_c(std::string file_name);
  virtual ~console_kax_analyzer_c();

  virtual void set_show_progress(bool show_progress);

  virtual void show_progress_start(int64_t size);
  virtual bool show_progress_running(int percentage);
  virtual void show_progress_done();

  virtual void debug_abort_process();
};
typedef std::shared_ptr<console_kax_analyzer_c> console_kax_analyzer_cptr;

#endif  // __MTX_COMMON_KAX_ANALYZER_H
