/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_TEXTSUBS_H
#define __XTR_TEXTSUBS_H

#include "os.h"

#include <string>
#include <vector>

#include "xml_element_writer.h"

#include "xtr_base.h"

using namespace std;

class xtr_srt_c: public xtr_base_c {
public:
  int m_num_entries;
  string m_sub_charset;
  int m_conv;

public:
  xtr_srt_c(const string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "SRT text subtitles";
  };
};

class xtr_ssa_c: public xtr_base_c {
public:
  class ssa_line_c {
  public:
    string m_line;
    int m_num;

    bool operator < (const ssa_line_c &cmp) const {
      return m_num < cmp.m_num;
    }

    ssa_line_c(string line, int num)
      : m_line(line)
      , m_num(num)
    { }
  };

  vector<string> m_ssa_format;
  vector<ssa_line_c> m_lines;
  string m_sub_charset;
  int m_conv;
  bool m_warning_printed;

  static const char *ms_kax_ssa_fields[10];

public:
  xtr_ssa_c(const string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "SSA/ASS text subtitles";
  };
};

class xtr_usf_c: public xtr_base_c {
private:
  struct usf_entry_t {
    string m_text;
    int64_t m_start, m_end;

    usf_entry_t(const string &text, int64_t start, int64_t end):
      m_text(text), m_start(start), m_end(end) { }
  };

  string m_sub_charset, m_codec_private, m_language;
  counted_ptr<xml_formatter_c> m_formatter;
  vector<usf_entry_t> m_entries;

public:
  xtr_usf_c(const string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void finish_track();
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "XML (USF text subtitles)";
  };
};

#endif
