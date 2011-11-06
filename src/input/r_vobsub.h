/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the VobSub stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_VOBSUB_H
#define __R_VOBSUB_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "output/p_vobsub.h"

class vobsub_entry_c {
public:
  int64_t position;
  int64_t timestamp;
  int64_t duration;

  bool operator < (const vobsub_entry_c &cmp) const;
};

class vobsub_track_c {
public:
  std::string language;
  int ptzr;
  std::vector<vobsub_entry_c> entries;
  unsigned int idx;
  int aid;
  bool mpeg_version_warning_printed;
  int64_t packet_num, spu_size, overhead;

public:
  vobsub_track_c(const std::string &new_language):
    language(new_language),
    ptzr(-1),
    idx(0),
    aid(-1),
    mpeg_version_warning_printed(false),
    packet_num(0),
    spu_size(0),
    overhead(0) {
  }
};

class vobsub_reader_c: public generic_reader_c {
private:
  mm_io_c *sub_file;
  mm_text_io_c *idx_file;
  int version, ifo_data_size;
  int64_t num_indices, indices_processed, delay;
  std::string idx_data;

  std::vector<vobsub_track_c *> tracks;

private:
  static const std::string id_string;

public:
  vobsub_reader_c(track_info_c &_ti);
  virtual ~vobsub_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("VobSub") : "VobSub";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();
  virtual int get_progress();
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_io_c *io, uint64_t size);

protected:
  virtual void parse_headers();
  virtual file_status_e flush_packetizers();
  virtual int deliver_packet(unsigned char *buf, int size, int64_t timecode, int64_t default_duration, generic_packetizer_c *ptzr);

  virtual int extract_one_spu_packet(int64_t track_id);
};

#endif  // __R_VOBSUB_H
