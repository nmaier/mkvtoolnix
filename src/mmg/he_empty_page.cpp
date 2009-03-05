/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: empty page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "he_empty_page.h"

he_empty_page_c::he_empty_page_c(wxTreebook *parent,
                                 const wxString &title,
                                 const wxString &content)
  : he_page_base_c(parent)
  , m_title(title)
  , m_content(content)
{
  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);

  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, m_title),   0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(this),                      0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, m_content), 0, wxGROW | wxLEFT | wxRIGHT, 5);
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

