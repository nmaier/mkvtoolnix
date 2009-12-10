/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks and other items from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MKVEXTRACT_H
#define __MKVEXTRACT_H

extern "C" {
#include <avilib.h>
}

#include <ogg/ogg.h>
#include <vector>

#include <ebml/EbmlElement.h>

#include <matroska/KaxChapters.h>
#include <matroska/KaxTags.h>

#include "common/file_types.h"
#include "common/mm_io.h"
#include "librmff/librmff.h"

using namespace libebml;
using namespace libmatroska;

struct track_spec_t {
  enum target_mode_e {
    tm_normal,
    tm_raw,
    tm_full_raw
  };

  int64_t tid, tuid;
  std::string out_name;

  std::string sub_charset;
  bool embed_in_ogg;
  bool extract_cuesheet;

  target_mode_e target_mode;
  int extract_blockadd_level;

  bool done;

  track_spec_t();
};

#define in_parent(p)                                                                     \
  (   !p->IsFiniteSize()                                                                 \
   || (in->getFilePointer() < (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

// Helper functions in mkvextract.cpp
void show_element(EbmlElement *l, int level, const std::string &message);
inline void
show_element(EbmlElement *l,
             int level,
             const boost::format &format) {
  show_element(l, level, format.str());
}

void show_error(const std::string &error);
inline void
show_error(const boost::format &format) {
  show_error(format.str());
}

bool extract_tracks(const char *file_name, std::vector<track_spec_t> &tspecs);
void extract_tags(const char *file_name, bool parse_fully);
void extract_chapters(const char *file_name, bool chapter_format_simple, bool parse_fully);
void extract_attachments(const char *file_name, std::vector<track_spec_t> &tracks, bool parse_fully);
void extract_cuesheet(const char *file_name, bool parse_fully);
void write_cuesheet(const char *file_name, KaxChapters &chapters, KaxTags &tags, int64_t tuid, mm_io_c &out);
void extract_timecodes(const std::string &file_name, std::vector<track_spec_t> &tspecs, int version);

#endif // __MKVEXTRACT_H
