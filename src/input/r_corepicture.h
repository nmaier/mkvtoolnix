/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the CorePanorama video reader

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __R_COREPICTURE_H
#define __R_COREPICTURE_H

#include "os.h"

#include <expat.h>
#include <setjmp.h>

#include <vector>

#include "mm_io.h"
#include "common.h"
#include "pr_generic.h"
#include "xml_element_parser.h"

enum corepicture_pic_type {
  COREPICTURE_TYPE_JPEG = 0,
  COREPICTURE_TYPE_PNG  = 1,
  COREPICTURE_TYPE_UNKNOWN
};

enum corepicture_panorama_type {
  COREPICTURE_PAN_FLAT       = 0,
  COREPICTURE_PAN_BASIC      = 1,
  COREPICTURE_PAN_WRAPAROUND = 2,
  COREPICTURE_PAN_SPHERICAL  = 3,
  COREPICTURE_PAN_UNKNOWN
};

const uint32 COREPICTURE_USE_JPEG = (1 << 0);
const uint32 COREPICTURE_USE_PNG  = (1 << 1);

struct corepicture_pic_t {
  int64_t m_time, m_end_time;
  string m_url;
  corepicture_pic_type m_pic_type;
  corepicture_panorama_type m_pan_type;

  corepicture_pic_t():
    m_time(-1), m_end_time(-1), m_pic_type(COREPICTURE_TYPE_UNKNOWN),
    m_pan_type(COREPICTURE_PAN_UNKNOWN)
  {}

  bool is_valid() const {
    return (COREPICTURE_TYPE_UNKNOWN != m_pic_type) && (COREPICTURE_PAN_UNKNOWN != m_pan_type) && (m_url != "") && (0 <= m_time);
  }

  bool operator <(const corepicture_pic_t &cmp) const {
    return m_time < cmp.m_time;
  }
};

class corepicture_reader_c: public generic_reader_c, public xml_parser_c {
private:
  int m_ptzr;

  vector<string> m_parents;
  int m_width, m_height;
  vector<corepicture_pic_t> m_pictures;
  vector<corepicture_pic_t>::const_iterator m_current_picture;

public:
  corepicture_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~corepicture_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_text_io_c *io, int64_t size);

  virtual void start_element_cb(const char *name, const char **atts);
  virtual void end_element_cb(const char *name);

private:
  virtual int64_t try_to_parse_timecode(const char *s);
};

#endif  // __R_COREPICTURE_H
