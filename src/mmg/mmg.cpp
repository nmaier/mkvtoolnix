/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mmg.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief main stuff
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"

wxString last_open_dir;
vector<mmg_file_t> *files;

mmg_dialog::mmg_dialog(): wxFrame(NULL, -1, "mkvmerge GUI", wxPoint(0, 0),
                                  wxSize(520, 700),
                                  wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  wxBoxSizer *bs_main = new wxBoxSizer(wxVERTICAL);
  this->SetSizer(bs_main);
  this->SetAutoLayout(true);

  wxNotebook *notebook =
    new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  tab_input *input_page = new tab_input(notebook);

  notebook->AddPage(input_page, _("Input"));

  bs_main->Add(notebook, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

  wxStaticBox *sb_low = new wxStaticBox(this, -1, _("Output filename"));
  wxStaticBoxSizer *sbs_low = new wxStaticBoxSizer(sb_low, wxHORIZONTAL);
  bs_main->Add(sbs_low, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

  tc_output =
    new wxTextCtrl(this, ID_TC_OUTPUT, _(""), wxDefaultPosition,
                   wxSize(400, -1), 0);
  sbs_low->Add(tc_output, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  b_browse_output =
    new wxButton(this, ID_B_BROWSEOUTPUT, _("Browse"), wxDefaultPosition,
                 wxDefaultSize, 0);
  sbs_low->Add(b_browse_output, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  wxStaticBox *sb_low2 = new wxStaticBox(this, -1, _("Command line"));
  wxStaticBoxSizer *sbs_low2 = new wxStaticBoxSizer(sb_low2, wxHORIZONTAL);
  bs_main->Add(sbs_low2, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

  tc_cmdline =
    new wxTextCtrl(this, ID_TC_CMDLINE, _(""), wxDefaultPosition,
                   wxSize(490, 50), wxTE_READONLY | wxTE_WORDWRAP);
  sbs_low2->Add(tc_cmdline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  wxBoxSizer *bs_low3 = new wxBoxSizer(wxHORIZONTAL);
  bs_main->Add(bs_low3, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 5);

  b_run = new wxButton(this, ID_B_RUN, _("Run"), wxDefaultPosition,
                       wxDefaultSize, 0);
  bs_low3->Add(b_run, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  bs_low3->Add(20, 5, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  b_save_cmdline = new wxButton(this, ID_B_SAVECMDLINE, _("Save command line"),
                                wxDefaultPosition, wxDefaultSize, 0);
  bs_low3->Add(b_save_cmdline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
  bs_low3->Add(20, 5, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  b_copy_to_clipboard = new wxButton(this, ID_B_COPYTOCLIPBOARD,
                                     _("Copy to clipboard"), wxDefaultPosition,
                                     wxDefaultSize, 0);
  bs_low3->Add(b_copy_to_clipboard, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

  files = new vector<mmg_file_t>;
  last_open_dir = "";
}

void mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", "", "",
                   _T("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                      "All Files (*.*)|*.*"), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK)
    tc_output->SetValue(dlg.GetPath());
}

void mmg_dialog::on_run(wxCommandEvent &evt) {
}

void mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
}

void mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
}

class mmg_app: public wxApp {
public:
  virtual bool OnInit();
};

bool mmg_app::OnInit() {
  mmg_dialog *dialog = new mmg_dialog();
  dialog->Show(true);

  return true;
}

wxString &break_line(wxString &line, int break_after) {
  uint32_t i, chars;
  wxString broken;

  for (i = 0, chars = 0; i < line.Length(); i++) {
    if (chars >= break_after) {
      if ((line[i] == ',') || (line[i] == '.') || (line[i] == '-')) {
        broken += line[i];
        broken += "\n";
        chars = 0;
      } else if (line[i] == ' ') {
        broken += "\n";
        chars = 0;
      } else if (line[i] == '(') {
        broken += "\n(";
        chars = 0;
      } else {
        broken += line[i];
        chars++;
      }
    } else if ((chars != 0) || (broken[i] != ' ')) {
      broken += line[i];
      chars++;
    }
  }

  line = broken;
  return line;
}

IMPLEMENT_CLASS(mmg_dialog, wxFrame);
BEGIN_EVENT_TABLE(mmg_dialog, wxFrame)
  EVT_BUTTON(ID_B_BROWSEOUTPUT, mmg_dialog::on_browse_output)

END_EVENT_TABLE();

IMPLEMENT_APP(mmg_app)
