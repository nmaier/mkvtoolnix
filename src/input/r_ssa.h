/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 * 
 * class definition for the SSA/ASS subtitle parser
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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

using namespace std;

class ssa_reader_c: public generic_reader_c {
private:
  mm_text_io_c *mm_io;
  vector<string> format;
  int cc_utf8;
  bool is_ass;
  string global;

public:
  ssa_reader_c(track_info_c *nti) throw (error_c);
  virtual ~ssa_reader_c();

  virtual file_status_t read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_text_io_c *mm_io, int64_t size);

protected:
  virtual int64_t parse_time(string &time);
  virtual string get_element(const char *index, vector<string> &fields);
  virtual string recode_text(vector<string> &fields);
};

#endif  // __R_SSA_H
