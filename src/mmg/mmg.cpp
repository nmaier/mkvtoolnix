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
#include "wx/config.h"
#include "wx/datetime.h"
#include "wx/file.h"
#include "wx/fileconf.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statusbr.h"
#include "wx/statline.h"
#include "wx/strconv.h"

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "jobs.h"
#if defined(SYS_UNIX) || defined(SYS_APPLE)
#include "matroskalogo_big.xpm"
#endif
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"
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
vector<mmg_track_t *> tracks;
map<wxString, wxString, lt_wxString> capabilities;
vector<job_t> jobs;

#define ID_CLIOPTIONS_COB 2000
#define ID_CLIOPTIONS_ADD 2001

const wxString cli_options[][2] = {
  { wxT("### Global output control ###"),
    wxT("Several options that control the overall output that mkvmerge "
        "creates.") },
  { wxT("--cluster-length"),
    wxT("This option needs an additional argument 'n'. Tells mkvmerge to put "
        "at most 'n' data blocks into each cluster. If the number is"
        "postfixed with 'ms' then put at most 'n' milliseconds of data into "
        "each cluster. The maximum length for a cluster is 32767ms. Programs "
        "will only be able to seek to clusters, so creating larger clusters "
        "may lead to imprecise seeking and/or processing.") },
  { wxT("--no-cues"),
    wxT("Tells mkvmerge not to create and write the cue data which can be "
        "compared to an index in an AVI. Matroska files can be played back "
        "without the cue data, but seeking will probably be imprecise and "
        "slower. Use this only for testing purposes.") },
  { wxT("--no-clusters-in-meta-seek"),
    wxT("Tells mkvmerge not to create a meta seek element at the end of the "
        "file containing all clusters.") },
  { wxT("--disable-lacing"),
    wxT("Disables lacing for all tracks. This will increase the file's size, "
        "especially if there are many audio tracks. Use only for testing.") },
  { wxT("--enable-durations"),
    wxT("Write durations for all blocks. This will increase file size and "
        "does not offer any additional value for players at the moment.") },
  { wxT("### Development hacks ###"),
    wxT("Options meant ONLY for developpers. Do not use them. If something "
        "is considered to be an officially supported option then it's NOT "
        "in this list!") },
  { wxT("--engage space_after_chapters"),
    wxT("Leave additional space (EbmlVoid) in the output file after the "
        "chapters.") },
  { wxT("--engage no_chapters_in_meta_seek"),
    wxT("Do not add an entry for the chapters in the meta seek element.") },
  { wxT("--engage no_meta_seek"),
    wxT("Do not write meta seek elements at all.") },
  { wxT("--engage lacing_xiph"),
    wxT("Force Xiph style lacing.") },
  { wxT("--engage lacing_ebml"),
    wxT("Force EBML style lacing.") },
  { wxT("--engage native_bframes"),
    wxT("Analyze MPEG4 bitstreams, put each frame into one Matroska block, "
        "use proper timestamping (I P B B = 0 120 40 80), use "
        "V_MPEG4/ISO/... CodecIDs.") },
  { wxT("--engage no_variable_data"),
    wxT("Use fixed values for the elements that change with each file "
        "otherwise (muxing date, segment UID, track UIDs etc.). Two files "
        "muxed with the same settings and this switch activated will be "
        "identical.") },
  { wxT("--engage no_default_header_values"),
    wxT("Do not write those header elements whose values are the same "
        "as their default values according to the Matroska specs.") },
  { wxT("--engage force_passthrough_packetizer"),
    wxT("Forces the Matroska reader to use the generic passthrough "
        "packetizer even for known and supported track types.") },
  { wxT("--engage cow"),
    wxT("No help available.") },
  { wxT(""), wxT("") }
};


class cli_options_dlg: public wxDialog {
  DECLARE_CLASS(cli_options_dlg);
  DECLARE_EVENT_TABLE();
public:
  wxComboBox *cob_option;
  wxTextCtrl *tc_options, *tc_description;

public:
  cli_options_dlg(wxWindow *parent);
  void on_option_changed(wxCommandEvent &evt);
  void on_add_clicked(wxCommandEvent &evt);
  bool go(wxString &options);
};

cli_options_dlg::cli_options_dlg(wxWindow *parent):
  wxDialog(parent, 0, wxT("Add command line options"), wxDefaultPosition,
           wxSize(400, 350)) {
  wxBoxSizer *siz_all, *siz_line;
  wxButton *button;
  int i;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(0, 10, 0, 0, 0);
  siz_all->Add(new wxStaticText(this, -1, wxT("Here you can add more command "
                                              "line options either by\n"
                                              "entering them below or by "
                                              "chosing one from the drop\n"
                                              "down box and pressing the "
                                              "'add' button.")),
               0, wxLEFT | wxRIGHT, 10);
  siz_all->Add(0, 10, 0, 0, 0);
  siz_all->Add(new wxStaticText(this, -1, wxT("Command line options:")),
               0, wxLEFT, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  tc_options = new wxTextCtrl(this, -1);
  siz_all->Add(tc_options, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, 10, 0, 0, 0);
  siz_all->Add(new wxStaticText(this, -1, wxT("Available options:")),
               0, wxLEFT, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  cob_option = new wxComboBox(this, ID_CLIOPTIONS_COB);
  i = 0;
  while (cli_options[i][0].length() > 0) {
    cob_option->Append(cli_options[i][0]);
    i++;
  }
  cob_option->SetSelection(0);
  siz_line->Add(cob_option, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);
  button = new wxButton(this, ID_CLIOPTIONS_ADD, wxT("Add"));
  siz_line->Add(button, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_all->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, 10, 0, 0, 0);
  siz_all->Add(new wxStaticText(this, -1, wxT("Description:")), 0, wxLEFT, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  tc_description =
    new wxTextCtrl(this, -1, cli_options[0][1], wxDefaultPosition,
                   wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY |
                   wxTE_WORDWRAP);
  siz_all->Add(tc_description, 1, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, 10, 0, 0, 0);
  siz_all->Add(new wxStaticLine(this, -1), 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, 10, 0, 0, 0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(new wxButton(this, wxID_OK, wxT("Ok")), 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->Add(0, 10, 0, 0, 0);

  SetSizer(siz_all);
}

void
cli_options_dlg::on_option_changed(wxCommandEvent &evt) {
  int i;

  i = cob_option->GetSelection();
  if (i > 0)
    tc_description->SetValue(cli_options[i][1]);
}

void
cli_options_dlg::on_add_clicked(wxCommandEvent &evt) {
  wxString sel, opt;

  opt = cob_option->GetStringSelection();
  if (opt.Left(3) == wxT("###"))
    return;
  sel = tc_options->GetValue();
  if (sel.length() > 0)
    sel += wxT(" ");
  sel += opt;
  tc_options->SetValue(sel);
}

bool
cli_options_dlg::go(wxString &options) {
  tc_options->SetValue(options);
  if (ShowModal() == wxID_OK) {
    options = tc_options->GetValue();
    return true;
  }
  return false;
}

#if WXUNICODE
wxString
UTFstring_to_wxString(const UTFstring &u) {
  return wxString(u.c_str());
}
#else
wxString
UTFstring_to_wxString(const UTFstring &u) {
  wxString retval;
  char *c;
  c = UTFstring_to_cstr(u);
  retval = c;
  safefree(c);
  return retval;
}
#endif

wxString &
break_line(wxString &line,
           int break_after) {
  uint32_t i, chars;
  wxString broken;

  for (i = 0, chars = 0; i < line.length(); i++) {
    if (chars >= break_after) {
      if ((line[i] == wxT(' ')) || (line[i] == wxT('\t'))) {
        broken += wxT("\n");
        chars = 0;
      } else if (line[i] == wxT('(')) {
        broken += wxT("\n(");
        chars = 0;
      } else {
        broken += line[i];
        chars++;
      }
    } else if ((chars != 0) || (broken[i] != wxT(' '))) {
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

  if (source.Find(wxT("---")) == 0)
    return wxT("---");

  copy = source;
  if ((pos = copy.Find(wxT(" ("))) >= 0)
    copy.Remove(pos);

  return copy;
}

wxString
shell_escape(wxString source) {
  uint32_t i;
  wxString escaped;

  for (i = 0; i < source.Length(); i++) {
#if defined(SYS_UNIX) || defined(SYS_APPLE)
    if (source[i] == wxT('"'))
      escaped += wxT("\\\"");
    else if (source[i] == wxT('\\'))
      escaped += wxT("\\\\");
#else
    if (source[i] == wxT('"'))
      ;
#endif
    else if ((source[i] == wxT('\n')) || (source[i] == wxT('\r')))
      escaped += wxT(" ");
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
    if (source[i] == wxT('\n'))
      escaped += wxT(" ");
    else
      escaped += source[i];
  }

  return escaped;
}

vector<wxString>
split(const wxString &src,
      const wxString &pattern,
      int max_num) {
  int num, pos;
  wxString copy;
  vector<wxString> v;

  copy = src;
  pos = copy.Find(pattern);
  num = 1;
  while ((pos >= 0) && ((max_num == -1) || (num < max_num))) {
    v.push_back(copy.Left(pos));
    copy.Remove(0, pos + pattern.length());
    pos = copy.Find(pattern);
    num++;
  }
  v.push_back(copy);

  return v;
}

wxString
join(const wxString &pattern,
     vector<wxString> &strings) {
  wxString dst;
  uint32_t i;

  if (strings.size() == 0)
    return wxT("");
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

string
to_utf8(const wxString &src) {
  char *utf8;
  string retval;

#if WXUNICODE
  int len;

  len = wxConvUTF8.WC2MB(NULL, src.c_str(), 0);
  utf8 = (char *)safemalloc(len + 1);
  wxConvUTF8.WC2MB(utf8, src.c_str(), len + 1);
#else
  utf8 = to_utf8(cc_local_utf8, src.c_str());
#endif
  retval = utf8;
  safefree(utf8);

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
    if (src[current_char] == wxT('\\')) {
      if (next_char == src.length()) // This is an error...
        dst += wxT('\\');
      else {
        if (src[next_char] == wxT('2'))
          dst += wxT('"');
        else if (src[next_char] == wxT('s'))
          dst += wxT(' ');
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

wxString
format_date_time(time_t date_time) {
  wxString s;
  wxDateTime dt(date_time);

  s.Printf(wxT("%04d-%02d-%02d %02d:%02d:%02d"), dt.GetYear(),
           dt.GetMonth(), dt.GetDay(), dt.GetHour(), dt.GetMinute(),
           dt.GetSecond());
  return s;
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
    !strcmp(code, "zho") || // Chinese
    !strcmp(code, "nld") || // Dutch
    !strcmp(code, "eng") || // English
    !strcmp(code, "fin") || // Finnish
    !strcmp(code, "fre") || // French
    !strcmp(code, "ger") || // German
    !strcmp(code, "ita") || // Italian
    !strcmp(code, "jpn") || // Japanese
    !strcmp(code, "nor") || // Norwegian
    !strcmp(code, "por") || // Portuguese
    !strcmp(code, "rus") || // Russian
    !strcmp(code, "spa") || // Spanish
    !strcmp(code, "swe");   // Swedish
}

#if defined(SYS_WINDOWS)
wxString
format_tooltip(const wxString &s) {
  wxString tooltip(s);

  return break_line(tooltip, 30);
}
#endif

wxString
create_track_order() {
  int i;
  wxString s, wformat;
  string format;

  fix_format("%lld", format);
  wformat = wxU(format.c_str());
  for (i = 0; i < tracks.size(); i++) {
    if (!tracks[i]->enabled)
      continue;
    if (s.length() > 0)
      s += wxT(",");
    s += wxString::Format(wxT("%d:") + wformat, tracks[i]->source,
                          tracks[i]->id);
  }

  return s;
}

mmg_dialog::mmg_dialog(): wxFrame(NULL, -1, wxT("mkvmerge GUI v" VERSION),
                                  wxPoint(0, 0),
#ifdef SYS_WINDOWS
                                  wxSize(520, 740),
#else
                                  wxSize(520, 718),
#endif
                                  wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  mdlg = this;

  file_menu = new wxMenu();
  file_menu->Append(ID_M_FILE_NEW, wxT("&New\tCtrl-N"),
                    wxT("Start with empty settings"));
  file_menu->Append(ID_M_FILE_LOAD, wxT("&Load settings\tCtrl-L"),
                    wxT("Load muxing settings from a file"));
  file_menu->Append(ID_M_FILE_SAVE, wxT("&Save settings\tCtrl-S"),
                    wxT("Save muxing settings to a file"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_SETOUTPUT, wxT("Set &output file"),
                    wxT("Select the file you want to write to"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_EXIT, wxT("&Quit\tCtrl-Q"),
                    wxT("Quit the application"));

  file_menu_sep = false;
  update_file_menu();

  wxMenu *muxing_menu = new wxMenu();
  muxing_menu->Append(ID_M_MUXING_START,
                      wxT("Sta&rt muxing (run mkvmerge)\tCtrl-R"),
                      wxT("Run mkvmerge and start the muxing process"));
  muxing_menu->Append(ID_M_MUXING_COPY_CMDLINE,
                      wxT("&Copy command line to clipboard"),
                      wxT("Copy the command line to the clipboard"));
  muxing_menu->Append(ID_M_MUXING_SAVE_CMDLINE,
                      wxT("Sa&ve command line"),
                      wxT("Save the command line to a file"));
  muxing_menu->Append(ID_M_MUXING_CREATE_OPTIONFILE,
                      wxT("Create &option file"),
                      wxT("Save the command line to an option file "
                          "that can be read by mkvmerge"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_ADD_TO_JOBQUEUE,
                      wxT("&Add to job queue"),
                      wxT("Adds the current settings as a new job entry to "
                          "the job queue"));
  muxing_menu->Append(ID_M_MUXING_MANAGE_JOBS,
                      wxT("&Manage jobs"),
                      wxT("Brings up the job queue editor"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_ADD_CLI_OPTIONS,
                      wxT("Add &command line options"),
                      wxT("Lets you add arbitrary options to the command "
                          "line"));

  chapter_menu = new wxMenu();
  chapter_menu->Append(ID_M_CHAPTERS_NEW, wxT("&New chapters"),
                       wxT("Create a new chapter file"));
  chapter_menu->Append(ID_M_CHAPTERS_LOAD, wxT("&Load"),
                       wxT("Load a chapter file (simple/OGM format or XML "
                           "format)"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVE, wxT("&Save"),
                       wxT("Save the current chapters to a XML file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVETOKAX, wxT("Save to &Matroska file"),
                       wxT("Save the current chapters to an existing Matroska "
                           "file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVEAS, wxT("Save &as"),
                       wxT("Save the current chapters to a file with another "
                           "name"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_VERIFY, wxT("&Verify"),
                       wxT("Verify the current chapter entries to see if "
                           "there are any errors"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_SETDEFAULTS, wxT("Set &default values"));
  chapter_menu_sep = false;
  update_chapter_menu();

  wxMenu *window_menu = new wxMenu();
  window_menu->Append(ID_M_WINDOW_INPUT, wxT("&Input\tAlt-1"));
  window_menu->Append(ID_M_WINDOW_ATTACHMENTS, wxT("&Attachments\tAlt-2"));
  window_menu->Append(ID_M_WINDOW_GLOBAL, wxT("&Global options\tAlt-3"));
  window_menu->Append(ID_M_WINDOW_SETTINGS, wxT("&Settings\tAlt-4"));
  window_menu->AppendSeparator();
  window_menu->Append(ID_M_WINDOW_CHAPTEREDITOR,
                      wxT("&Chapter editor\tAlt-5"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HELP_ABOUT, wxT("&About\tF1"),
                    wxT("Show program information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(file_menu, wxT("&File"));
  menu_bar->Append(muxing_menu, wxT("&Muxing"));
  menu_bar->Append(chapter_menu, wxT("&Chapter Editor"));
  menu_bar->Append(window_menu, wxT("&Window"));
  menu_bar->Append(help_menu, wxT("&Help"));
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
  chapter_editor_page = new tab_chapters(notebook, chapter_menu);

  notebook->AddPage(input_page, wxT("Input"));
  notebook->AddPage(attachments_page, wxT("Attachments"));
  notebook->AddPage(global_page, wxT("Global"));
  notebook->AddPage(settings_page, wxT("Settings"));
  notebook->AddPage(chapter_editor_page, wxT("Chapter Editor"));

  bs_main->Add(notebook, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  wxStaticBox *sb_low = new wxStaticBox(panel, -1, wxT("Output filename"));
  wxStaticBoxSizer *sbs_low = new wxStaticBoxSizer(sb_low, wxHORIZONTAL);
  bs_main->Add(sbs_low, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  tc_output =
    new wxTextCtrl(panel, ID_TC_OUTPUT, wxT(""), wxDefaultPosition,
                   wxSize(400, -1), 0);
  sbs_low->Add(tc_output, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  b_browse_output =
    new wxButton(panel, ID_B_BROWSEOUTPUT, wxT("Browse"), wxDefaultPosition,
                 wxDefaultSize, 0);
  sbs_low->Add(b_browse_output, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

  sb_commandline =
    new wxStaticBox(panel, -1, wxT("Command line"));
  wxStaticBoxSizer *sbs_low2 =
    new wxStaticBoxSizer(sb_commandline, wxHORIZONTAL);
  bs_main->Add(sbs_low2, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT, 5);

  tc_cmdline =
    new wxTextCtrl(panel, ID_TC_CMDLINE, wxT(""), wxDefaultPosition,
                   wxSize(490, 50), wxTE_READONLY | wxTE_LINEWRAP |
                   wxTE_MULTILINE);
  sbs_low2->Add(tc_cmdline, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

  wxBoxSizer *bs_buttons = new wxBoxSizer(wxHORIZONTAL);

  b_start_muxing =
    new wxButton(panel, ID_B_STARTMUXING, wxT("Sta&rt muxing"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(b_start_muxing, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  b_copy_to_clipboard =
    new wxButton(panel, ID_B_COPYTOCLIPBOARD, wxT("&Copy to clipboard"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(10, 0);
  bs_buttons->Add(b_copy_to_clipboard, 0, wxALIGN_CENTER_HORIZONTAL | wxALL,
                  8);

  b_add_to_jobqueue =
    new wxButton(panel, ID_B_ADD_TO_JOBQUEUE, wxT("&Add to job queue"),
                 wxDefaultPosition, wxSize(130, -1));
  bs_buttons->Add(b_add_to_jobqueue, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  bs_main->Add(bs_buttons, 0, wxALIGN_CENTER_HORIZONTAL);

  last_open_dir = wxT("");
  cmdline = wxT("\"") + mkvmerge_path + wxT("\" -o \"") +
    tc_output->GetValue() + wxT("\" ");
  tc_cmdline->SetValue(cmdline);
  cmdline_timer.SetOwner(this, ID_T_UPDATECMDLINE);
  cmdline_timer.Start(1000);

  load_job_queue();

  SetIcon(wxICON(matroskalogo_big));

  set_status_bar(wxT("mkvmerge GUI ready"));

  if (app->argc > 1) {
    wxString file;

    file = app->argv[1];
    if (!wxFileExists(file) || wxDirExists(file))
      wxMessageBox(wxT("The file '") + file + wxT("' does not exist."),
                   wxT("Error loading settings"),
                   wxOK | wxCENTER | wxICON_ERROR);
    else {
#ifdef SYS_WINDOWS
      if ((file.Length() > 3) && (file.c_str()[1] != wxT(':')) &&
          (file.c_str()[0] != wxT('\\')))
        file = wxGetCwd() + wxT("\\") + file;
#else
      if ((file.Length() > 0) && (file.c_str()[0] != wxT('/')))
        file = wxGetCwd() + wxT("/") + file;
#endif
      load(file);
    }
  }
}

void
mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose an output file"), last_open_dir,
                   tc_output->GetValue().AfterLast(PSEP),
                   wxT("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                       ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_output->SetValue(dlg.GetPath());
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
  status_bar->SetStatusText(wxT(""));
}

void
mmg_dialog::on_quit(wxCommandEvent &evt) {
  Close(true);
}

void
mmg_dialog::on_file_new(wxCommandEvent &evt) {
  wxFileConfig *cfg;
  wxString tmp_name;

  tmp_name.Printf(wxT("tempsettings-%u.mmg"), getpid());
  cfg = new wxFileConfig(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), tmp_name);
  tc_output->SetValue(wxT(""));

  input_page->load(cfg);
  attachments_page->load(cfg);
  global_page->load(cfg);
  settings_page->load(cfg);

  delete cfg;
  wxRemoveFile(tmp_name);

  set_status_bar(wxT("Configuration cleared."));
}

void
mmg_dialog::on_file_load(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose an input file"), last_open_dir, wxT(""),
                   wxT("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    if (!wxFileExists(dlg.GetPath()) || wxDirExists(dlg.GetPath())) {
      wxMessageBox(wxT("The file does not exist."),
                   wxT("Error loading settings"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return;
    }
    last_open_dir = dlg.GetDirectory();
    load(dlg.GetPath());
  }
}

void
mmg_dialog::load(wxString file_name,
                 bool used_for_jobs) {
  wxFileConfig *cfg;
  wxString s;
  int version;

  cfg = new wxFileConfig(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), file_name);
  cfg->SetPath(wxT("/mkvmergeGUI"));
  if (!cfg->Read(wxT("file_version"), &version) || (version != 1)) {
    if (used_for_jobs)
      return;
    wxMessageBox(wxT("The file does not seem to be a valid mkvmerge GUI "
                     "settings file."), wxT("Error loading settings"),
                 wxOK | wxCENTER | wxICON_ERROR);
    return;
  }
  cfg->Read(wxT("output_file_name"), &s);
  tc_output->SetValue(s);
  if (!cfg->Read(wxT("cli_options"), &cli_options))
    cli_options = wxT("");

  input_page->load(cfg);
  attachments_page->load(cfg);
  global_page->load(cfg);
  settings_page->load(cfg);

  delete cfg;

  if (!used_for_jobs) {
    set_last_settings_in_menu(file_name);
    set_status_bar(wxT("Configuration loaded."));
  }
}

void
mmg_dialog::on_file_save(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT("mkvmerge GUI settings (*.mmg)|*.mmg|" ALLFILES),
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

  cfg = new wxFileConfig(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), file_name);
  cfg->SetPath(wxT("/mkvmergeGUI"));
  cfg->Write(wxT("file_version"), 1);
  cfg->Write(wxT("gui_version"), wxU(VERSION));
  cfg->Write(wxT("output_file_name"), tc_output->GetValue());
  cfg->Write(wxT("cli_options"), cli_options);

  input_page->save(cfg);
  attachments_page->save(cfg);
  global_page->save(cfg);
  settings_page->save(cfg);

  delete cfg;

  if (!used_for_jobs) {
    set_status_bar(wxT("Configuration saved."));
    set_last_settings_in_menu(file_name);
  }
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
  cfg->SetPath(wxT("/GUI"));
  for (i = 0; i < last_settings.size(); i++) {
    s.Printf(wxT("last_settings %d"), i);
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
  cfg->SetPath(wxT("/GUI"));
  for (i = 0; i < last_chapters.size(); i++) {
    s.Printf(wxT("last_chapters %d"), i);
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
    wxMessageBox(wxT("You have not yet selected an output file."),
                 wxT("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings() ||
      !settings_page->validate_settings())
    return;

  if (settings_page->cb_ask_before_overwriting->IsChecked() &&
      wxFile::Exists(tc_output->GetValue()) &&
      (wxMessageBox(wxT("The output file '") + tc_output->GetValue() +
                    wxT("' does already exists. Do you want to overwrite it?"),
                    wxT("Overwrite existing file?"), wxYES_NO) != wxYES))
    return;

  set_on_top(false);
  mux_dlg = new mux_dialog(this);
  delete mux_dlg;
  restore_on_top();
}

void
mmg_dialog::on_about(wxCommandEvent &evt) {
  wxMessageBox(wxT("mkvmerge GUI v" VERSION " built on " __DATE__ " "
                   __TIME__ "\n"
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
               wxT("About mkvmerge's GUI"),
               wxOK | wxCENTER | wxICON_INFORMATION);
}

void
mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT(ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = cmdline + wxT("\n");
    file->Write(s);
    delete file;

    set_status_bar(wxT("Command line saved."));
  }
}

void
mmg_dialog::on_create_optionfile(wxCommandEvent &evt) {
  const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
  uint32_t i;
  string arg_utf8;
  wxFile *file;

  wxFileDialog dlg(NULL, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT(ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    try {
      file = new wxFile(dlg.GetPath(), wxFile::write);
      file->Write(utf8_bom, 3);
    } catch (...) {
      wxMessageBox(wxT("Could not create the specified file."),
                   wxT("File creation failed"), wxOK | wxCENTER |
                   wxICON_ERROR);
      return;
    }
    for (i = 1; i < clargs.Count(); i++) {
      if (clargs[i].length() == 0)
        file->Write(wxT("#EMPTY#"), wxConvUTF8);
      else
        file->Write(clargs[i], wxConvUTF8);
      file->Write(wxT("\n"), wxConvUTF8);
    }
    delete file;

    set_status_bar(wxT("Option file created."));
  }
}

void
mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
  update_command_line();
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(cmdline));
    wxTheClipboard->Close();
    set_status_bar(wxT("Command line copied to clipboard."));
  } else
    set_status_bar(wxT("Could not open the clipboard."));
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
  cmdline = wxT("\"") + mkvmerge_path + wxT("\" -o \"") +
    tc_output->GetValue() + wxT("\" ") + wxT("--command-line-charset UTF-8 ");

  clargs.Clear();
  clargs.Add(mkvmerge_path);
  clargs.Add(wxT("-o"));
  clargs.Add(tc_output->GetValue());
  args_start = clargs.Count();

  if (settings_page->cob_priority->GetValue() != wxT("normal")) {
    clargs.Add(wxT("--priority"));
    clargs.Add(settings_page->cob_priority->GetValue());
  }

  for (fidx = 0; fidx < files.size(); fidx++) {
    f = &files[fidx];
    tracks_selected_here = false;
    no_audio = true;
    no_video = true;
    no_subs = true;
    aids = wxT("");
    sids = wxT("");
    dids = wxT("");
    for (tidx = 0; tidx < f->tracks->size(); tidx++) {
      string format;

      t = &(*f->tracks)[tidx];
      if (!t->enabled)
        continue;

      tracks_selected_here = true;
      fix_format("%lld", format);
      sid.Printf(wxU(format.c_str()), t->id);

      if (t->type == wxT('a')) {
        no_audio = false;
        if (aids.length() > 0)
          aids += wxT(",");
        aids += sid;

        if (t->aac_is_sbr) {
          clargs.Add(wxT("--aac-is-sbr"));
          clargs.Add(sid);
        }

      } else if (t->type == wxT('v')) {
        no_video = false;
        if (dids.length() > 0)
          dids += wxT(",");
        dids += sid;

      } else if (t->type == wxT('s')) {
        no_subs = false;
        if (sids.length() > 0)
          sids += wxT(",");
        sids += sid;

        if ((t->sub_charset->Length() > 0) &&
            (*t->sub_charset != wxT("default"))) {
          clargs.Add(wxT("--sub-charset"));
          clargs.Add(sid + wxT(":") + shell_escape(*t->sub_charset));
        }
      }

      if (*t->language != wxT("none")) {
        clargs.Add(wxT("--language"));
        clargs.Add(sid + wxT(":") + extract_language_code(*t->language));
      }

      if (*t->cues != wxT("default")) {
        clargs.Add(wxT("--cues"));
        if (*t->cues == wxT("only for I frames"))
          clargs.Add(sid + wxT(":iframes"));
        else if (*t->cues == wxT("for all frames"))
          clargs.Add(sid + wxT(":all"));
        else if (*t->cues == wxT("none"))
          clargs.Add(sid + wxT(":none"));
      }

      if ((t->delay->Length() > 0) || (t->stretch->Length() > 0)) {
        arg = sid + wxT(":");
        if (t->delay->Length() > 0)
          arg += *t->delay;
        else
          arg += wxT("0");
        if (t->stretch->Length() > 0)
          arg += *t->stretch;
        clargs.Add(wxT("--sync"));
        clargs.Add(arg);
      }

      if ((t->track_name->Length() > 0) || t->track_name_was_present) {
        clargs.Add(wxT("--track-name"));
        clargs.Add(sid + wxT(":") + *t->track_name);
      }

      if (t->default_track) {
        clargs.Add(wxT("--default-track"));
        clargs.Add(sid);
      }

      if (t->tags->Length() > 0) {
        clargs.Add(wxT("--tags"));
        clargs.Add(sid + wxT(":") + *t->tags);
      }

      if (!t->display_dimensions_selected && (t->aspect_ratio->Length() > 0)) {
        clargs.Add(wxT("--aspect-ratio"));
        clargs.Add(sid + wxT(":") + *t->aspect_ratio);
      } else if (t->display_dimensions_selected &&
                 (t->dwidth->Length() > 0) && (t->dheight->Length() > 0)) {
        clargs.Add(wxT("--display-dimensions"));
        clargs.Add(sid + wxT(":") + *t->dwidth + wxT("x") + *t->dheight);
      }

      if (t->fourcc->Length() > 0) {
        clargs.Add(wxT("--fourcc"));
        clargs.Add(sid + wxT(":") + *t->fourcc);
      }

      if (t->compression->Length() > 0) {
        clargs.Add(wxT("--compression"));
        clargs.Add(sid + wxT(":") + *t->compression);
      }

      if (t->timecodes->Length() > 0) {
        clargs.Add(wxT("--timecodes"));
        clargs.Add(sid + wxT(":") + *t->timecodes);
      }
    }

    if (aids.length() > 0) {
      clargs.Add(wxT("-a"));
      clargs.Add(aids);
    }
    if (dids.length() > 0) {
      clargs.Add(wxT("-d"));
      clargs.Add(dids);
    }
    if (sids.length() > 0) {
      clargs.Add(wxT("-s"));
      clargs.Add(sids);
    }

    if (tracks_selected_here) {
      if (f->no_chapters)
        clargs.Add(wxT("--no-chapters"));
      if (f->no_attachments)
        clargs.Add(wxT("--no-attachments"));
      if (f->no_tags)
        clargs.Add(wxT("--no-tags"));

      if (no_video)
        clargs.Add(wxT("-D"));
      if (no_audio)
        clargs.Add(wxT("-A"));
      if (no_subs)
        clargs.Add(wxT("-S"));

      clargs.Add(*f->file_name);
    }
  }

  track_order = create_track_order();
  if (track_order.length() > 0) {
    clargs.Add(wxT("--track-order"));
    clargs.Add(track_order);
  }

  for (fidx = 0; fidx < attachments.size(); fidx++) {
    a = &attachments[fidx];

    clargs.Add(wxT("--attachment-mime-type"));
    clargs.Add(*a->mime_type);
    if (a->description->Length() > 0) {
      clargs.Add(wxT("--attachment-description"));
      clargs.Add(no_cr(*a->description));
    }
    if (a->style == 0)
      clargs.Add(wxT("--attach-file"));
    else
      clargs.Add(wxT("--attach-file-once"));
    clargs.Add(*a->file_name);
  }

  if (global_page->tc_title->GetValue().Length() > 0) {
    clargs.Add(wxT("--title"));
    clargs.Add(global_page->tc_title->GetValue());
  } else if (title_was_present) {
    clargs.Add(wxT("--title"));
    clargs.Add(wxT(""));
  }

  if (global_page->cb_split->IsChecked()) {
    clargs.Add(wxT("--split"));
    if (global_page->rb_split_by_size->GetValue())
      clargs.Add(global_page->cob_split_by_size->GetValue());
    else
      clargs.Add(global_page->cob_split_by_time->GetValue());

    if (global_page->tc_split_max_files->GetValue().Length() > 0) {
      clargs.Add(wxT("--split-max-files"));
      clargs.Add(global_page->tc_split_max_files->GetValue());
    }

    if (global_page->cb_link->IsChecked())
      clargs.Add(wxT("--link"));
  }

  if (global_page->tc_previous_segment_uid->GetValue().Length() > 0) {
    clargs.Add(wxT("--link-to-previous"));
    clargs.Add(global_page->tc_previous_segment_uid->GetValue());
  }

  if (global_page->tc_next_segment_uid->GetValue().Length() > 0) {
    clargs.Add(wxT("--link-to-next"));
    clargs.Add(global_page->tc_next_segment_uid->GetValue());
  }

  if (global_page->tc_chapters->GetValue().Length() > 0) {
    if (global_page->cob_chap_language->GetValue().Length() > 0) {
      clargs.Add(wxT("--chapter-language"));
      clargs.Add(extract_language_code(global_page->
                                       cob_chap_language->GetValue()));
    }

    if (global_page->cob_chap_charset->GetValue().Length() > 0) {
      clargs.Add(wxT("--chapter-charset"));
      clargs.Add(global_page->cob_chap_charset->GetValue());
    }

    if (global_page->tc_cue_name_format->GetValue().Length() > 0) {
      clargs.Add(wxT("--cue-chapter-name-format"));
      clargs.Add(global_page->tc_cue_name_format->GetValue());
    }

    clargs.Add(wxT("--chapters"));
    clargs.Add(global_page->tc_chapters->GetValue());
  }

  if (global_page->tc_global_tags->GetValue().Length() > 0) {
    clargs.Add(wxT("--global-tags"));
    clargs.Add(global_page->tc_global_tags->GetValue());
  }

  cli_options = strip(cli_options);
  if (cli_options.length() > 0) {
    vector<wxString> opts;
    int i;

    opts = split(cli_options, wxString(wxT(" ")));
    for (i = 0; i < opts.size(); i++)
      clargs.Add(strip(opts[i]));
  }

  for (i = args_start; i < clargs.Count(); i++) {
    if (clargs[i].Find(wxT(" ")) >= 0)
      cmdline += wxT(" \"") + shell_escape(clargs[i]) + wxT("\"");
    else
      cmdline += wxT(" ") + shell_escape(clargs[i]);
  }

#if WXUNICODE
  string utf8;
  char *tmp;
  utf8 = to_utf8(cmdline);
  tmp = to_utf8(utf8_init("ISO8859-1"), utf8.c_str());
  cmdline = wxU(tmp);
  safefree(tmp);
#else
  cmdline = to_utf8(cmdline).c_str();
#endif
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

  notebook->SetSelection(4);
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
    s.Printf(wxT("&%u. %s"), i + 1, last_settings[i].c_str());
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
    s.Printf(wxT("&%u. %s"), i + 1, last_chapters[i].c_str());
    chapter_menu->Append(ID_M_CHAPTERS_LOADLAST1 + i, s);
  }
}

void
mmg_dialog::on_new_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_new_chapters(evt);
}

void
mmg_dialog::on_load_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_load_chapters(evt);
}

void
mmg_dialog::on_save_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_save_chapters(evt);
}

void
mmg_dialog::on_save_chapters_to_kax_file(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_save_chapters_to_kax_file(evt);
}

void
mmg_dialog::on_save_chapters_as(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_save_chapters_as(evt);
}

void
mmg_dialog::on_verify_chapters(wxCommandEvent &evt) {
  notebook->SetSelection(4);
  chapter_editor_page->on_verify_chapters(evt);
}

void
mmg_dialog::on_set_default_chapter_values(wxCommandEvent &evt) {
  notebook->SetSelection(4);
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
    output += wxT(".mkv");
    tc_output->SetValue(output);
  }
}

void
mmg_dialog::on_add_to_jobqueue(wxCommandEvent &evt) {
  wxString description, line;
  job_t job;
  int i, result;
  bool ok;

  if (tc_output->GetValue().Length() == 0) {
    wxMessageBox(wxT("You have not yet selected an output file."),
                 wxT("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings() ||
      !settings_page->validate_settings())
    return;

  line = wxT("The output file '") + tc_output->GetValue() +
    wxT("' does already exists. Do you want to overwrite it?");
  if (settings_page->cb_ask_before_overwriting->IsChecked() &&
      wxFile::Exists(tc_output->GetValue()) &&
      (wxMessageBox(break_line(line, 60), wxT("Overwrite existing file?"),
                    wxYES_NO) != wxYES))
    return;

  description = tc_output->GetValue().AfterLast(wxT('/')).AfterLast(wxT('\\'));
  description = description.AfterLast(wxT('/')).BeforeLast(wxT('.'));
  ok = false;
  do {
    description = wxGetTextFromUser(wxT("Please enter a description for the "
                                        "new job:"), wxT("Job description"),
                                    description);
    if (description.length() == 0)
      return;

    if (!settings_page->cb_ask_before_overwriting->IsChecked())
      break;
    line = wxT("A job with the description '") + description +
      wxT("' does already exist. Do you really want to add another one "
          "with the same description?");
    break_line(line, 60);
    ok = true;
    for (i = 0; i < jobs.size(); i++)
      if (description == *jobs[i].description) {
        ok = false;
        result = wxMessageBox(line, wxT("Description already exists"),
                              wxYES_NO | wxCANCEL);
        if (result == wxYES)
          ok = true;
        else if (result == wxCANCEL)
          return;
        break;
      }
  } while (!ok);

  if (!wxDirExists(wxT("jobs")))
    wxMkdir(wxT("jobs"));

  last_job_id++;
  if (last_job_id > 2000000000)
    last_job_id = 0;
  job.id = last_job_id;
  job.status = jobs_pending;
  job.added_on = wxGetLocalTime();
  job.started_on = -1;
  job.finished_on = -1;
  job.description = new wxString(description);
  job.log = new wxString();
  jobs.push_back(job);

  description.Printf(wxT("/jobs/%d.mmg"), job.id);
  save(wxGetCwd() + description);

  save_job_queue();

  set_status_bar(wxT("Job added to job queue"));
}

void
mmg_dialog::on_manage_jobs(wxCommandEvent &evt) {
  set_on_top(false);
  job_dialog jdlg(this);
  restore_on_top();
}

void
mmg_dialog::load_job_queue() {
  int num, i, value;
  wxString s;
  wxConfigBase *cfg;
  job_t job;

  last_job_id = 0;

  cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/jobs"));
  cfg->Read(wxT("last_job_id"), &last_job_id);
  if (!cfg->Read(wxT("number_of_jobs"), &num))
    return;

  for (i = 0; i < jobs.size(); i++)
    delete jobs[i].description;
  jobs.clear();

  for (i = 0; i < num; i++) {
    s.Printf(wxT("/jobs/%u"), i);
    if (!cfg->HasGroup(s))
      continue;
    cfg->SetPath(s);
    cfg->Read(wxT("id"), &value);
    job.id = value;
    cfg->Read(wxT("status"), &s);
    job.status =
      s == wxT("pending") ? jobs_pending :
      s == wxT("done") ? jobs_done :
      s == wxT("done_warnings") ? jobs_done_warnings :
      s == wxT("aborted") ? jobs_aborted :
      jobs_failed;
    cfg->Read(wxT("added_on"), &value);
    job.added_on = value;
    cfg->Read(wxT("started_on"), &value);
    job.started_on = value;
    cfg->Read(wxT("finished_on"), &value);
    job.finished_on = value;
    cfg->Read(wxT("description"), &s);
    job.description = new wxString(s);
    cfg->Read(wxT("log"), &s);
    s.Replace(wxT(":::"), wxT("\n"));
    job.log = new wxString(s);
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
  cfg->SetPath(wxT("/jobs"));
  if (cfg->GetFirstGroup(s, cookie)) {
    do {
      job_groups.push_back(s);
    } while (cfg->GetNextGroup(s, cookie));
    for (i = 0; i < job_groups.size(); i++)
      cfg->DeleteGroup(job_groups[i]);
  }
  cfg->Write(wxT("last_job_id"), last_job_id);
  cfg->Write(wxT("number_of_jobs"), (int)jobs.size());
  for (i = 0; i < jobs.size(); i++) {
    s.Printf(wxT("/jobs/%u"), i);
    cfg->SetPath(s);
    cfg->Write(wxT("id"), jobs[i].id);
    cfg->Write(wxT("status"),
               jobs[i].status == jobs_pending ? wxT("pending") :
               jobs[i].status == jobs_done ? wxT("done") :
               jobs[i].status == jobs_done_warnings ? wxT("done_warnings") :
               jobs[i].status == jobs_aborted ? wxT("aborted") :
               wxT("failed"));
    cfg->Write(wxT("added_on"), jobs[i].added_on);
    cfg->Write(wxT("started_on"), jobs[i].started_on);
    cfg->Write(wxT("finished_on"), jobs[i].finished_on);
    cfg->Write(wxT("description"), *jobs[i].description);
    s = *jobs[i].log;
    s.Replace(wxT("\n"), wxT(":::"));
    cfg->Write(wxT("log"), s);
  }
  cfg->Flush();
}

void
mmg_dialog::on_add_cli_options(wxCommandEvent &evt) {
  cli_options_dlg dlg(this);

  if (dlg.go(cli_options))
    update_command_line();
}

void
set_on_top(bool on_top) {
  long style;

  style = mdlg->GetWindowStyleFlag();
  if (on_top)
    style |= wxSTAY_ON_TOP;
  else
    style &= ~wxSTAY_ON_TOP;
  mdlg->SetWindowStyleFlag(style);
}

void
restore_on_top() {
  set_on_top(mdlg->settings_page->cb_on_top->IsChecked());
}

IMPLEMENT_CLASS(cli_options_dlg, wxDialog);
BEGIN_EVENT_TABLE(cli_options_dlg, wxDialog)
  EVT_COMBOBOX(ID_CLIOPTIONS_COB, cli_options_dlg::on_option_changed)
  EVT_BUTTON(ID_CLIOPTIONS_ADD, cli_options_dlg::on_add_clicked)
END_EVENT_TABLE();

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
  EVT_MENU(ID_M_MUXING_ADD_CLI_OPTIONS, mmg_dialog::on_add_cli_options)
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
  EVT_MENU(ID_M_WINDOW_SETTINGS, mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_CHAPTEREDITOR, mmg_dialog::on_window_selected)
END_EVENT_TABLE();

bool
mmg_app::OnInit() {
  wxConfigBase *cfg;
  uint32_t i;
  wxString k, v;

  cc_local_utf8 = utf8_init(NULL);

  cfg = new wxConfig(wxT("mkvmergeGUI"));
  wxConfigBase::Set(cfg); 
  cfg->SetPath(wxT("/GUI"));
  if (!cfg->Read(wxT("last_directory"), &last_open_dir))
    last_open_dir = wxT("");
  for (i = 0; i < 4; i++) {
    k.Printf(wxT("last_settings %u"), i);
    if (cfg->Read(k, &v))
      last_settings.push_back(v);
    k.Printf(wxT("last_chapters %u"), i);
    if (cfg->Read(k, &v))
      last_chapters.push_back(v);
  }
  cfg->SetPath(wxT("/chapter_editor"));
  if (cfg->Read(wxT("default_language"), &k)) {
    if (k.length() == 0)
      k = wxT("eng");
    default_chapter_language = to_utf8(k).c_str();
  } else
    default_chapter_language = "eng";
  if (cfg->Read(wxT("default_country"), &k))
    default_chapter_country = to_utf8(k).c_str();

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
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("last_directory"), last_open_dir);
  cfg->SetPath(wxT("/chapter_editor"));
  cfg->Write(wxT("default_language"), wxCS2WS(default_chapter_language));
  cfg->Write(wxT("default_country"), wxCS2WS(default_chapter_country));
  cfg->Flush();

  delete cfg;

  utf8_done();

  return 0;
}

IMPLEMENT_APP(mmg_app)
