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
#include "wx/clipbrd.h"
#include "wx/file.h"
#include "wx/confbase.h"
#include "wx/fileconf.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"

wxApp *app;
wxString last_open_dir;
wxString mkvmerge_path;
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
                                  wxPoint(0, 0), wxSize(520, 718),
                                  wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  wxMenu *file_menu = new wxMenu();
  file_menu->Append(ID_M_FILE_LOAD, _T("&Load settings\tCtrl-L"),
                    _T("Load muxing settings from a file"));
  file_menu->Append(ID_M_FILE_SAVE, _T("&Save settings\tCtrl-S"),
                    _T("Save muxing settings to a file"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_EXIT, _T("E&xit\tCtrl-X"),
                    _T("Quit the application"));

  wxMenu *muxing_menu = new wxMenu();
  muxing_menu->Append(ID_M_MUXING_START,
                      _T("Sta&rt muxing (run mkvmerge)\tCtrl-R"),
                      _T("Run mkvmerge and start the muxing process"));
  muxing_menu->Append(ID_M_MUXING_COPY_CMDLINE,
                      _T("&Copy to clipboard\tCtrl-C"),
                      _T("Copy the command line to the clipboard"));
  muxing_menu->Append(ID_M_MUXING_SAVE_CMDLINE,
                      _T("Sa&ve command line\tCtrl-V"),
                      _T("Save the command line to a file"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HELP_ABOUT, _T("&About\tF1"),
                    _T("Show program information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(file_menu, _T("&File"));
  menu_bar->Append(muxing_menu, _T("&Muxing"));
  menu_bar->Append(help_menu, _T("&Help"));
  SetMenuBar(menu_bar);

  status_bar = new wxStatusBar(this, -1);
  SetStatusBar(status_bar);
  status_bar_timer.SetOwner(this, ID_T_STATUSBAR);

  wxBoxSizer *bs_main = new wxBoxSizer(wxVERTICAL);
  this->SetSizer(bs_main);
  this->SetAutoLayout(true);

  wxNotebook *notebook =
    new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  input_page = new tab_input(notebook);
  attachments_page = new tab_attachments(notebook);
  settings_page = new tab_settings(notebook);

  notebook->AddPage(input_page, _("Input"));
  notebook->AddPage(attachments_page, _("Attachments"));
  notebook->AddPage(settings_page, _("Settings"));

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

  last_open_dir = "";
  cmdline = mkvmerge_path + " -o \"" + tc_output->GetValue() + "\" ";
  tc_cmdline->SetValue(cmdline);
  cmdline_timer.SetOwner(this, ID_T_UPDATECMDLINE);
  cmdline_timer.Start(1000);

  set_status_bar("mkvmerge GUI ready");

  if (app->argc > 1) {
    wxString file;

    file = app->argv[1];
    if (!wxFileExists(file) || wxDirExists(file))
      wxMessageBox("The file does exist.", "Error loading settings",
                   wxOK | wxCENTER | wxICON_ERROR);
    else {
#ifdef SYS_WINDOWS
      if ((file.Length() > 3) && (file.c_str()[1] != ':') &&
          (file.c_str()[0] != '\\'))
        file = wxGetCwd() + "\\" + file;
#else
      if ((file.Length() > 0) && (file.c_str()[0] != '/'))
        file = wxGetCwd() + "/" + file;
#endif
      load(file);
    }
  }
}

void mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                      "All Files (*.*)|*.*"), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_output->SetValue(dlg.GetPath());
  }
}

void mmg_dialog::set_status_bar(wxString text) {
  status_bar_timer.Stop();
  status_bar->SetStatusText(text);
  status_bar_timer.Start(4000, true);
}

void mmg_dialog::on_clear_status_bar(wxTimerEvent &evt) {
  status_bar->SetStatusText("");
}

void mmg_dialog::on_quit(wxCommandEvent &evt) {
  Close(true);
}

void mmg_dialog::on_file_load(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an input file", last_open_dir, "",
                   _T("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    if (!wxFileExists(dlg.GetPath()) || wxDirExists(dlg.GetPath())) {
      wxMessageBox("The file does exist.", "Error loading settings",
                   wxOK | wxCENTER | wxICON_ERROR);
      return;
    }
    last_open_dir = dlg.GetDirectory();
    load(dlg.GetPath());
  }
}

void mmg_dialog::load(wxString file_name) {
  wxFileConfig *cfg;
  wxString s;
  int version;

  cfg = new wxFileConfig("mkvmerge GUI", "Moritz Bunkus", file_name);
  cfg->SetPath("/mkvmergeGUI");
  if (!cfg->Read("file_version", &version) || (version != 1)) {
    wxMessageBox("The file does not seem to be a valid mkvmerge GUI "
                 "settings file.", "Error loading settings",
                 wxOK | wxCENTER | wxICON_ERROR);
    return;
  }
  cfg->Read("output_file_name", &s);
  tc_output->SetValue(s);

  input_page->load(cfg);
  attachments_page->load(cfg);
  settings_page->load(cfg);

  delete cfg;

  set_status_bar("Configuration loaded.");
}

void mmg_dialog::on_file_save(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    save(dlg.GetPath());
  }
}

void mmg_dialog::save(wxString file_name) {
  wxFileConfig *cfg;

  cfg = new wxFileConfig("mkvmerge GUI", "Moritz Bunkus", file_name);
  cfg->SetPath("/mkvmergeGUI");
  cfg->Write("file_version", 1);
  cfg->Write("gui_version", VERSION);
  cfg->Write("output_file_name", tc_output->GetValue());

  input_page->save(cfg);
  attachments_page->save(cfg);
  settings_page->save(cfg);

  delete cfg;

  set_status_bar("Configuration saved.");
}

void mmg_dialog::on_run(wxCommandEvent &evt) {
  update_command_line();
}

void mmg_dialog::on_about(wxCommandEvent &evt) {
  wxMessageBox(_("mkvmerge GUI v" VERSION "\n"
                 "This GUI was written by Moritz Bunkus <moritz@bunkus.org>\n"
                 "Based on mmg by Florian Wagner <root@sirelvis.de>\n"
                 "mkvmerge GUI is licensed under the GPL.\n"
                 "http://www.bunkus.org/videotools/mkvtoolnix/\n"
                 "\n"
                 "Help is available in form of tool tips and the HTML "
                 "documentation\n"
                 "for mkvmerge, mkvmerge.html. It can also be viewed online "
                 "at\n"
                 "http://www.bunkus.org/videotools/mkvtoolnix/doc/"
                 "mkvmerge.html"),
               "About mkvmerge's GUI", wxOK | wxCENTER | wxICON_INFORMATION);
}

void mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T(ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = cmdline + "\n";
    file->Write(s);
    delete file;

    set_status_bar("Command line saved.");
  }
}

void mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
  update_command_line();
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(cmdline));
    wxTheClipboard->Close();
    set_status_bar("Command line copied to clipboard.");
  } else
    set_status_bar("Could not open the clipboard.");
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
  cmdline = mkvmerge_path + " -o \"" + tc_output->GetValue() + "\" ";

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
  EVT_TIMER(ID_T_UPDATECMDLINE, mmg_dialog::on_update_command_line)
  EVT_TIMER(ID_T_STATUSBAR, mmg_dialog::on_clear_status_bar)
  EVT_MENU(ID_M_FILE_EXIT, mmg_dialog::on_quit)
  EVT_MENU(ID_M_FILE_LOAD, mmg_dialog::on_file_load)
  EVT_MENU(ID_M_FILE_SAVE, mmg_dialog::on_file_save)
  EVT_MENU(ID_M_MUXING_START, mmg_dialog::on_run)
  EVT_MENU(ID_M_MUXING_COPY_CMDLINE, mmg_dialog::on_copy_to_clipboard)
  EVT_MENU(ID_M_MUXING_SAVE_CMDLINE, mmg_dialog::on_save_cmdline)
  EVT_MENU(ID_M_HELP_ABOUT, mmg_dialog::on_about)
END_EVENT_TABLE();

class mmg_app: public wxApp {
protected:
  mmg_dialog *mdlg;

public:
  virtual bool OnInit();
};

bool mmg_app::OnInit() {
  app = this;
  mdlg = new mmg_dialog();
  mdlg->Show(true);

  return true;
}

IMPLEMENT_APP(mmg_app)
