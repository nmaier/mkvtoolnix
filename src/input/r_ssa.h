/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ssa.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the SSA/ASS subtitle parser
    \author Moritz Bunkus <moritz@bunkus.org>
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

#include "p_textsubs.h"

using namespace std;

class ssa_reader_c: public generic_reader_c {
private:
  mm_text_io_c *mm_io;
  textsubs_packetizer_c *textsubs_packetizer;
  int act_wchar;
  vector<string> format;
  int cc_utf8;

public:
  ssa_reader_c(track_info_c *nti) throw (error_c);
  virtual ~ssa_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void set_headers();
  virtual void identify();

  virtual int display_priority();
  virtual void display_progress(bool final = false);

  static int probe_file(mm_text_io_c *mm_io, int64_t size);

protected:
  virtual int64_t parse_time(string &time);
  virtual string get_element(const char *index, vector<string> &fields);
  virtual string recode_text(vector<string> &fields);
};

#endif  // __R_SSA_H
