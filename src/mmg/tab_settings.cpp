/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_input.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "settings" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"
#include "wx/config.h"

#include "mmg.h"
#include "common.h"
#include "matroskalogo_big.xpm"

tab_settings::tab_settings(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  new wxStaticBox(this, -1, _("mkvmrge executable"), wxPoint(10, 5),
                  wxSize(475, 50));
  tc_mkvmerge =
    new wxTextCtrl(this, ID_TC_MKVMERGE, _(""), wxPoint(15, 25),
                   wxSize(370, -1), wxTE_READONLY);

  new wxButton(this, ID_B_BROWSEMKVMERGE, _("Browse"), wxPoint(395, 25),
               wxDefaultSize, 0);

  new wxStaticBox(this, -1, _("About"), wxPoint(10, 350),
                  wxSize(475, 104));
  new wxStaticBitmap(this, -1, wxBitmap(matroskalogo_big_xpm),
                     wxPoint(20, 370), wxSize(64,64));
  new wxStaticText(this, wxID_STATIC,
                   _("mkvmerge GUI v" VERSION "\n"
                     "This GUI was written by Moritz Bunkus <moritz@"
                     "bunkus.org>\n"
                     "Based on mmg by Florian Wagner <flo.wagner@gmx.de>\n"
                     "mkvmerge GUI is licensed under the GPL.\n"
                     "http://www.bunkus.org/videotools/mkvtoolnix/"),
                   wxPoint(95, 360), wxDefaultSize, 0);

  load_preferences();
}

tab_settings::~tab_settings() {
//   delete wxConfigBase::Get();
}

void tab_settings::on_browse(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose the mkvmerge executable",
                   tc_mkvmerge->GetValue().BeforeLast(PSEP), "",
#ifdef SYS_WINDOWS
                   _T("Executable files (*.exe)|*.exe|" ALLFILES),
#else
                   _T("All files (*)|*"),
#endif
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    tc_mkvmerge->SetValue(dlg.GetPath());
    mkvmerge_path = dlg.GetPath();
    save_preferences();
  }
}

void tab_settings::load_preferences() {
  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();

  cfg->SetPath("/GUI");
  if (!cfg->Read("mkvmerge_executable", &mkvmerge_path))
    mkvmerge_path = "mkvmerge";
  tc_mkvmerge->SetValue(mkvmerge_path);
}

void tab_settings::save_preferences() {
  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();
  cfg->Write("/GUI/mkvmerge_executable", tc_mkvmerge->GetValue());
  cfg->Flush();
}

void tab_settings::save(wxConfigBase *cfg) {
}

void tab_settings::load(wxConfigBase *cfg) {
}

bool tab_settings::validate_settings() {
  return true;
}

IMPLEMENT_CLASS(tab_settings, wxPanel);
BEGIN_EVENT_TABLE(tab_settings, wxPanel)
  EVT_BUTTON(ID_B_BROWSEMKVMERGE, tab_settings::on_browse)
END_EVENT_TABLE();
