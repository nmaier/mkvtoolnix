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
 * output handling
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __OUTPUT_CONTROL_H
#define __OUTPUT_CONTROL_H

#include "os.h"

#include "pr_generic.h"

namespace libmatroska {
  class KaxChapters;
  class KaxCues;
  class KaxSeekHead;
  class KaxSegment;
  class KaxTag;
  class KaxTags;
  class KaxTrackEntry;
  class KaxTracks;
};

using namespace std;
using namespace libmatroska;

class mm_io_c;
class generic_packetizer_c;
class generic_reader_c;

typedef struct {
  int64_t src_file_id;
  int64_t src_track_id;
  int64_t dst_file_id;
  int64_t dst_track_id;
} append_spec_t;

typedef struct packetizer_t {
  file_status_t status, old_status;
  packet_t *pack;
  generic_packetizer_c *packetizer, *orig_packetizer;
  int64_t file, orig_file;
  bool deferred;
} packetizer_t;

typedef struct {
  append_spec_t amap;
  packetizer_t *ptzr;
} deferred_connection_t;

typedef struct {
  char *name;

  int type;

  packet_t *pack;

  generic_reader_c *reader;

  track_info_c *ti;
  bool appending, appended_to, done;

  vector<deferred_connection_t> deferred_connections;
} filelist_t;

struct attachment_t {
  string name, mime_type, description;
  int64_t size;
  bool to_all_files;

  attachment_t() {
    clear();
  }
  void clear() {
    name = "";
    mime_type = "";
    description = "";
    size = 0;
    to_all_files = false;
  }
};

typedef struct {
  int64_t file_id;
  int64_t track_id;
} track_order_t;

enum timecode_scale_mode_t {
  timecode_scale_mode_normal = 0,
  timecode_scale_mode_fixed,
  timecode_scale_mode_auto
};

extern vector<packetizer_t> packetizers;
extern vector<filelist_t> files;
extern vector<attachment_t> attachments;
extern vector<track_order_t> track_order;
extern vector<append_spec_t> append_mapping;

extern string outfile;

extern double timecode_scale;
extern timecode_scale_mode_t timecode_scale_mode;

extern bitvalue_c *seguid_link_previous, *seguid_link_next;

extern bool write_meta_seek_for_clusters;

extern string chapter_file_name;
extern string chapter_language;
extern string chapter_charset;

extern KaxTags *tags_from_cue_chapters;

extern KaxSegment *kax_segment;
extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern KaxCues *kax_cues;
extern KaxSeekHead *kax_sh_main, *kax_sh_cues;
extern KaxChapters *kax_chapters;
extern int64_t tags_size;
extern string segment_title;
extern bool segment_title_set;
extern string default_language;

extern float video_fps;

extern generic_packetizer_c *video_packetizer;
extern bool write_cues, cue_writing_requested;
extern bool no_lacing, no_linking, use_durations;

extern bool identifying, identify_verbose;

extern int file_num;
extern bool splitting;

extern int64_t max_ns_per_cluster;
extern int max_blocks_per_cluster;
extern int default_tracks[3], default_tracks_priority[3];
extern int64_t split_after;
extern int split_max_num_files;
extern bool split_by_time;

extern double timecode_scale;

int get_file_type(const string &filename);
void create_readers();

void setup();
void cleanup();
void main_loop();

void add_packetizer_globally(generic_packetizer_c *packetizer);
void add_tags(KaxTag *tags);

void create_next_output_file();
void finish_file(bool last_file = false);
void rerender_track_headers();
string create_output_name();

#endif // __OUTPUT_CONTROL_H
