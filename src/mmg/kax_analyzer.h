/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __KAX_ANALYZER_H
#define __KAX_ANALYZER_H

#include <string>
#include <vector>

#include <ebml/EbmlElement.h>
#include <matroska/KaxSegment.h>

#include <wx/wx.h>

#include "common.h"
#include "error.h"
#include "mm_io.h"

using namespace std;
using namespace libmatroska;

#define ID_B_ANALYZER_ABORT 11000

class analyzer_data_c {
public:
  EbmlId id;
  int64_t pos, size;
  bool delete_this;

public:
  analyzer_data_c(const EbmlId nid, int64_t npos, int64_t nsize):
  id(nid), pos(npos), size(nsize), delete_this(false) {
  };
};

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

public:
  wxWindow *m_parent;

  vector<analyzer_data_c *> data;
  string file_name;
  mm_file_io_c *file;
  KaxSegment *segment;

public:                         // Static functions
  static bool probe(string file_name);

public:
  kax_analyzer_c(wxWindow *parent, string name);
  virtual ~kax_analyzer_c();

  virtual update_element_result_e update_element(EbmlElement *e, bool write_defaults = false);
  virtual EbmlElement *read_element(analyzer_data_c *element_data, const EbmlCallbacks &callbacks);
  virtual EbmlElement *read_element(uint32_t pos, const EbmlCallbacks &callbacks) {
    return read_element(data[pos], callbacks);
  }
  virtual int find(const EbmlId &id) {
    uint32_t i;

    for (i = 0; i < data.size(); i++)
      if (id == data[i]->id)
        return i;

    return -1;
  };

  virtual bool process();

protected:
  virtual void remove_from_meta_seeks(EbmlId id);
  virtual void overwrite_all_instances(EbmlId id);
  virtual void merge_void_elements();
  virtual void write_element(EbmlElement *e, bool write_defaults);
  virtual void add_to_meta_seek(EbmlElement *e);

  virtual void adjust_segment_size();
  virtual bool create_void_element(int64_t file_pos, int void_size, int data_idx, bool add_new_data_element);

  virtual void debug_dump_elements();
};

#endif // __KAX_ANALYZER_H
