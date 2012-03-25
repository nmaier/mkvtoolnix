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

#include "common/common_pch.h"

#include "common/xml/element_writer.h"
#include "extract/xtr_base.h"


class xtr_srt_c: public xtr_base_c {
public:
  int m_num_entries;
  std::string m_sub_charset;
  charset_converter_cptr m_conv;

public:
  xtr_srt_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

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
    std::string m_line;
    int m_num;

    bool operator < (const ssa_line_c &cmp) const {
      return m_num < cmp.m_num;
    }

    ssa_line_c(std::string line, int num)
      : m_line(line)
      , m_num(num)
    { }
  };

  std::vector<std::string> m_ssa_format;
  std::vector<ssa_line_c> m_lines;
  std::string m_sub_charset;
  charset_converter_cptr m_conv;
  bool m_warning_printed;

  static const char *ms_kax_ssa_fields[10];

public:
  xtr_ssa_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

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
    std::string m_text;
    int64_t m_start, m_end;

    usf_entry_t(const std::string &text, int64_t start, int64_t end):
      m_text(text), m_start(start), m_end(end) { }
  };

  std::string m_sub_charset, m_codec_private, m_language;
  std::shared_ptr<xml_formatter_c> m_formatter;
  std::vector<usf_entry_t> m_entries;

public:
  xtr_usf_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

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
