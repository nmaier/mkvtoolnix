/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: empty page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "common/wx.h"
#include "mmg/header_editor/empty_page.h"

he_empty_page_c::he_empty_page_c(header_editor_frame_c *parent,
                                 const translatable_string_c &title,
                                 const translatable_string_c &content)
  : he_page_base_c(parent, title)
  , m_content(content)
{
  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);
  m_st_title      = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_st_content    = new wxStaticText(this, wxID_ANY, wxEmptyString);

  siz->AddSpacer(5);
  siz->Add(m_st_title,             0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(this), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(m_st_content,           0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddStretchSpacer();

  SetSizer(siz);
}

he_empty_page_c::~he_empty_page_c() {
}

bool
he_empty_page_c::has_this_been_modified() {
  return false;
}

bool
he_empty_page_c::validate_this() {
  return true;
}

void
he_empty_page_c::modify_this() {
}

void
he_empty_page_c::translate_ui() {
  m_st_title->SetLabel(wxU(m_title.get_translated()));
  m_st_content->SetLabel(wxU(m_content.get_translated()));
}
