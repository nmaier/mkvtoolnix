/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

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

#include "librmff.h"
#include "mkvmerge.h"
#include "mm_io.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

struct track_spec_t {
  int64_t tid, tuid;
  char *out_name;

  char *sub_charset;
  bool embed_in_ogg;
  bool extract_cuesheet;

  int extract_raw;
  int extract_blockadd_level;

  bool done;
};

extern bool no_variable_data;

#define in_parent(p) \
  (!p->IsFiniteSize() || \
   (in->getFilePointer() < \
    (p->GetElementPosition() + p->HeadSize() + p->GetSize())))

// Helper functions in mkvextract.cpp
void show_element(EbmlElement *l, int level, const char *fmt, ...);
void show_error(const char *fmt, ...);

bool extract_tracks(const char *file_name, vector<track_spec_t> &tspecs);
void extract_tags(const char *file_name, bool parse_fully);
void extract_chapters(const char *file_name, bool chapter_format_simple,
                      bool parse_fully);
void extract_attachments(const char *file_name,
                         vector<track_spec_t> &tracks, bool parse_fully);
void extract_cuesheet(const char *file_name, bool parse_fully);
void write_cuesheet(const char *file_name, KaxChapters &chapters,
                    KaxTags &tags, int64_t tuid, mm_io_c &out);

#endif // __MKVEXTRACT_H
