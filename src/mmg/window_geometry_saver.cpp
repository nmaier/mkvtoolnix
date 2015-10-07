/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/confbase.h>
#include <wx/window.h>

#include "common/wx.h"
#include "mmg/window_geometry_saver.h"

window_geometry_saver_c::window_geometry_saver_c(wxWindow *window,
                                                 std::string const &name)
  : m_window{window}
  , m_name{name}
  , m_set_as_min_size{false}
{
}

window_geometry_saver_c::~window_geometry_saver_c() {
  save();
}

window_geometry_saver_c &
window_geometry_saver_c::set_default_position(unsigned int x,
                                              unsigned int y) {
  m_x.reset(x);
  m_y.reset(y);

  return *this;
}

window_geometry_saver_c &
window_geometry_saver_c::set_default_size(unsigned int width,
                                          unsigned int height,
                                          bool set_as_min_size) {
  m_width.reset(width);
  m_height.reset(height);
  m_set_as_min_size = set_as_min_size;

  return *this;
}

void
window_geometry_saver_c::restore() {
  int x      = 0, y = 0;
  bool valid = false;
  auto cfg   = wxConfigBase::Get();

  cfg->SetPath(get_config_group());

  // Position
  if (cfg->Read(wxT("x"), &x, 0) && cfg->Read(wxT("y"), &y, 0) && (0 <= x) && (0 <= y))
    valid = true;

  else if (m_x && m_y) {
    x     = *m_x;
    y     = *m_y;
    valid = true;
  }

  if (valid) {
    x = std::min<int>(x, wxSystemSettings::GetMetric(wxSYS_SCREEN_X) - 50);
    y = std::min<int>(y, wxSystemSettings::GetMetric(wxSYS_SCREEN_Y) - 32);

    m_window->Move(x, y);
  }

  // Size
  valid = false;
  if (cfg->Read(wxT("width"), &x, 0) && cfg->Read(wxT("height"), &y, 0))
    valid = true;

  else if (m_width && m_height) {
    x     = *m_width;
    y     = *m_height;
    valid = true;
  }

  if (m_width && m_height && m_set_as_min_size)
    m_window->SetMinSize(wxSize{static_cast<int>(*m_width), static_cast<int>(*m_height)});

  if (valid) {
    x = std::max(std::min<int>(x, wxSystemSettings::GetMetric(wxSYS_SCREEN_X)), 100);
    y = std::max(std::min<int>(y, wxSystemSettings::GetMetric(wxSYS_SCREEN_Y)), 100);

    m_restored_width  = x;
    m_restored_height = y;

    m_window->SetSize(x, y);
  }
}

void
window_geometry_saver_c::save()
  const {

  auto rect = m_window->GetRect();
  auto cfg  = wxConfigBase::Get();

  cfg->SetPath(get_config_group());

  if ((rect.GetX() >= 0) && (rect.GetY() >= 0)) {
    cfg->Write(wxT("x"), rect.GetX());
    cfg->Write(wxT("y"), rect.GetY());
  }

  if ((rect.GetWidth() > 0) && (rect.GetHeight() > 0)) {
    cfg->Write(wxT("width"),  get_value_for_saving(rect.GetWidth(),  m_restored_width));
    cfg->Write(wxT("height"), get_value_for_saving(rect.GetHeight(), m_restored_height));
  }
}

wxString
window_geometry_saver_c::get_config_group()
  const {
  return wxU(boost::format("/GUI/geometry/%1%") % m_name);
}

int
window_geometry_saver_c::get_value_for_saving(int current_value,
                                              boost::optional<unsigned int> restored_value) {
  // wxWidgets is... interesting. When setting the size with SetSize()
  // upon restoration the window ends up one pixel wider than the
  // values I specified. So adjust for that here.

  if (restored_value && (static_cast<int>(*restored_value + 1) == current_value))
    return *restored_value;
  return current_value;
}
