/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   CorePanorama video reader

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include "common/endian.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/parsing.h"
#include "common/xml/xml.h"
#include "input/r_corepicture.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_video.h"


class corepicture_xml_find_root_c: public xml_parser_c {
public:
  std::string m_root_element;

public:
  corepicture_xml_find_root_c(mm_text_io_c *io):
    xml_parser_c(io) {
  }

  virtual void start_element_cb(const char *name, const char **atts) {
    if (m_root_element == "")
      m_root_element = name;
  }
};

int
corepicture_reader_c::probe_file(mm_text_io_c *io,
                                 uint64_t) {
  try {
    corepicture_xml_find_root_c root_finder(io);

    io->setFilePointer(0);
    while (root_finder.parse_one_xml_line() && (root_finder.m_root_element == ""))
      ;

    return (root_finder.m_root_element == "CorePanorama" ? 1 : 0);

  } catch(...) {
  }

  return 0;
}

corepicture_reader_c::corepicture_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  m_width(-1),
  m_height(-1) {

  try {
    m_xml_source = new mm_text_io_c(new mm_file_io_c(m_ti.m_fname));

    if (!corepicture_reader_c::probe_file(m_xml_source, 0))
      throw error_c(boost::format(Y("%1%: Source is not a valid %1% file.")) % get_format_name());

    parse_xml_file();

    delete m_xml_source;

    std::stable_sort(m_pictures.begin(), m_pictures.end());
    m_current_picture = m_pictures.begin();

  } catch (xml_parser_error_c &error) {
    throw error_c(error.get_error());

  } catch (mm_io_error_c &) {
    throw error_c(boost::format(Y("%1%: Could not open the source file.")) % get_format_name());
  }

  show_demuxer_info();
}

corepicture_reader_c::~corepicture_reader_c() {
}

void
corepicture_reader_c::start_element_cb(const char *name,
                                       const char **atts) {
  size_t i;
  std::string node;

  m_parents.push_back(name);

  // Generate the full path to this node.
  for (i = 0; m_parents.size() > i; ++i) {
    if (!node.empty())
      node += ".";
    node += m_parents[i];
  }

  if (node == "CorePanorama.Info") {
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)  {
      if (!strcasecmp(atts[i], "width") && (0 != atts[i + 1][0])) {
        if (!parse_int(atts[i + 1], m_width))
          m_width = -1;
      } else if (!strcasecmp(atts[i], "height") && (0 != atts[i + 1][0])) {
        if (!parse_int(atts[i + 1], m_height))
          m_height = -1;
      }
    }

  } else if (node == "CorePanorama.Picture") {
    corepicture_pic_t new_picture;
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2) {
      if (!strcasecmp(atts[i], "time") && (0 != atts[i + 1][0]))
        new_picture.m_time = try_to_parse_timecode(atts[i + 1]);

      else if (!strcasecmp(atts[i], "end") && (0 != atts[i + 1][0]))
        new_picture.m_end_time = try_to_parse_timecode(atts[i + 1]);

      else if (!strcasecmp(atts[i], "type") && (0 != atts[i + 1][0])) {
        if (!strcasecmp(atts[i + 1], "jpeg") || !strcasecmp(atts[i + 1], "jpg"))
          new_picture.m_pic_type = COREPICTURE_TYPE_JPEG;

        else if (!strcasecmp(atts[i + 1], "png"))
          new_picture.m_pic_type = COREPICTURE_TYPE_PNG;

        else
          mxwarn_tid(m_ti.m_fname, 0, boost::format(Y("The picture type '%1%' is not recognized.\n")) % atts[i + 1]);

      } else if (!strcasecmp(atts[i], "panorama") && (0 != atts[i + 1][0])) {
        if (!strcasecmp(atts[i + 1], "flat"))
          new_picture.m_pan_type = COREPICTURE_PAN_FLAT;

        else if (!strcasecmp(atts[i + 1], "pan"))
          new_picture.m_pan_type = COREPICTURE_PAN_BASIC;

        else if (!strcasecmp(atts[i + 1], "wraparound"))
          new_picture.m_pan_type = COREPICTURE_PAN_WRAPAROUND;

        else if (!strcasecmp(atts[i + 1], "spherical"))
          new_picture.m_pan_type = COREPICTURE_PAN_SPHERICAL;

        else
          mxwarn_tid(m_ti.m_fname, 0, boost::format(Y("The panoramic mode '%1%' is not recognized.\n")) % atts[i + 1]);

      } else if (!strcasecmp(atts[i], "url") && (0 != atts[i + 1][0]))
        new_picture.m_url = escape_xml(atts[i + 1]);
    }

    if (new_picture.is_valid())
      m_pictures.push_back(new_picture);
  }
}

void
corepicture_reader_c::end_element_cb(const char *name) {
  size_t i;
  std::string node;

  // Generate the full path to this node.
  for (i = 0; m_parents.size() > i; ++i) {
    if (!node.empty())
      node += ".";
    node += m_parents[i];
  }

  m_parents.pop_back();
}

void
corepicture_reader_c::create_packetizer(int64_t tid) {
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  uint8 private_buffer[5];
  uint32 codec_used = 0;
  std::vector<corepicture_pic_t>::const_iterator picture;

  private_buffer[0] = 0; // version 0

  mxforeach(picture, m_pictures) {
    if (COREPICTURE_TYPE_JPEG == picture->m_pic_type)
      codec_used |= COREPICTURE_USE_JPEG;
    else if (COREPICTURE_TYPE_PNG == picture->m_pic_type)
      codec_used |= COREPICTURE_USE_PNG;
  }
  put_uint32_be(&private_buffer[1], codec_used);

  m_ti.m_private_data = (unsigned char *)safememdup(private_buffer, sizeof(private_buffer));
  m_ti.m_private_size = sizeof(private_buffer);
  m_ptzr              = add_packetizer(new video_packetizer_c(this, m_ti, MKV_V_COREPICTURE, 0.0, m_width, m_height));

  show_packetizer_info(0, PTZR(m_ptzr));
}

file_status_e
corepicture_reader_c::read(generic_packetizer_c *ptzr,
                           bool) {

  if (m_current_picture != m_pictures.end()) {
    try {
      counted_ptr<mm_io_c> io(new mm_file_io_c(m_current_picture->m_url));
      uint64_t size  = io->get_size();
      binary *buffer = (binary *)safemalloc(7 + size);

      put_uint16_be(&buffer[0], 7);
      put_uint32_be(&buffer[2], m_current_picture->m_pan_type);

      buffer[6]           = m_current_picture->m_pic_type;
      uint32_t bytes_read = io->read(&buffer[7], size);

      if (0 != bytes_read) {
        int64_t duration = m_current_picture->m_end_time == -1 ? -1 : m_current_picture->m_end_time - m_current_picture->m_time;
        PTZR(m_ptzr)->process(new packet_t(new memory_c(buffer, 7 + bytes_read, false), m_current_picture->m_time, duration));
      }

    } catch(...) {
      mxerror_tid(m_ti.m_fname, 0, boost::format(Y("Impossible to use file '%1%': The file could not be opened for reading.\n")) % m_current_picture->m_url);
    }
    m_current_picture++;
  }

  return m_current_picture == m_pictures.end() ? flush_packetizer(m_ptzr) : FILE_STATUS_MOREDATA;
}

int
corepicture_reader_c::get_progress() {
  if (m_pictures.size() == 0)
    return 0;
  return 100 - std::distance(m_current_picture, (std::vector<corepicture_pic_t>::const_iterator)m_pictures.end()) * 100 / m_pictures.size();
}

int64_t
corepicture_reader_c::try_to_parse_timecode(const char *s) {
  int64_t timecode;

  if (!parse_timecode(s, timecode))
    throw xml_parser_error_c(Y("Invalid start timecode"), m_xml_parser);

  return timecode;
}

void
corepicture_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, "CorePicture");
}
