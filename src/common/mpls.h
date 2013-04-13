/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  definitions and helper functions for BluRay playlist files (MPLS)

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MPLS_COMMON_H
#define MTX_COMMON_MPLS_COMMON_H

#include "common/common_pch.h"

#include <vector>

#include "common/bit_cursor.h"
#include "common/fourcc.h"
#include "common/timecode.h"

#define MPLS_FILE_MAGIC   fourcc_c("MPLS")
#define MPLS_FILE_MAGIC2A fourcc_c("0100")
#define MPLS_FILE_MAGIC2B fourcc_c("0200")

namespace mtx { namespace mpls {

class exception: public mtx::exception {
protected:
  std::string m_message;
public:
  explicit exception(std::string const &message)  : m_message(message)       { }
  explicit exception(boost::format const &message): m_message(message.str()) { }
  virtual ~exception() throw() { }

  virtual char const *what() const throw() {
    return m_message.c_str();
  }
};

struct header_t {
  fourcc_c type_indicator1, type_indicator2;
  unsigned int playlist_pos, chapter_pos, ext_pos;

  void dump() const;
};

struct stream_t {
  unsigned int stream_type, sub_path_id, sub_clip_id, pid, coding_type, format, rate, char_code;
  std::string language;

  void dump(std::string const &type) const;
};

struct stn_t {
  unsigned int num_video, num_audio, num_pg, num_ig, num_secondary_audio, num_secondary_video, num_pip_pg;
  std::vector<stream_t> audio_streams, video_streams, pg_streams;

  void dump() const;
};

struct play_item_t {
  std::string clip_id, codec_id;
  unsigned int connection_condition, stc_id;
  timecode_c in_time, out_time, relative_in_time;
  bool is_multi_angle;
  stn_t stn;

  void dump() const;
};

struct playlist_t {
  unsigned int list_count, sub_count;
  std::vector<play_item_t> items;
  timecode_c duration;

  void dump() const;
};

class parser_c {
protected:
  bool m_ok, m_debug;

  header_t m_header;
  playlist_t m_playlist;
  std::vector<timecode_c> m_chapters;

  bit_reader_cptr m_bc;

public:
  parser_c();
  virtual ~parser_c();

  virtual bool parse(mm_io_c *in);
  virtual bool is_ok() const {
    return m_ok;
  }

  virtual void dump() const;

  virtual playlist_t const &get_playlist() const {
    return m_playlist;
  }

  std::vector<timecode_c> const &get_chapters() const {
    return m_chapters;
  }

protected:
  virtual void parse_header();
  virtual void parse_playlist();
  virtual play_item_t parse_play_item();
  virtual stn_t parse_stn();
  virtual stream_t parse_stream();
  virtual void parse_chapters();
  virtual std::string read_string(unsigned int length);
};
typedef std::shared_ptr<parser_c> parser_cptr;

}}
#endif // MTX_COMMON_MPLS_COMMON_H
