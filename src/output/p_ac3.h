/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_ac3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the AC3 output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_AC3_H
#define __P_AC3_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"
#include "ac3_common.h"

class ac3_packetizer_c: public generic_packetizer_c {
protected:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, bsid;
  byte_buffer_c byte_buffer;

public:
  ac3_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                   int nchannels, int nbsid, track_info_c *nti)
    throw (error_c);
  virtual ~ac3_packetizer_c();

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "AC3";
  }
  virtual int can_connect_to(generic_packetizer_c *src);

protected:
  virtual unsigned char *get_ac3_packet(unsigned long *header,
                                        ac3_header_t *ac3header);
  virtual void add_to_buffer(unsigned char *buf, int size);

  virtual void dump_debug_info();
};

class ac3_bs_packetizer_c: public ac3_packetizer_c {
protected:
  unsigned char bsb;
  bool bsb_present;

public:
  ac3_bs_packetizer_c(generic_reader_c *nreader,
                      unsigned long nsamples_per_sec,
                      int nchannels, int nbsid, track_info_c *nti)
    throw (error_c);

protected:
  virtual void add_to_buffer(unsigned char *buf, int size);
};

#endif // __P_AC3_H
