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
 * class definitions for the WAV reader module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 * Modified by Peter Niemayer <niemayer@isg.de>.
 */

#ifndef __R_WAV_H
#define __R_WAV_H

#include "os.h"

#include <stdio.h>

#include "common.h"
#include "dts_common.h"
#include "error.h"
#include "mm_io.h"
#include "pr_generic.h"

extern "C" {
#include "avilib.h"
}

class wav_reader_c: public generic_reader_c {
private:
  unsigned char *chunk;
  mm_io_c *mm_io;
  int dts_swap_bytes, dts_14_16;
  int bps;
  struct wave_header wheader;
  int64_t bytes_processed;
  bool is_dts;
  dts_header_t dtsheader;

public:
  wav_reader_c(track_info_c *nti) throw (error_c);
  virtual ~wav_reader_c();

  virtual file_status_t read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  virtual int get_progress();

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_WAV_H
