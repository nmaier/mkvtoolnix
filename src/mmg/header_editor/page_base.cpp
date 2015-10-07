/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: abstract base class for all pages

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>

#include "common/wx.h"
#include "mmg/header_editor/frame.h"
#include "mmg/header_editor/page_base.h"

he_page_base_c::he_page_base_c(header_editor_frame_c *parent,
                               const translatable_string_c &title)
  : wxPanel(parent->get_page_panel(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
  , m_parent(parent)
  , m_page_id()
  , m_title(title)
{
  Hide();
}

he_page_base_c::~he_page_base_c() {
}

bool
he_page_base_c::has_been_modified() {
  if (has_this_been_modified())
    return true;

  for (auto child : m_children)
    if (child->has_been_modified())
      return true;

  return false;
}

void
he_page_base_c::do_modifications() {
  modify_this();

  for (auto child : m_children)
    child->do_modifications();
}

wxTreeItemId
he_page_base_c::validate() {
  if (!validate_this())
    return m_page_id;

  for (auto child : m_children) {
    wxTreeItemId result = child->validate();
    if (result.IsOk())
      return result;
  }

  return wxTreeItemId();
}

wxString
he_page_base_c::get_title() {
  return wxU(m_title.get_translated());
}
