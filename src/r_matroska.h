/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_matroska.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the Matroska reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_MATROSKA_H
#define __R_MATROSKA_H

#include "os.h"

#include <stdio.h>

#include <vector>

#include "mm_io.h"
#include "common.h"
#include "pr_generic.h"
#include "error.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>

using namespace libmatroska;
using namespace std;

typedef struct {
  string name, type;
  int64 size, id;
} kax_attachment_t;

typedef struct {
  uint32_t tnum, tuid;

  char *codec_id;
  int ms_compat;

  char type; // 'v' = video, 'a' = audio, 't' = text subs
  char sub_type; // 't' = text, 'v' = VobSub

  // Parameters for video tracks
  uint32_t v_width, v_height, v_dwidth, v_dheight;
  float v_frate;
  char v_fourcc[5];
  bool v_bframes;

  // Parameters for audio tracks
  uint32_t a_channels, a_bps, a_formattag;
  float a_sfreq, a_osfreq;

  void *private_data;
  unsigned int private_size;

  unsigned char *headers[3];
  uint32_t header_sizes[3];

  int default_track;
  char *language;

  int64_t units_processed;

  int ok;

  generic_packetizer_c *packetizer;
} kax_track_t;

typedef struct {
  unsigned char *data;
  int length;
} buffer_t;

class kax_reader_c: public generic_reader_c {
private:
  int act_wchar;

  vector<kax_track_t *> tracks;

  int64_t tc_scale;
  uint32_t cluster_tc;

  mm_io_c *in;

  EbmlStream *es;
  EbmlElement *saved_l1, *saved_l2, *segment;

  KaxCluster *cluster;

  buffer_t **buffers;
  int num_buffers;

  float segment_duration, last_timecode, first_timecode;

  vector<kax_attachment_t> attachments;

public:
  kax_reader_c(track_info_t *nti) throw (error_c);
  virtual ~kax_reader_c();

  virtual int read(generic_packetizer_c *ptzr);

  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);

private:
  virtual int read_headers();
  virtual void create_packetizers();
  virtual kax_track_t *new_kax_track();
  virtual kax_track_t *find_track_by_num(uint32_t num, kax_track_t *c = NULL);
  virtual kax_track_t *find_track_by_uid(uint32_t uid, kax_track_t *c = NULL);
  virtual void verify_tracks();
  virtual int packets_available();
  virtual void handle_attachments(mm_io_c *io, EbmlStream *es,
                                  EbmlElement *l0, int64_t pos);
  virtual void handle_chapters(mm_io_c *io, EbmlStream *es,
                               EbmlElement *l0, int64_t pos);
  virtual int64_t get_queued_bytes();
};


#endif  // __R_MATROSKA_H
