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
vector<mmg_file_t> files;

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

wxString extract_language_code(wxString &source) {
  wxString copy;
  int pos;

  copy = source;
  if ((pos = copy.Find(" (")) >= 0)
    copy.Remove(pos);

  return copy;
}

wxString shell_escape(wxString &source) {
  uint32_t i;
  wxString escaped;

  for (i = 0; i < source.Length(); i++) {
    if (source[i] == '"')
      escaped += "\\\"";
    else if (source[i] == '\\')
      escaped += "\\\\";
    else if (source[i] == '\n')
      escaped += " ";
    else
      escaped += source[i];
  }

  return escaped;
}

mmg_dialog::mmg_dialog(): wxFrame(NULL, -1, "mkvmerge GUI v" VERSION,
                                  wxPoint(0, 0), wxSize(520, 700),
                                  wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  wxBoxSizer *bs_main = new wxBoxSizer(wxVERTICAL);
  this->SetSizer(bs_main);
  this->SetAutoLayout(true);

  wxNotebook *notebook =
    new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  tab_input *input_page = new tab_input(notebook);
  tab_attachments *attachments_page = new tab_attachments(notebook);

  notebook->AddPage(input_page, _("Input"));
  notebook->AddPage(attachments_page, _("Attachments"));

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
                   wxSize(490, 50), wxTE_READONLY | wxTE_LINEWRAP |
                   wxTE_MULTILINE);
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

  last_open_dir = "";
  cmdline = "mkvmerge -o \"" + tc_output->GetValue() + "\" ";
  tc_cmdline->SetValue(cmdline);
  cmdline_timer.SetOwner(this, ID_T_UPDATECMDLINE);
  cmdline_timer.Start(1000);
}

void mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", "", "",
                   _T("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                      "All Files (*.*)|*.*"), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK)
    tc_output->SetValue(dlg.GetPath());
}

void mmg_dialog::on_run(wxCommandEvent &evt) {
  update_command_line();
}

void mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
}

void mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
}

wxString &mmg_dialog::get_command_line() {
  return cmdline;
}

void mmg_dialog::on_update_command_line(wxTimerEvent &evt) {
  update_command_line();
}

void mmg_dialog::update_command_line() {
  uint32_t fidx, tidx;
  bool tracks_present_here, tracks_present;
  bool no_audio, no_video, no_subs;
  mmg_file_t *f;
  mmg_track_t *t;
  mmg_attachment_t *a;
  wxString sid, old_cmdline;

  old_cmdline = cmdline;
  cmdline = "mkvmerge -o \"" + tc_output->GetValue() + "\" ";

  tracks_present = false;
  for (fidx = 0; fidx < files.size(); fidx++) {
    f = &files[fidx];
    tracks_present_here = false;
    no_audio = true;
    no_video = true;
    no_subs = true;
    for (tidx = 0; tidx < f->tracks->size(); tidx++) {
      t = &(*f->tracks)[tidx];
      if (!t->enabled)
        continue;

      tracks_present_here = true;
      sid.Printf("%lld", t->id);

      if (t->type == 'a') {
        no_audio = false;
        cmdline += "-a " + sid + " ";

        if (t->aac_is_sbr)
          cmdline += "--aac-is-sbr " + sid + " ";

      } else if (t->type == 'v') {
        no_video = false;
        cmdline += "-d " + sid + " ";

      } else if (t->type == 's') {
        no_subs = false;
        cmdline += "-s " + sid + " ";

        if (t->sub_charset->Length() > 0)
          cmdline += "--sub-charset \"" + sid + ":" +
            shell_escape(*t->sub_charset) + "\" ";
      }

      if (*t->language != "none")
        cmdline += "--language " + sid + ":" +
          extract_language_code(*t->language) + " ";

      if (*t->cues != "default") {
        cmdline += "--cues " + sid + ":";
        if (*t->cues == "only for I frames")
          cmdline += "iframes ";
        else if (*t->cues == "for all frames")
          cmdline += "all ";
        else if (*t->cues == "none")
          cmdline += "none ";
        else
          mxerror("Unknown cues option '%s'. Should not have happened.\n",
                  t->cues->c_str());
      }

      if ((t->delay->Length() > 0) || (t->stretch->Length() > 0)) {
        cmdline += "--sync " + sid + ":";
        if (t->delay->Length() > 0)
          cmdline += *t->delay;
        else
          cmdline += "0";
        if (t->stretch->Length() > 0)
          cmdline += "," + *t->stretch;
        cmdline += " ";
      }

      if (t->track_name->Length() > 0)
        cmdline += "--track-name \"" + sid + ":" +
          shell_escape(*t->track_name) + "\" ";

      if (t->default_track)
        cmdline += "--default-track " + sid + " ";

      if (t->tags->Length() > 0)
        cmdline += "--tags \"" + sid + ":" +
          shell_escape(*t->tags) + "\" ";
    }

    if (tracks_present_here) {
      tracks_present = true;

      if (f->no_chapters)
        cmdline += "--no-chapters ";
      cmdline += "\"" + *f->file_name + "\" ";
    }
  }

  for (fidx = 0; fidx < attachments.size(); fidx++) {
    a = &attachments[fidx];

    cmdline += "--attachment-mime-type \"" +
      shell_escape(*a->mime_type) + "\" ";
    if (a->description->Length() > 0)
      cmdline += "--attachment-description \"" +
        shell_escape(*a->description) + "\" ";
    if (a->style == 0)
      cmdline += "--attach-file \"";
    else
      cmdline += "--attach-file-once \"";
    cmdline += shell_escape(*a->file_name) + "\" ";
  }

  if (old_cmdline != cmdline)
    tc_cmdline->SetValue(cmdline);
}

IMPLEMENT_CLASS(mmg_dialog, wxFrame);
BEGIN_EVENT_TABLE(mmg_dialog, wxFrame)
  EVT_BUTTON(ID_B_BROWSEOUTPUT, mmg_dialog::on_browse_output)
  EVT_BUTTON(ID_B_RUN, mmg_dialog::on_run)
  EVT_BUTTON(ID_B_SAVECMDLINE, mmg_dialog::on_save_cmdline)
  EVT_BUTTON(ID_B_COPYTOCLIPBOARD, mmg_dialog::on_copy_to_clipboard)
  EVT_TIMER(ID_T_UPDATECMDLINE, mmg_dialog::on_update_command_line)
END_EVENT_TABLE();

class mmg_app: public wxApp {
protected:
  mmg_dialog *mdlg;

public:
  virtual bool OnInit();
};

bool mmg_app::OnInit() {
  mdlg = new mmg_dialog();
  mdlg->Show(true);

  return true;
}

IMPLEMENT_APP(mmg_app)
