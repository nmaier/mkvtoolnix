/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp4.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the Quicktime & MP4 reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_MP4_H
#define __R_MP4_H

#include "os.h"

#include <stdio.h>

#include <vector>

#include "common.h"
#include "mm_io.h"
#include "p_video.h"

class qtmp4_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
//   vector<real_demuxer_t *> demuxers;
  int64_t file_size;//, last_timecode, num_packets_in_chunk, num_packets;
  bool done;

public:
  qtmp4_reader_c(track_info_t *nti) throw (error_c);
  virtual ~qtmp4_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *in, int64_t size);

protected:
  virtual void parse_headers();
  virtual void create_packetizers();
//   virtual real_demuxer_t *find_demuxer(int id);
//   virtual void assemble_packet(real_demuxer_t *dmx, unsigned char *p, int size,
//                                int64_t timecode, bool keyframe);
//   virtual void deliver_segments(real_demuxer_t *dmx, int64_t timecode);
//   virtual int finish();
//   virtual bool get_rv_dimensions(unsigned char *buf, int size, uint32_t &width,
//                                  uint32_t &height);
//   virtual void set_dimensions(real_demuxer_t *dmx, unsigned char *buffer,
//                               int size);
//   virtual void get_information_from_data();
};

#endif  // __R_MP4_H
