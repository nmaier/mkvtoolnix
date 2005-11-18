/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   output handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __OUTPUT_CONTROL_H
#define __OUTPUT_CONTROL_H

#include "os.h"

#include "mkvmerge.h"
#include "pr_generic.h"
#include "smart_pointers.h"

namespace libmatroska {
  class KaxChapters;
  class KaxCues;
  class KaxSeekHead;
  class KaxSegment;
  class KaxTag;
  class KaxTags;
  class KaxTrackEntry;
  class KaxTracks;
  class KaxSegmentFamily;
  class KaxInfo;
};

using namespace std;
using namespace libmatroska;

class mm_io_c;
class generic_packetizer_c;
class generic_reader_c;

struct append_spec_t {
  int64_t src_file_id;
  int64_t src_track_id;
  int64_t dst_file_id;
  int64_t dst_track_id;
};

struct packetizer_t {
  file_status_e status, old_status;
  packet_cptr pack;
  generic_packetizer_c *packetizer, *orig_packetizer;
  int64_t file, orig_file;
  bool deferred;

  packetizer_t():
    status(FILE_STATUS_MOREDATA), old_status(FILE_STATUS_MOREDATA),
    packetizer(NULL), orig_packetizer(NULL),
    file(0), orig_file(0),
    deferred(false) { }
};

struct deferred_connection_t {
  append_spec_t amap;
  packetizer_t *ptzr;
};

struct filelist_t {
  string name;
  int64_t size;

  file_type_e type;

  packet_cptr pack;

  generic_reader_c *reader;

  track_info_c *ti;
  bool appending, appended_to, done;

  int num_unfinished_packetizers, old_num_unfinished_packetizers;
  vector<deferred_connection_t> deferred_connections;
  int64_t deferred_max_timecode_seen;

  filelist_t():
    size(0), type(FILE_TYPE_IS_UNKNOWN),
    reader(NULL),
    ti(NULL), appending(false), appended_to(false), done(false),
    num_unfinished_packetizers(0), old_num_unfinished_packetizers(0),
    deferred_max_timecode_seen(-1) {}
};

struct attachment_t {
  string name, stored_name, mime_type, description;
  int64_t id;
  bool to_all_files;
  counted_ptr<buffer_t> data;

  attachment_t() {
    clear();
  }
  void clear() {
    name = "";
    stored_name = "";
    mime_type = "";
    description = "";
    id = 0;
    to_all_files = false;
    data = counted_ptr<buffer_t>(NULL);
  }
};

struct track_order_t {
  int64_t file_id;
  int64_t track_id;
};

enum timecode_scale_mode_e {
  TIMECODE_SCALE_MODE_NORMAL = 0,
  TIMECODE_SCALE_MODE_FIXED,
  TIMECODE_SCALE_MODE_AUTO
};

class family_uids_c: public vector<bitvalue_c> {
public:
  bool add_family_uid(const KaxSegmentFamily &family);
};

extern vector<packetizer_t> packetizers;
extern vector<filelist_t> files;
extern vector<attachment_t> attachments;
extern vector<track_order_t> track_order;
extern vector<append_spec_t> append_mapping;

extern string outfile;

extern double timecode_scale;
extern timecode_scale_mode_e timecode_scale_mode;

extern bitvalue_c *seguid_link_previous, *seguid_link_next, *seguid_forced;
extern family_uids_c segfamily_uids;

extern KaxInfo *kax_info_chap;

extern bool write_meta_seek_for_clusters;

extern string chapter_file_name;
extern string chapter_language;
extern string chapter_charset;

extern string segmentinfo_file_name;

extern KaxTags *tags_from_cue_chapters;

extern KaxSegment *kax_segment;
extern KaxTracks kax_tracks;
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

extern int64_t max_ns_per_cluster;
extern int max_blocks_per_cluster;
extern int default_tracks[3], default_tracks_priority[3];

extern bool splitting;
extern int split_max_num_files;

extern double timecode_scale;

void get_file_type(filelist_t &file);
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

int64_t add_attachment(attachment_t attachment);

#endif // __OUTPUT_CONTROL_H
