/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_WINDOW_GEOMETRY_SAVER_H
#define MTX_MMG_WINDOW_GEOMETRY_SAVER_H

#include "common/common_pch.h"

#include <boost/optional.hpp>

class wxString;
class wxWindow;

class window_geometry_saver_c {
protected:
  wxWindow *m_window;
  std::string m_name;
  boost::optional<unsigned int> m_x, m_y, m_width, m_height;
  bool m_set_as_min_size;

public:
  window_geometry_saver_c(wxWindow *window, std::string const &name);
  ~window_geometry_saver_c();

  window_geometry_saver_c(window_geometry_saver_c const &)             = delete;
  window_geometry_saver_c &operator =(window_geometry_saver_c const &) = delete;

  window_geometry_saver_c &set_default_position(unsigned int x, unsigned int y);
  window_geometry_saver_c &set_default_size(unsigned int width, unsigned int height, bool set_as_min_size);
  void restore();
  void save() const;

protected:
  wxString get_config_group() const;
};

#endif
