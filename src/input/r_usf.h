/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the USF subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_USF_H
#define __R_USF_H

#include "common/common_pch.h"

#include <expat.h>

#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/xml/element_parser.h"

struct usf_entry_t {
  int64_t m_start, m_end;
  std::string m_text;

  usf_entry_t():
    m_start(-1),
    m_end(-1) {
  }

  usf_entry_t(int64_t start, int64_t end, const std::string &text):
    m_start(start),
    m_end(end),
    m_text(text) {
  }

  bool operator <(const usf_entry_t &cmp) const {
    return m_start < cmp.m_start;
  }
};

struct usf_track_t {
  int m_ptzr;

  std::string m_language;
  std::vector<usf_entry_t> m_entries;
  std::vector<usf_entry_t>::const_iterator m_current_entry;

  usf_track_t():
    m_ptzr(-1) {
  }
};

class usf_reader_c: public generic_reader_c, public xml_parser_c {
private:
  int m_copy_depth;

  std::vector<usf_track_t> m_tracks;
  std::string m_private_data, m_default_language;
  int m_longest_track;

  std::vector<std::string> m_parents;
  std::string m_data_buffer, m_copy_buffer, m_previous_start;
  bool m_strip;

public:
  usf_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~usf_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("USF") : "USF";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();
  virtual int get_progress();
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_text_io_c *io, uint64_t size);

  virtual void start_element_cb(const char *name, const char **atts);
  virtual void end_element_cb(const char *name);
  virtual void add_data_cb(const XML_Char *s, int len);

private:
  virtual int64_t try_to_parse_timecode(const char *s);
};

#endif  // __R_USF_H
