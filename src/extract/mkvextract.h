/*
 * mkvextract -- extract tracks from Matroska files into other files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * extracts tracks and other items from Matroska files into other files
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __MKVEXTRACT_H
#define __MKVEXTRACT_H

extern "C" {
#include <avilib.h>
}

#include <ogg/ogg.h>

#include <vector>

#include <ebml/EbmlElement.h>

#include "librmff.h"
#include "mm_io.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

class ssa_line_c {
public:
  char *line;
  int num;

  bool operator < (const ssa_line_c &cmp) const;
};

typedef struct {
  char *out_name;

  mm_io_c *out;
  avi_t *avi;
  ogg_stream_state osstate;
  rmff_track_t *rmtrack;

  int64_t tid, tuid;
  bool in_use, done;

  char track_type;
  int type;

  char *codec_id;
  void *private_data;
  int private_size;

  float a_sfreq;
  int a_channels, a_bps;

  float v_fps;
  int v_width, v_height;

  int64_t default_duration;

  int srt_num;
  char *sub_charset;
  int conv_handle;
  vector<ssa_line_c> ssa_lines;
  bool warning_printed;

  wave_header wh;
  int64_t bytes_written;

  unsigned char *buffered_data;
  int buffered_size;
  int64_t packetno, last_end, subpacketno;
  int header_sizes[3];
  unsigned char *headers[3];

  int aac_id, aac_profile, aac_srate_idx;

  bool embed_in_ogg;
  bool extract_cuesheet;

  // Needed for TTA.
  vector<int64_t> frame_sizes;
  int64_t last_duration;
} kax_track_t;

extern vector<kax_track_t> tracks;
extern char typenames[TYPEMAX + 1][20];

#define fits_parent(l, p) (l->GetElementPosition() < \
                           (p->GetElementPosition() + p->ElementSize()))
#define in_parent(p) (in->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

// Helper functions in mkvextract.cpp
void show_element(EbmlElement *l, int level, const char *fmt, ...);
void show_error(const char *fmt, ...);
kax_track_t *find_track(int tid);

bool extract_tracks(const char *file_name);
extern int conv_utf8;
void extract_tags(const char *file_name, bool parse_fully);
void extract_chapters(const char *file_name, bool chapter_format_simple,
                      bool parse_fully);
void extract_attachments(const char *file_name, bool parse_fully);
void extract_cuesheet(const char *file_name, bool parse_fully);
void write_cuesheet(const char *file_name, KaxChapters &chapters,
                    KaxTags &tags, int64_t tuid, mm_io_c &out);

#endif // __MKVEXTRACT_H
