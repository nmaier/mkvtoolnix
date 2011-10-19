/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the CorePanorama video reader

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __R_COREPICTURE_H
#define __R_COREPICTURE_H

#include "common/common_pch.h"

#include "common/corepicture.h"
#include "common/mm_io.h"
#include "common/xml/element_parser.h"
#include "merge/pr_generic.h"

class corepicture_reader_c: public generic_reader_c, public xml_parser_c {
private:
  int m_ptzr;

  std::vector<std::string> m_parents;
  int m_width, m_height;
  std::vector<corepicture_pic_t> m_pictures;
  std::vector<corepicture_pic_t>::const_iterator m_current_picture;

public:
  corepicture_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~corepicture_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("CorePanorama pictures") : "CorePanorama pictures";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();

  static int probe_file(mm_text_io_c *io, uint64_t size);

  virtual void start_element_cb(const char *name, const char **atts);
  virtual void end_element_cb(const char *name);

private:
  virtual int64_t try_to_parse_timecode(const char *s);
};

#endif  // __R_COREPICTURE_H
