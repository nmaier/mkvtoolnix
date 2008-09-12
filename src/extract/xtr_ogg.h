/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_OGG_H
#define __XTR_OGG_H

#include "os.h"

#include <ogg/ogg.h>

#include "kate_common.h"
#include "theora_common.h"
#include "xtr_base.h"

class xtr_flac_c: public xtr_base_c {
public:
  xtr_flac_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
};

class xtr_oggbase_c: public xtr_base_c {
public:
  ogg_stream_state os;
  int packetno;
  memory_cptr buffered_data;
  int sfreq;
  int64_t previous_end;

public:
  xtr_oggbase_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);
  virtual ~xtr_oggbase_c();

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual void write_pages();
  virtual void flush_pages();
};

class xtr_oggflac_c: public xtr_oggbase_c {
public:
  xtr_oggflac_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);

  virtual const char *get_container_name() {
    return "Ogg (FLAC in Ogg)";
  };
};

class xtr_oggvorbis_c: public xtr_oggbase_c {
public:
  xtr_oggvorbis_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);

  virtual const char *get_container_name() {
    return "Ogg (Vorbis in Ogg)";
  };
};

class xtr_oggkate_c: public xtr_oggbase_c {
public:
  xtr_oggkate_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "Ogg (Kate in Ogg)";
  };

private:
  kate_identification_header_t kate_id_header;
};

class xtr_oggtheora_c: public xtr_oggbase_c {
public:
  xtr_oggtheora_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "Ogg (Theora in Ogg)";
  };

private:
  theora_identification_header_t theora;
  int64_t keyframe_number, non_keyframe_number;
};

#endif
