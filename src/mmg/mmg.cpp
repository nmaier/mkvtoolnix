/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mmg.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

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

#include <unistd.h>

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/clipbrd.h"
#include "wx/file.h"
#include "wx/config.h"
#include "wx/fileconf.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#include "chapters.h"
#include "common.h"
#include "jobs.h"
#if defined(SYS_UNIX) || defined(SYS_APPLE)
#include "matroskalogo_big.xpm"
#endif
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"
#include "tab_advanced.h"
#include "tab_attachments.h"
#include "tab_chapters.h"
#include "tab_input.h"
#include "tab_global.h"
#include "tab_settings.h"

mmg_app *app;
mmg_dialog *mdlg;
wxString last_open_dir;
wxString mkvmerge_path;
vector<wxString> last_settings;
vector<wxString> last_chapters;
vector<mmg_file_t> files;
map<wxString, wxString, lt_wxString> capabilities;
vector<job_t> jobs;

wxString &
break_line(wxString &line,
           int break_after) {
  uint32_t i, chars;
  wxString broken;

  for (i = 0, chars = 0; i < line.Length(); i++) {
    if (chars >= break_after) {
      if ((line[i] == wxC(',')) || (line[i] == wxC('.')) ||
          (line[i] == wxC('-'))) {
        broken += line[i];
        broken += wxS("\n");
        chars = 0;
      } else if (line[i] == wxC(' ')) {
        broken += wxS("\n");
        chars = 0;
      } else if (line[i] == wxC('(')) {
        broken += wxS("\n(");
        chars = 0;
      } else {
        broken += line[i];
        chars++;
      }
    } else if ((chars != 0) || (broken[i] != wxC(' '))) {
      broken += line[i];
      chars++;
    }
  }

  line = broken;
  return line;
}

wxString
extract_language_code(wxString source) {
  wxString copy;
  int pos;

  if (source.Find(wxS("---")) == 0)
    return wxS("---");

  copy = source;
  if ((pos = copy.Find(wxS(" ("))) >= 0)
    copy.Remove(pos);

  return copy;
}

wxString
shell_escape(wxString source) {
  uint32_t i;
  wxString escaped;

  for (i = 0; i < source.Length(); i++) {
#if defined(SYS_UNIX) || defined(SYS_APPLE)
    if (source[i] == wxC('"'))
      escaped += wxS("\\\"");
    else if (source[i] == wxC('\\'))
      escaped += wxS("\\\\");
#else
    if (source[i] == wxC('"'))
      ;
#endif
    else if ((source[i] == wxC('\n')) || (source[i] == wxC('\r')))
      escaped += wxS(" ");
    else
      escaped += source[i];
  }

  return escaped;
}

wxString
no_cr(wxString source) {
  uint32_t i;
  wxString escaped;

  for (i = 0; i < source.Length(); i++) {
    if (source[i] == wxC('\n'))
      escaped += wxS(" ");
    else
      escaped += source[i];
  }

  return escaped;
}

vector<wxString>
split(const wxString &src,
      const char *pattern,
      int max_num) {
  int num, i, plen;
  char *copy, *p1, *p2;
  vector<wxString> v;

  plen = strlen(pattern);
  copy = safestrdup(src);
  p2 = copy;
  p1 = strstr(p2, pattern);
  num = 1;
  while ((p1 != NULL) && ((max_num == -1) || (num < max_num))) {
    for (i = 0; i < plen; i++)
      p1[i] = 0;
    v.push_back(wxString(p2));
    p2 = &p1[plen];
    p1 = strstr(p2, pattern);
    num++;
  }
  if (*p2 != 0)
    v.push_back(wxString(p2));
  safefree(copy);

  return v;
}

wxString
join(const char *pattern,
     vector<wxString> &strings) {
  wxString dst;
  uint32_t i;

  if (strings.size() == 0)
    return "";
  dst = strings[0];
  for (i = 1; i < strings.size(); i++) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

wxString &
strip(wxString &s,
      bool newlines) {
  int i, len;
  const wxChar *c;

  c = s.c_str();
  i = 0;
  if (newlines)
    while ((c[i] != 0) && (isblanktab(c[i]) || iscr(c[i])))
      i++;
  else
    while ((c[i] != 0) && isblanktab(c[i]))
      i++;

  if (i > 0)
    s.Remove(0, i);

  c = s.c_str();
  len = s.length();
  i = 0;

  if (newlines)
    while ((i < len) && (isblanktab(c[len - i - 1]) || iscr(c[len - i - 1])))
      i++;
  else
    while ((i < len) && isblanktab(c[len - i - 1]))
      i++;

  if (i > 0)
    s.Remove(len - i, i);

  return s;
}

vector<wxString> &
strip(vector<wxString> &v,
      bool newlines) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);

  return v;
}

wxString
to_utf8(const wxString &src) {
  char *utf8;
  wxString retval;

  utf8 = to_utf8(cc_local_utf8, src.c_str());
  retval = utf8;
  safefree(utf8);

  return retval;
}

wxString
from_utf8(const wxString &src) {
  char *local;
  wxString retval;

  local = from_utf8(cc_local_utf8, src.c_str());
  retval = local;
  safefree(local);

  return retval;
}

wxString
unescape(const wxString &src) {
  wxString dst;
  int current_char, next_char;

  if (src.length() <= 1)
    return src;
  next_char = 1;
  current_char = 0;
  while (current_char < src.length()) {
    if (src[current_char] == wxC('\\')) {
      if (next_char == src.length()) // This is an error...
        dst += wxC('\\');
      else {
        if (src[next_char] == wxC('2'))
          dst += wxC('"');
        else if (src[next_char] == wxC('s'))
          dst += wxC(' ');
        else
          dst += src[next_char];
        current_char++;
      }
    } else
      dst += src[current_char];
    current_char++;
    next_char = current_char + 1;
  }

  return dst;
}

bool
is_popular_language(const char *lang) {
  return
    !strcmp(lang, "Chinese") ||
    !strcmp(lang, "Dutch") ||
    !strcmp(lang, "English") ||
    !strcmp(lang, "Finnish") ||
    !strcmp(lang, "French") ||
    !strcmp(lang, "German") ||
    !strcmp(lang, "Italian") ||
    !strcmp(lang, "Japanese") ||
    !strcmp(lang, "Norwegian") ||
    !strcmp(lang, "Portuguese") ||
    !strcmp(lang, "Russian") ||
    !strcmp(lang, "Spanish") ||
    !strcmp(lang, "Swedish");
}

bool
is_popular_language_code(const char *code) {
  return
    !strcmp(code, "zho") ||     // Chinese
    !strcmp(code, "nld") ||     // Dutch
    !strcmp(code, "eng") ||     // English
    !strcmp(code, "fin") ||     // Finnish
    !strcmp(code, "fre") ||     // French
    !strcmp(code, "ger") ||     // German
    !strcmp(code, "ita") ||     // Italian
    !strcmp(code, "jpn") ||     // Japanese
    !strcmp(code, "nor") ||     // Norwegian
    !strcmp(code, "por") ||     // Portuguese
    !strcmp(code, "rus") ||     // Russian
    !strcmp(code, "spa") ||     // Spanish
    !strcmp(code, "swe");       // Swedish
}

mmg_dialog::mmg_dialog(): wxFrame(NULL, -1, "mkvmerge GUI v" VERSION,
                                  wxPoint(0, 0),
#ifdef SYS_WINDOWS
                                  wxSize(520, 740),
#else
                                  wxSize(520, 718),
#endif
                                  wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  mdlg = this;

  file_menu = new wxMenu();
  file_menu->Append(ID_M_FILE_NEW, wxS("&New\tCtrl-N"),
                    wxS("Start with empty settings"));
  file_menu->Append(ID_M_FILE_LOAD, wxS("&Load settings\tCtrl-L"),
                    wxS("Load muxing settings from a file"));
  file_menu->Append(ID_M_FILE_SAVE, wxS("&Save settings\tCtrl-S"),
                    wxS("Save muxing settings to a file"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_SETOUTPUT, wxS("Set &output file"),
                    wxS("Select the file you want to write to"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_EXIT, wxS("&Quit\tCtrl-Q"),
                    wxS("Quit the application"));

  file_menu_sep = false;
  update_file_menu();

  wxMenu *muxing_menu = new wxMenu();
  muxing_menu->Append(ID_M_MUXING_START,
                      wxS("Sta&rt muxing (run mkvmerge)\tCtrl-R"),
                      wxS("Run mkvmerge and start the muxing process"));
  muxing_menu->Append(ID_M_MUXING_COPY_CMDLINE,
                      wxS("&Copy command line to clipboard"),
                      wxS("Copy the command line to the clipboard"));
  muxing_menu->Append(ID_M_MUXING_SAVE_CMDLINE,
                      wxS("Sa&ve command line"),
                      wxS("Save the command line to a file"));
  muxing_menu->Append(ID_M_MUXING_CREATE_OPTIONFILE,
                      wxS("Create &option file"),
                      wxS("Save the command line to an option file "
                          "that can be read by mkvmerge"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_ADD_TO_JOBQUEUE,
                      wxS("&Add to job queue"),
                      wxS("Adds the current settings as a new job entry to "
                          "the job queue"));
  muxing_menu->Append(ID_M_MUXING_MANAGE_JOBS,
                      wxS("&Manage jobs"),
                      wxS("Brings up the job queue editor"));

  chapter_menu = new wxMenu();
  chapter_menu->Append(ID_M_CHAPTERS_NEW, wxS("&New chapters"),
                       wxS("Create a new chapter file"));
  chapter_menu->Append(ID_M_CHAPTERS_LOAD, wxS("&Load"),
                       wxS("Load a chapter file (simple/OGM format or XML "
                           "format)"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVE, wxS("&Save"),
                       wxS("Save the current chapters to a XML file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVETOKAX, wxS("Save to &Matroska file"),
                       wxS("Save the current chapters to an existing Matroska "
                           "file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVEAS, wxS("Save &as"),
                       wxS("Save the current chapters to a file with another "
                           "name"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_VERIFY, wxS("&Verify"),
                       wxS("Verify the current chapter entries to see if "
                           "there are any errors"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_SETDEFAULTS, wxS("Set &default values"));
  chapter_menu_sep = false;
  update_chapter_menu();

  wxMenu *window_menu = new wxMenu();
  window_menu->Append(ID_M_WINDOW_INPUT, wxS("&Input\tAlt-1"));
  window_menu->Append(ID_M_WINDOW_ATTACHMENTS, wxS("&Attachments\tAlt-2"));
  window_menu->Append(ID_M_WINDOW_GLOBAL, wxS("&Global options\tAlt-3"));
  window_menu->Append(ID_M_WINDOW_ADVANCED, wxS("A&dvanced\tAlt-4"));
  window_menu->Append(ID_M_WINDOW_SETTINGS, wxS("&Settings\tAlt-5"));
  window_menu->AppendSeparator();
  window_menu->Append(ID_M_WINDOW_CHAPTEREDITOR,
                      wxS("&Chapter editor\tAlt-6"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HELP_ABOUT, wxS("&About\tF1"),
                    wxS("Show program information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(file_menu, wxS("&File"));
  menu_bar->Append(muxing_menu, wxS("&Muxing"));
  menu_bar->Append(chapter_menu, wxS("&Chapter Editor"));
  menu_bar->Append(window_menu, wxS("&Window"));
  menu_bar->Append(help_menu, wxS("&Help"));
  SetMenuBar(menu_bar);

  status_bar = new wxStatusBar(this, -1);
  SetStatusBar(status_bar);
  status_bar_timer.SetOwner(this, ID_T_STATUSBAR);

  wxPanel *panel = new wxPanel(this, -1);

  wxBoxSizer *bs_main = new wxBoxSizer(wxVERTICAL);
  panel->SetSizer(bs_main);
  panel->SetAutoLayout(true);

  notebook =
    new wxNotebook(panel, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  settings_page = new tab_settings(notebook);
  input_page = new tab_input(notebook);
  attachments_page = new tab_attachments(notebook);
  global_page = new tab_global(notebook);
  advanced_page = new tab_advanced(notebook);
  chapter_editor_page = new tab_chapters(notebook, chapter_menu);

  notebook->AddPage(input_page, wxS("Input"));
  notebook->AddPage(attachments_page, wxS("Attachments"));
  notebook->AddPage(global_page, wxS("Global"));
  notebook->AddPage(advanced_page, wxS("Advanced"));
  notebook->AddPage(settings_page, wxS("Settings"));
  notebook->AddPage(chapter_editor_page, wxS("Chapter Editor"));

  bs_main->Add(notebook, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  wxStaticBox *sb_low = new wxStaticBox(panel, -1, wxS("Output filename"));
  wxStaticBoxSizer *sbs_low = new wxStaticBoxSizer(sb_low, wxHORIZONTAL);
  bs_main->Add(sbs_low, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  tc_output =
    new wxTextCtrl(panel, ID_TC_OUTPUT, wxS(""), wxDefaultPosition,
                   wxSize(400, -1), 0);
  sbs_low->Add(tc_output, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  b_browse_output =
    new wxButton(panel, ID_B_BROWSEOUTPUT, wxS("Browse"), wxDefaultPosition,
                 wxDefaultSize, 0);
  sbs_low->Add(b_browse_output, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

  sb_commandline =
    new wxStaticBox(panel, -1, wxS("Command line"));
  wxStaticBoxSizer *sbs_low2 =
    new wxStaticBoxSizer(sb_commandline, wxHORIZONTAL);
  bs_main->Add(sbs_low2, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  tc_cmdline =
    new wxTextCtrl(panel, ID_TC_CMDLINE, wxS(""), wxDefaultPosition,
                   wxSize(490, 50), wxTE_READONLY | wxTE_LINEWRAP |
                   wxTE_MULTILINE);
  sbs_low2->Add(tc_cmdline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

  wxBoxSizer *bs_buttons = new wxBoxSizer(wxHORIZONTAL);

  b_start_muxing =
    new wxButton(panel, ID_B_STARTMUXING, wxS("Sta&rt muxing"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(b_start_muxing, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  b_copy_to_clipboard =
    new wxButton(panel, ID_B_COPYTOCLIPBOARD, wxS("&Copy to clipboard"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(10, 0);
  bs_buttons->Add(b_copy_to_clipboard, 0, wxALIGN_CENTER_HORIZONTAL | wxALL,
                  8);

  b_add_to_jobqueue =
    new wxButton(panel, ID_B_ADD_TO_JOBQUEUE, wxS("&Add to job queue"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(b_add_to_jobqueue, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  bs_main->Add(bs_buttons, 0, wxALIGN_CENTER_HORIZONTAL);

  last_open_dir = "";
  cmdline = "\"" + mkvmerge_path + "\" -o \"" + tc_output->GetValue() + "\" ";
  tc_cmdline->SetValue(cmdline);
  cmdline_timer.SetOwner(this, ID_T_UPDATECMDLINE);
  cmdline_timer.Start(1000);

  load_job_queue();

  SetIcon(wxICON(matroskalogo_big));

  set_status_bar("mkvmerge GUI ready");

  if (app->argc > 1) {
    wxString file;

    file = app->argv[1];
    if (!wxFileExists(file) || wxDirExists(file))
      wxMessageBox("The file '" + file + "' does exist.",
                   "Error loading settings", wxOK | wxCENTER | wxICON_ERROR);
    else {
#ifdef SYS_WINDOWS
      if ((file.Length() > 3) && (file.c_str()[1] != wxC(':')) &&
          (file.c_str()[0] != wxC('\\')))
        file = wxGetCwd() + wxS("\\") + file;
#else
      if ((file.Length() > 0) && (file.c_str()[0] != wxC('/')))
        file = wxGetCwd() + wxS("/") + file;
#endif
      load(file);
    }
  }
}

void
mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir,
                   tc_output->GetValue().AfterLast(PSEP),
                   wxS("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                       ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_output->SetValue(dlg.GetPath());
    verified_output_file = dlg.GetPath();
  }
}

void
mmg_dialog::set_status_bar(wxString text) {
  status_bar_timer.Stop();
  status_bar->SetStatusText(text);
  status_bar_timer.Start(4000, true);
}

void
mmg_dialog::on_clear_status_bar(wxTimerEvent &evt) {
  status_bar->SetStatusText("");
}

void
mmg_dialog::on_quit(wxCommandEvent &evt) {
  Close(true);
}

void
mmg_dialog::on_file_new(wxCommandEvent &evt) {
  wxFileConfig *cfg;
  wxString tmp_name;

  tmp_name.Printf("tempsettings-%u.mmg", getpid());
  cfg = new wxFileConfig("mkvmerge GUI", "Moritz Bunkus", tmp_name);
  tc_output->SetValue("");

  input_page->load(cfg);
  attachments_page->load(cfg);
  global_page->load(cfg);
  settings_page->load(cfg);

  delete cfg;
  unlink(tmp_name);
  verified_output_file = wxS("");

  set_status_bar("Configuration cleared.");
}

void
mmg_dialog::on_file_load(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an input file", last_open_dir, "",
                   wxS("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
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

void
mmg_dialog::load(wxString file_name) {
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
  set_last_settings_in_menu(file_name);
  cfg->Read("output_file_name", &s);
  tc_output->SetValue(s);
  verified_output_file = wxS("");

  input_page->load(cfg);
  attachments_page->load(cfg);
  global_page->load(cfg);
  settings_page->load(cfg);

  delete cfg;

  set_status_bar("Configuration loaded.");
}

void
mmg_dialog::on_file_save(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   wxS("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    save(dlg.GetPath());
  }
}

void
mmg_dialog::save(wxString file_name,
                 bool used_for_jobs) {
  wxFileConfig *cfg;

  if (!used_for_jobs)
    set_last_settings_in_menu(file_name);
  cfg = new wxFileConfig("mkvmerge GUI", "Moritz Bunkus", file_name);
  cfg->SetPath("/mkvmergeGUI");
  cfg->Write("file_version", 1);
  cfg->Write("gui_version", VERSION);
  cfg->Write("output_file_name", tc_output->GetValue());

  input_page->save(cfg);
  attachments_page->save(cfg);
  global_page->save(cfg);
  settings_page->save(cfg);

  delete cfg;

  if (!used_for_jobs)
    set_status_bar("Configuration saved.");
}

void
mmg_dialog::set_last_settings_in_menu(wxString name) {
  uint32_t i;
  vector<wxString>::iterator eit;
  wxConfigBase *cfg;
  wxString s;

  i = 0;
  while (i < last_settings.size()) {
    if (last_settings[i] == name) {
      eit = last_settings.begin();
      eit += i;
      last_settings.erase(eit);
    } else
      i++;
  }
  last_settings.insert(last_settings.begin(), name);
  while (last_settings.size() > 4)
    last_settings.pop_back();

  cfg = wxConfigBase::Get();
  cfg->SetPath("/GUI");
  for (i = 0; i < last_settings.size(); i++) {
    s.Printf("last_settings %d", i);
    cfg->Write(s, last_settings[i]);
  }
  cfg->Flush();

  update_file_menu();
}

void
mmg_dialog::set_last_chapters_in_menu(wxString name) {
  uint32_t i;
  vector<wxString>::iterator eit;
  wxConfigBase *cfg;
  wxString s;

  i = 0;
  while (i < last_chapters.size()) {
    if (last_chapters[i] == name) {
      eit = last_chapters.begin();
      eit += i;
      last_chapters.erase(eit);
    } else
      i++;
  }
  last_chapters.insert(last_chapters.begin(), name);
  while (last_chapters.size() > 4)
    last_chapters.pop_back();

  cfg = wxConfigBase::Get();
  cfg->SetPath("/GUI");
  for (i = 0; i < last_chapters.size(); i++) {
    s.Printf("last_chapters %d", i);
    cfg->Write(s, last_chapters[i]);
  }
  cfg->Flush();

  update_chapter_menu();
}

void
mmg_dialog::on_run(wxCommandEvent &evt) {
  mux_dialog *mux_dlg;

  update_command_line();

  if (tc_output->GetValue().Length() == 0) {
    wxMessageBox(wxS("You have not yet selected an output file."),
                 wxS("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings() ||
      !settings_page->validate_settings())
    return;

  if ((verified_output_file != tc_output->GetValue()) &&
      wxFile::Exists(tc_output->GetValue()) &&
      (wxMessageBox(wxS("The output file '") + tc_output->GetValue() +
                    wxS("' exists already. Do you want to overwrite it?"),
                    wxS("Overwrite existing file?"), wxYES_NO) != wxYES))
    return;
  verified_output_file = tc_output->GetValue();

  mux_dlg = new mux_dialog(this);
  delete mux_dlg;
}

void
mmg_dialog::on_about(wxCommandEvent &evt) {
  wxMessageBox(wxS("mkvmerge GUI v" VERSION "\n"
                   "This GUI was written by Moritz Bunkus <moritz@bunkus.org>"
                   "\nBased on mmg by Florian Wagner <flo.wagner@gmx.de>\n"
                   "mkvmerge GUI is licensed under the GPL.\n"
                   "http://www.bunkus.org/videotools/mkvtoolnix/\n"
                   "\n"
                   "Help is available in form of tool tips and the HTML "
                   "documentation\n"
                   "for mkvmerge, mkvmerge.html. It can also be viewed online "
                   "at\n"
                   "http://www.bunkus.org/videotools/mkvtoolnix/doc/"
                   "mkvmerge.html"),
               wxS("About mkvmerge's GUI"),
               wxOK | wxCENTER | wxICON_INFORMATION);
}

void
mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
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

void
mmg_dialog::on_create_optionfile(wxCommandEvent &evt) {
  uint32_t i;
  char *arg_utf8;
  mm_io_c *file;

  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T(ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    try {
      file = new mm_io_c(dlg.GetPath().c_str(), MODE_CREATE);
      file->write_bom("UTF-8");
    } catch (...) {
      wxMessageBox("Could not create the specified file.",
                   "File creation failed", wxOK | wxCENTER |
                   wxICON_ERROR);
      return;
    }
    for (i = 1; i < clargs.Count(); i++) {
      if (clargs[i].length() == 0)
        file->puts_unl("#EMPTY#");
      else {
        arg_utf8 = to_utf8(cc_local_utf8, clargs[i].c_str());
        file->puts_unl(arg_utf8);
        safefree(arg_utf8);
      }
      file->puts_unl("\n");
    }
    delete file;

    set_status_bar("Option file created.");
  }
}

void
mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
  update_command_line();
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(cmdline));
    wxTheClipboard->Close();
    set_status_bar("Command line copied to clipboard.");
  } else
    set_status_bar("Could not open the clipboard.");
}

wxString &
mmg_dialog::get_command_line() {
  return cmdline;
}

wxArrayString &
mmg_dialog::get_command_line_args() {
  return clargs;
}

void
mmg_dialog::on_update_command_line(wxTimerEvent &evt) {
  update_command_line();
}

void
mmg_dialog::update_command_line() {
  uint32_t fidx, tidx, i, args_start;
  bool tracks_selected_here;
  bool no_audio, no_video, no_subs;
  mmg_file_t *f;
  mmg_track_t *t;
  mmg_attachment_t *a;
  wxString sid, old_cmdline, arg, aids, sids, dids, track_order;

  old_cmdline = cmdline;
  cmdline = "\"" + mkvmerge_path + "\" -o \"" + tc_output->GetValue() + "\" " +
    "--command-line-charset UTF-8 ";

  clargs.Clear();
  clargs.Add(mkvmerge_path);
  clargs.Add("-o");
  clargs.Add(tc_output->GetValue());
  args_start = clargs.Count();

  if (settings_page->cob_priority->GetValue() != wxS("normal")) {
    clargs.Add("--priority");
    clargs.Add(settings_page->cob_priority->GetValue());
  }

  for (fidx = 0; fidx < files.size(); fidx++) {
    f = &files[fidx];
    tracks_selected_here = false;
    no_audio = true;
    no_video = true;
    no_subs = true;
    aids = "";
    sids = "";
    dids = "";
    track_order = "";
    for (tidx = 0; tidx < f->tracks->size(); tidx++) {
      string format;

      t = &(*f->tracks)[tidx];
      if (!t->enabled)
        continue;

      tracks_selected_here = true;
      fix_format("%lld", format);
      sid.Printf(format.c_str(), t->id);
      if (track_order.Length() > 0)
        track_order += ",";
      track_order += sid;

      if (t->type == 'a') {
        no_audio = false;
        if (aids.length() > 0)
          aids += ",";
        aids += sid;

        if (t->aac_is_sbr) {
          clargs.Add("--aac-is-sbr");
          clargs.Add(sid);
        }

      } else if (t->type == 'v') {
        no_video = false;
        if (dids.length() > 0)
          dids += ",";
        dids += sid;

      } else if (t->type == 's') {
        no_subs = false;
        if (sids.length() > 0)
          sids += ",";
        sids += sid;

        if ((t->sub_charset->Length() > 0) && (*t->sub_charset != "default")) {
          clargs.Add("--sub-charset");
          clargs.Add(sid + ":" + shell_escape(*t->sub_charset));
        }
      }

      if (*t->language != "none") {
        clargs.Add("--language");
        clargs.Add(sid + ":" + extract_language_code(*t->language));
      }

      if (*t->cues != "default") {
        clargs.Add("--cues");
        if (*t->cues == "only for I frames")
          clargs.Add(sid + ":iframes");
        else if (*t->cues == "for all frames")
          clargs.Add(sid + ":all");
        else if (*t->cues == "none")
          clargs.Add(sid + ":none");
        else
          mxerror("Unknown cues option '%s'. Should not have happened.\n",
                  t->cues->c_str());
      }

      if ((t->delay->Length() > 0) || (t->stretch->Length() > 0)) {
        arg = sid + ":";
        if (t->delay->Length() > 0)
          arg += *t->delay;
        else
          arg += "0";
        if (t->stretch->Length() > 0)
          arg += *t->stretch;
        clargs.Add("--sync");
        clargs.Add(arg);
      }

      if ((t->track_name->Length() > 0) || t->track_name_was_present) {
        clargs.Add("--track-name");
        clargs.Add(sid + ":" + *t->track_name);
      }

      if (t->default_track) {
        clargs.Add("--default-track");
        clargs.Add(sid);
      }

      if (t->tags->Length() > 0) {
        clargs.Add("--tags");
        clargs.Add(sid + ":" + *t->tags);
      }

      if (!t->display_dimensions_selected && (t->aspect_ratio->Length() > 0)) {
        clargs.Add("--aspect-ratio");
        clargs.Add(sid + ":" + *t->aspect_ratio);
      } else if (t->display_dimensions_selected &&
                 (t->dwidth->Length() > 0) && (t->dheight->Length() > 0)) {
        clargs.Add("--display-dimensions");
        clargs.Add(sid + ":" + *t->dwidth + "x" + *t->dheight);
      }

      if (t->fourcc->Length() > 0) {
        clargs.Add("--fourcc");
        clargs.Add(sid + ":" + *t->fourcc);
      }

      if (t->compression->Length() > 0) {
        clargs.Add("--compression");
        clargs.Add(sid + ":" + *t->compression);
      }

      if (t->timecodes->Length() > 0) {
        clargs.Add("--timecodes");
        clargs.Add(sid + ":" + *t->timecodes);
      }
    }

    if (aids.length() > 0) {
      clargs.Add("-a");
      clargs.Add(aids);
    }
    if (dids.length() > 0) {
      clargs.Add("-d");
      clargs.Add(dids);
    }
    if (sids.length() > 0) {
      clargs.Add("-s");
      clargs.Add(sids);
    }

    if (tracks_selected_here) {
      if (f->no_chapters)
        clargs.Add("--no-chapters");
      if (f->no_attachments)
        clargs.Add("--no-attachments");
      if (f->no_tags)
        clargs.Add("--no-tags");

      if (no_video)
        clargs.Add("-D");
      if (no_audio)
        clargs.Add("-A");
      if (no_subs)
        clargs.Add("-S");

      clargs.Add("--track-order");
      clargs.Add(track_order);

      clargs.Add(*f->file_name);
    }
  }

  for (fidx = 0; fidx < attachments.size(); fidx++) {
    a = &attachments[fidx];

    clargs.Add("--attachment-mime-type");
    clargs.Add(*a->mime_type);
    if (a->description->Length() > 0) {
      clargs.Add("--attachment-description");
      clargs.Add(no_cr(*a->description));
    }
    if (a->style == 0)
      clargs.Add("--attach-file");
    else
      clargs.Add("--attach-file-once");
    clargs.Add(*a->file_name);
  }

  if (global_page->tc_title->GetValue().Length() > 0) {
    clargs.Add("--title");
    clargs.Add(global_page->tc_title->GetValue());
  } else if (title_was_present) {
    clargs.Add("--title");
    clargs.Add("");
  }

  if (global_page->cb_split->IsChecked()) {
    clargs.Add("--split");
    if (global_page->rb_split_by_size->GetValue())
      clargs.Add(global_page->cob_split_by_size->GetValue());
    else
      clargs.Add(global_page->cob_split_by_time->GetValue());

    if (global_page->tc_split_max_files->GetValue().Length() > 0) {
      clargs.Add("--split-max-files");
      clargs.Add(global_page->tc_split_max_files->GetValue());
    }

    if (global_page->cb_link->IsChecked())
      clargs.Add("--link");
  }

  if (global_page->tc_previous_segment_uid->GetValue().Length() > 0) {
    clargs.Add("--link-to-previous");
    clargs.Add(global_page->tc_previous_segment_uid->GetValue());
  }

  if (global_page->tc_next_segment_uid->GetValue().Length() > 0) {
    clargs.Add("--link-to-next");
    clargs.Add(global_page->tc_next_segment_uid->GetValue());
  }

  if (global_page->tc_chapters->GetValue().Length() > 0) {
    if (global_page->cob_chap_language->GetValue().Length() > 0) {
      clargs.Add("--chapter-language");
      clargs.Add(extract_language_code(global_page->
                                       cob_chap_language->GetValue()));
    }

    if (global_page->cob_chap_charset->GetValue().Length() > 0) {
      clargs.Add("--chapter-charset");
      clargs.Add(global_page->cob_chap_charset->GetValue());
    }

    if (global_page->tc_cue_name_format->GetValue().Length() > 0) {
      clargs.Add("--cue-chapter-name-format");
      clargs.Add(global_page->tc_cue_name_format->GetValue());
    }

    clargs.Add("--chapters");
    clargs.Add(global_page->tc_chapters->GetValue());
  }

  if (global_page->tc_global_tags->GetValue().Length() > 0) {
    clargs.Add("--global-tags");
    clargs.Add(global_page->tc_global_tags->GetValue());
  }

  if (advanced_page->cob_cl_charset->GetValue().Length() > 0) {
    clargs.Add("--command-line-charset");
    clargs.Add(advanced_page->cob_cl_charset->GetValue());
  }

  if (advanced_page->cb_no_cues->IsChecked())
    clargs.Add("--no-cues");

  if (advanced_page->cb_no_clusters->IsChecked())
    clargs.Add("--no-clusters-in-meta-seek");

  if (advanced_page->cb_disable_lacing->IsChecked())
    clargs.Add("--disable-lacing");

  if (advanced_page->cb_enable_durations->IsChecked())
    clargs.Add("--enable-durations");

  if (advanced_page->cb_enable_timeslices->IsChecked())
    clargs.Add("--enable-timeslices");

  for (i = args_start; i < clargs.Count(); i++) {
    if (clargs[i].Find(" ") >= 0)
      cmdline += " \"" + shell_escape(clargs[i]) + "\"";
    else
      cmdline += " " + shell_escape(clargs[i]);
  }

  cmdline = to_utf8(cmdline);
  if (old_cmdline != cmdline)
    tc_cmdline->SetValue(cmdline);
}

void
mmg_dialog::on_file_load_last(wxCommandEvent &evt) {
  if ((evt.GetId() < ID_M_FILE_LOADLAST1) ||
      ((evt.GetId() - ID_M_FILE_LOADLAST1) >= last_settings.size()))
    return;

  load(last_settings[evt.GetId() - ID_M_FILE_LOADLAST1]);
}

void
mmg_dialog::on_chapters_load_last(wxCommandEvent &evt) {
  if ((evt.GetId() < ID_M_CHAPTERS_LOADLAST1) ||
      ((evt.GetId() - ID_M_CHAPTERS_LOADLAST1) >= last_chapters.size()))
    return;

  notebook->SetSelection(5);
  chapter_editor_page->load(last_chapters[evt.GetId() -
                                          ID_M_CHAPTERS_LOADLAST1]);
}

void
mmg_dialog::update_file_menu() {
  uint32_t i;
  wxMenuItem *mi;
  wxString s;

  for (i = ID_M_FILE_LOADLAST1; i <= ID_M_FILE_LOADLAST4; i++) {
    mi = file_menu->Remove(i);
    if (mi != NULL)
      delete mi;
  }

  if ((last_settings.size() > 0) && !file_menu_sep) {
    file_menu->AppendSeparator();
    file_menu_sep = true;
  }
  for (i = 0; i < last_settings.size(); i++) {
    s.Printf("&%u. %s", i + 1, last_settings[i].c_str());
    file_menu->Append(ID_M_FILE_LOADLAST1 + i, s);
  }
}

void
mmg_dialog::update_chapter_menu() {
  uint32_t i;
  wxMenuItem *mi;
  wxString s;

  for (i = ID_M_CHAPTERS_LOADLAST1; i <= ID_M_CHAPTERS_LOADLAST4; i++) {
    mi = chapter_menu->Remove(i);
    if (mi != NULL)
      delete mi;
  }

  if ((last_chapters.size() > 0) && !chapter_menu_sep) {
    chapter_menu->AppendSeparator();
    chapter_menu_sep = true;
  }
  for (i = 0; i < last_chapters.size(); i++) {
    s.Printf("&%u. %s", i + 1, last_chapters[i].c_str());
    chapter_menu->Append(ID_M_CHAPTERS_LOADLAST1 + i, s);
  }
}

void
mmg_dialog::on_new_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_new_chapters(evt);
}

void
mmg_dialog::on_load_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_load_chapters(evt);
}

void
mmg_dialog::on_save_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_save_chapters(evt);
}

void
mmg_dialog::on_save_chapters_to_kax_file(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_save_chapters_to_kax_file(evt);
}

void
mmg_dialog::on_save_chapters_as(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_save_chapters_as(evt);
}

void
mmg_dialog::on_verify_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_verify_chapters(evt);
}

void
mmg_dialog::on_set_default_chapter_values(wxCommandEvent &evt) {
  notebook->SetSelection(5);
  chapter_editor_page->on_set_default_values(evt);
}

void
mmg_dialog::on_window_selected(wxCommandEvent &evt) {
  notebook->SetSelection(evt.GetId() - ID_M_WINDOW_INPUT);
}

void
mmg_dialog::set_title_maybe(const wxString &new_title) {
  if ((new_title.length() > 0) &&
      (global_page->tc_title->GetValue().length() == 0))
    global_page->tc_title->SetValue(new_title);
}

void
mmg_dialog::set_output_maybe(const wxString &new_output) {
  wxString output;

  if (settings_page->cb_autoset_output_filename->IsChecked() &&
      (new_output.length() > 0) &&
      (tc_output->GetValue().length() == 0)) {
    output = new_output.BeforeLast('.');
    output += ".mkv";
    tc_output->SetValue(output);
  }
}

void
mmg_dialog::on_add_to_jobqueue(wxCommandEvent &evt) {
  wxString description;
  job_t job;

  if (tc_output->GetValue().Length() == 0) {
    wxMessageBox(wxS("You have not yet selected an output file."),
                 wxS("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings() ||
      !settings_page->validate_settings())
    return;

  if ((verified_output_file != tc_output->GetValue()) &&
      wxFile::Exists(tc_output->GetValue()) &&
      (wxMessageBox(wxS("The output file '") + tc_output->GetValue() +
                    wxS("' exists already. Do you want to overwrite it?"),
                    wxS("Overwrite existing file?"), wxYES_NO) != wxYES))
    return;
  verified_output_file = tc_output->GetValue();

  description = wxGetTextFromUser(wxS("Please enter a description for the "
                                      "new job:"), wxS("Job description"),
                                  wxS(""));
  if (description.length() == 0)
    return;

  if (!wxDirExists(wxS("jobs")))
    wxMkdir(wxS("jobs"));

  last_job_id++;
  if (last_job_id > 2000000000)
    last_job_id = 0;
  job.id = last_job_id;
  job.status = jobs_pending;
  job.added_on = wxGetLocalTime();
  job.started_on = -1;
  job.finished_on = -1;
  job.description = new wxString(description);
  jobs.push_back(job);

  description.Printf(wxS("/jobs/%d.mmg"), job.id);
  save(wxGetCwd() + description);

  save_job_queue();

  set_status_bar(wxS("Job added to job queue"));
}

void
mmg_dialog::on_manage_jobs(wxCommandEvent &evt) {
  job_dialog jdlg(this);
}

void
mmg_dialog::load_job_queue() {
  int num, i, value;
  wxString s;
  wxConfigBase *cfg;
  job_t job;

  last_job_id = 0;

  cfg = wxConfigBase::Get();
  cfg->SetPath(wxS("/jobs"));
  cfg->Read(wxS("last_job_id"), &last_job_id);
  if (!cfg->Read(wxS("number_of_jobs"), &num))
    return;

  for (i = 0; i < jobs.size(); i++)
    delete jobs[i].description;
  jobs.clear();

  for (i = 0; i < num; i++) {
    s.Printf(wxS("/jobs/%u"), i);
    if (!cfg->HasGroup(s))
      continue;
    cfg->SetPath(s);
    cfg->Read(wxS("id"), &value);
    job.id = value;
    cfg->Read(wxS("status"), &s);
    job.status =
      s == wxS("pending") ? jobs_pending :
      s == wxS("done") ? jobs_done :
      s == wxS("aborted") ? jobs_aborted :
      jobs_failed;
    cfg->Read(wxS("added_on"), &value);
    job.added_on = value;
    cfg->Read(wxS("started_on"), &value);
    job.started_on = value;
    cfg->Read(wxS("finished_on"), &value);
    job.finished_on = value;
    cfg->Read(wxS("description"), &s);
    job.description = new wxString(s);
    jobs.push_back(job);
  }
}

void
mmg_dialog::save_job_queue() {
  wxString s;
  wxConfigBase *cfg;
  uint32_t i;
  vector<wxString> job_groups;
  long cookie;

  cfg = wxConfigBase::Get();
  cfg->SetPath(wxS("/jobs"));
  if (cfg->GetFirstGroup(s, cookie)) {
    do {
      job_groups.push_back(s);
    } while (cfg->GetNextGroup(s, cookie));
    for (i = 0; i < job_groups.size(); i++)
      cfg->DeleteGroup(job_groups[i]);
  }
  cfg->Write(wxS("last_job_id"), last_job_id);
  cfg->Write(wxS("number_of_jobs"), (int)jobs.size());
  for (i = 0; i < jobs.size(); i++) {
    s.Printf(wxS("/jobs/%u"), i);
    cfg->SetPath(s);
    cfg->Write(wxS("id"), jobs[i].id);
    cfg->Write(wxS("status"),
               jobs[i].status == jobs_pending ? wxS("pending") :
               jobs[i].status == jobs_done ? wxS("done") :
               jobs[i].status == jobs_aborted ? wxS("aborted") :
               wxS("failed"));
    cfg->Write(wxS("added_on"), jobs[i].added_on);
    cfg->Write(wxS("started_on"), jobs[i].started_on);
    cfg->Write(wxS("finished_on"), jobs[i].finished_on);
    cfg->Write(wxS("description"), *jobs[i].description);
  }
  cfg->Flush();
}

IMPLEMENT_CLASS(mmg_dialog, wxFrame);
BEGIN_EVENT_TABLE(mmg_dialog, wxFrame)
  EVT_BUTTON(ID_B_BROWSEOUTPUT, mmg_dialog::on_browse_output)
  EVT_BUTTON(ID_B_STARTMUXING, mmg_dialog::on_run)
  EVT_BUTTON(ID_B_COPYTOCLIPBOARD, mmg_dialog::on_copy_to_clipboard)
  EVT_BUTTON(ID_B_ADD_TO_JOBQUEUE, mmg_dialog::on_add_to_jobqueue)
  EVT_TIMER(ID_T_UPDATECMDLINE, mmg_dialog::on_update_command_line)
  EVT_TIMER(ID_T_STATUSBAR, mmg_dialog::on_clear_status_bar)
  EVT_MENU(ID_M_FILE_EXIT, mmg_dialog::on_quit)
  EVT_MENU(ID_M_FILE_NEW, mmg_dialog::on_file_new)
  EVT_MENU(ID_M_FILE_LOAD, mmg_dialog::on_file_load)
  EVT_MENU(ID_M_FILE_SAVE, mmg_dialog::on_file_save)
  EVT_MENU(ID_M_FILE_SETOUTPUT, mmg_dialog::on_browse_output)
  EVT_MENU(ID_M_MUXING_START, mmg_dialog::on_run)
  EVT_MENU(ID_M_MUXING_COPY_CMDLINE, mmg_dialog::on_copy_to_clipboard)
  EVT_MENU(ID_M_MUXING_SAVE_CMDLINE, mmg_dialog::on_save_cmdline)
  EVT_MENU(ID_M_MUXING_CREATE_OPTIONFILE, mmg_dialog::on_create_optionfile)
  EVT_MENU(ID_M_MUXING_ADD_TO_JOBQUEUE, mmg_dialog::on_add_to_jobqueue)
  EVT_MENU(ID_M_MUXING_MANAGE_JOBS, mmg_dialog::on_manage_jobs)
  EVT_MENU(ID_M_HELP_ABOUT, mmg_dialog::on_about)
  EVT_MENU(ID_M_FILE_LOADLAST1, mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST2, mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST3, mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST4, mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_CHAPTERS_NEW, mmg_dialog::on_new_chapters)
  EVT_MENU(ID_M_CHAPTERS_LOAD, mmg_dialog::on_load_chapters)
  EVT_MENU(ID_M_CHAPTERS_SAVE, mmg_dialog::on_save_chapters)
  EVT_MENU(ID_M_CHAPTERS_SAVEAS, mmg_dialog::on_save_chapters_as)
  EVT_MENU(ID_M_CHAPTERS_SAVETOKAX, mmg_dialog::on_save_chapters_to_kax_file)
  EVT_MENU(ID_M_CHAPTERS_VERIFY, mmg_dialog::on_verify_chapters)
  EVT_MENU(ID_M_CHAPTERS_SETDEFAULTS,
           mmg_dialog::on_set_default_chapter_values)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST1, mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST2, mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST3, mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST4, mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_WINDOW_INPUT, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_ATTACHMENTS, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_GLOBAL, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_ADVANCED, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_SETTINGS, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_CHAPTEREDITOR, mmg_dialog::on_window_selected)
END_EVENT_TABLE();

bool
mmg_app::OnInit() {
  wxConfigBase *cfg;
  uint32_t i;
  wxString k, v;

  cc_local_utf8 = utf8_init(NULL);

  cfg = new wxConfig("mkvmergeGUI");
  wxConfigBase::Set(cfg); 
  cfg->SetPath("/GUI");
  if (!cfg->Read("last_directory", &last_open_dir))
    last_open_dir = "";
  for (i = 0; i < 4; i++) {
    k.Printf("last_settings %u", i);
    if (cfg->Read(k, &v))
      last_settings.push_back(v);
    k.Printf("last_chapters %u", i);
    if (cfg->Read(k, &v))
      last_chapters.push_back(v);
  }
  cfg->SetPath("/chapter_editor");
  if (cfg->Read("default_language", &k))
    default_chapter_language = k.c_str();
  if (cfg->Read("default_country", &k))
    default_chapter_country = k.c_str();

  app = this;
  mdlg = new mmg_dialog();
  mdlg->Show(true);

  return true;
}

int
mmg_app::OnExit() {
  wxString s;
  wxConfigBase *cfg;

  cfg = wxConfigBase::Get();
  cfg->SetPath("/GUI");
  cfg->Write("last_directory", last_open_dir);
  cfg->SetPath("/chapter_editor");
  cfg->Write("default_language", default_chapter_language.c_str());
  cfg->Write("default_country", default_chapter_country.c_str());
  cfg->Flush();

  delete cfg;

  utf8_done();

  return 0;
}

IMPLEMENT_APP(mmg_app)
