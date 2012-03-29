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

#include "merge/pr_generic.h"

struct usf_entry_t {
  int64_t m_start, m_end;
  std::string m_text;

  usf_entry_t()
    : m_start(-1)
    , m_end(-1)
  {
  }

  usf_entry_t(int64_t start,
              int64_t end,
              std::string const &text)
    : m_start(start)
    , m_end(end)
    , m_text(text)
  {
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

  usf_track_t()
    : m_ptzr(-1)
  {
  }
};
typedef std::shared_ptr<usf_track_t> usf_track_cptr;

class usf_reader_c: public generic_reader_c {
private:
  std::vector<usf_track_cptr> m_tracks;
  std::string m_private_data, m_default_language;
  usf_track_cptr m_longest_track;

public:
  usf_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~usf_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("USF subtitles") : "USF subtitles";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();
  virtual int get_progress();
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_text_io_c *in, uint64_t size);

protected:
  virtual int64_t try_to_parse_timecode(const char *s);
  virtual void parse_metadata(mtx::xml::document_cptr &doc);
  virtual void parse_subtitles(mtx::xml::document_cptr &doc);
  virtual void create_codec_private(mtx::xml::document_cptr &doc);

};

#endif  // __R_USF_H
