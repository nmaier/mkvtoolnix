/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_SSA_H
#define __R_SSA_H

#include "os.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "mm_io.h"
#include "common.h"
#include "pr_generic.h"
#include "subtitles.h"

using namespace std;

class ssa_reader_c: public generic_reader_c {
private:
  enum ssa_section_e {
    SSA_SECTION_NONE,
    SSA_SECTION_INFO,
    SSA_SECTION_V4STYLES,
    SSA_SECTION_EVENTS,
    SSA_SECTION_GRAPHICS,
    SSA_SECTION_FONTS
  };

  vector<string> m_format;
  int m_cc_utf8;
  bool m_is_ass;
  string m_global;
  subtitles_c m_subs;

public:
  ssa_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~ssa_reader_c();

  virtual void parse_file(mm_text_io_c *io);
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_text_io_c *io, int64_t size);

protected:
  virtual int64_t parse_time(string &time);
  virtual string get_element(const char *index, vector<string> &fields);
  virtual string recode_text(vector<string> &fields);
  virtual void add_attachment_maybe(string &name, string &data_uu,
                                    ssa_section_e section);
  virtual void decode_chars(unsigned char c1, unsigned char c2,
                            unsigned char c3, unsigned char c4,
                            buffer_t &buffer, int bytes_to_add,
                            int &allocated);
};

#endif  // __R_SSA_H
