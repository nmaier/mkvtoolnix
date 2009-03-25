/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   main stuff

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <stdarg.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/config.h>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/statusbr.h>
#include <wx/statline.h>
#include <wx/strconv.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "extern_data.h"
#include "header_editor_frame.h"
#include "jobs.h"
#include "mkvmerge.h"
#include "matroskalogo.xpm"
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"
#include "options_dialog.h"
#include "tab_attachments.h"
#include "tab_chapters.h"
#include "tab_input.h"
#include "tab_global.h"
#include "translation.h"
#include "xml_element_mapping.h"
#include "wxcommon.h"

mmg_app *app;
mmg_dialog *mdlg;
wxString last_open_dir;
vector<wxString> last_settings;
vector<wxString> last_chapters;
vector<mmg_file_cptr> files;
vector<mmg_track_t *> tracks;
map<wxString, wxString> capabilities;
vector<job_t> jobs;

#define ID_CLIOPTIONS_COB 2000
#define ID_CLIOPTIONS_ADD 2001

struct cli_option_t {
  wxString option, description;

  cli_option_t(const wxString &n_option,
               const wxString &n_description)
    : option(n_option)
    , description(n_description) {
  };
};

class cli_options_dlg: public wxDialog {
  DECLARE_CLASS(cli_options_dlg);
  DECLARE_EVENT_TABLE();
public:
  static vector<cli_option_t> all_cli_options;

public:
  wxComboBox *cob_option;
  wxTextCtrl *tc_options, *tc_description;

public:
  cli_options_dlg(wxWindow *parent);
  void on_option_changed(wxCommandEvent &evt);
  void on_add_clicked(wxCommandEvent &evt);
  bool go(wxString &options);

public:
  static void init_cli_option_list();
};

vector<cli_option_t> cli_options_dlg::all_cli_options;

cli_options_dlg::cli_options_dlg(wxWindow *parent):
  wxDialog(parent, 0, Z("Add command line options"), wxDefaultPosition, wxSize(400, 350)) {
  wxBoxSizer *siz_all, *siz_line;
  wxButton *button;
  int i;

  if (all_cli_options.empty())
    init_cli_option_list();

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Here you can add more command line options either by\n"
                                            "entering them below or by chosing one from the drop\n"
                                            "down box and pressing the 'add' button.")), 0, wxLEFT | wxRIGHT, 10);
  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Command line options:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);
  tc_options = new wxTextCtrl(this, -1);
  siz_all->Add(tc_options, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Available options:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  cob_option = new wxComboBox(this, ID_CLIOPTIONS_COB);

  for (i = 0; i < all_cli_options.size(); i++)
    cob_option->Append(all_cli_options[i].option);
  cob_option->SetSelection(0);
  siz_line->Add(cob_option, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);
  button = new wxButton(this, ID_CLIOPTIONS_ADD, Z("Add"));
  siz_line->Add(button, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_all->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Description:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);
  tc_description = new wxTextCtrl(this, -1, all_cli_options[0].description, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
  siz_all->Add(tc_description, 1, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticLine(this, -1), 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_line->AddSpacer(10);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizer(siz_all);
}

void
cli_options_dlg::init_cli_option_list() {
  all_cli_options.clear();
  all_cli_options.push_back(cli_option_t(  Z("### Global output control ###"),
                                           Z("Several options that control the overall output that mkvmerge creates.")));
  all_cli_options.push_back(cli_option_t(wxU("--cluster-length"),
                                           Z("This option needs an additional argument 'n'. Tells mkvmerge to put at most 'n' data blocks into each cluster. "
                                             "If the number is postfixed with 'ms' then put at most 'n' milliseconds of data into each cluster. "
                                             "The maximum length for a cluster is 32767ms. Programs will only be able to seek to clusters, so creating larger clusters "
                                             "may lead to imprecise seeking and/or processing.")));
  all_cli_options.push_back(cli_option_t(wxU("--no-cues"),
                                           Z("Tells mkvmerge not to create and write the cue data which can be compared to an index in an AVI. "
                                             "Matroska files can be played back without the cue data, but seeking will probably be imprecise and slower. "
                                             "Use this only for testing purposes.")));
  all_cli_options.push_back(cli_option_t(wxU("--no-clusters-in-meta-seek"),
                                           Z("Tells mkvmerge not to create a meta seek element at the end of the file containing all clusters.")));
  all_cli_options.push_back(cli_option_t(wxU("--disable-lacing"),
                                           Z("Disables lacing for all tracks. This will increase the file's size, especially if there are many audio tracks. Use only for testing.")));
  all_cli_options.push_back(cli_option_t(wxU("--enable-durations"),
                                           Z("Write durations for all blocks. This will increase file size and does not offer any additional value for players at the moment.")));
  all_cli_options.push_back(cli_option_t(  Z("--timecode-scale REPLACEME"),
                                           Z("Forces the timecode scale factor to REPLACEME. You have to replace REPLACEME with a value between 1000 and 10000000 or with -1. "
                                             "Normally mkvmerge will use a value of 1000000 which means that timecodes and durations will have a precision of 1ms. "
                                             "For files that will not contain a video track but at least one audio track mkvmerge will automatically choose a timecode scale factor "
                                             "so that all timecodes and durations have a precision of one sample. "
                                             "This causes bigger overhead but allows precise seeking and extraction. "
                                             "If the magical value -1 is used then mkvmerge will use sample precision even if a video track is present.")));
  all_cli_options.push_back(cli_option_t(wxU("### Development hacks ###"),
                                           Z("Options meant ONLY for developpers. Do not use them. If something is considered to be an officially supported option "
                                             "then it's NOT in this list!")));
  all_cli_options.push_back(cli_option_t(wxU("--engage space_after_chapters"),
                                           Z("Leave additional space (EbmlVoid) in the output file after the chapters.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_chapters_in_meta_seek"),
                                           Z("Do not add an entry for the chapters in the meta seek element.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_meta_seek"),
                                           Z("Do not write meta seek elements at all.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage lacing_xiph"),
                                           Z("Force Xiph style lacing.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage lacing_ebml"),
                                           Z("Force EBML style lacing.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage native_mpeg4"),
                                           Z("Analyze MPEG4 bitstreams, put each frame into one Matroska block, use proper timestamping (I P B B = 0 120 40 80), "
                                             "use V_MPEG4/ISO/... CodecIDs.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_variable_data"),
                                           Z("Use fixed values for the elements that change with each file otherwise (muxing date, segment UID, track UIDs etc.). "
                                             "Two files muxed with the same settings and this switch activated will be identical.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_default_header_values"),
                                           Z("Do not write those header elements whose values are the same "
                                             "as their default values according to the Matroska specs.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage force_passthrough_packetizer"),
                                           Z("Forces the Matroska reader to use the generic passthrough packetizer even for known and supported track types.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage allow_avc_in_vfw_mode"),
                                           Z("Allows storing AVC/h.264 video in Video-for-Windows compatibility mode, e.g. when it is read from an AVI")));
  all_cli_options.push_back(cli_option_t(wxU("--engage use_simpleblock"),
                                           Z("Enable use of SimpleBlock instead of BlockGroup when possible.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage old_aac_codecid"),
                                           Z("Use the old AAC codec IDs (e.g. 'A_AAC/MPEG4/SBR') instead of the new one ('A_AAC').")));
  all_cli_options.push_back(cli_option_t(wxU("--engage use_codec_state"),
                                           Z("Allows the use of the CodecState element. This is used for e.g. MPEG-1/-2 video tracks for storing the sequence headers.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage cow"),
                                           Z("No help available.")));
}

void
cli_options_dlg::on_option_changed(wxCommandEvent &evt) {
  int i;

  i = cob_option->GetSelection();
  if (i > 0)
    tc_description->SetValue(all_cli_options[i].description);
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

class show_text_dlg: public wxDialog {
  DECLARE_CLASS(show_text_dlg);
  DECLARE_EVENT_TABLE();
public:
  show_text_dlg(wxWindow *parent, const wxString &title, const wxString &text);
};

show_text_dlg::show_text_dlg(wxWindow *parent,
                             const wxString &title,
                             const wxString &text):
  wxDialog(parent, 0, title, wxDefaultPosition, wxSize(400, 350)) {

  wxBoxSizer *siz_all, *siz_line;
  wxTextCtrl *tc_message;

  siz_all = new wxBoxSizer(wxVERTICAL);

  tc_message = new wxTextCtrl(this, 0, text, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(tc_message, 1, wxALL | wxGROW, 10);

  siz_all->Add(siz_line, 1, wxGROW, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK), 0, 0, 0);
  siz_line->AddSpacer(10);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizer(siz_all);
}

void
mmg_options_t::validate() {
  if (   (ODM_FROM_FIRST_INPUT_FILE != output_directory_mode)
      && (ODM_PREVIOUS              != output_directory_mode)
      && (ODM_FIXED                 != output_directory_mode))
    output_directory_mode = ODM_FROM_FIRST_INPUT_FILE;

  if (   (priority != wxU("highest")) && (priority != wxU("higher")) && (priority != wxU("normal"))
      && (priority != wxU("lower"))   && (priority != wxU("lowest")))
    priority = wxU("normal");
}

wxString
UTFstring_to_wxString(const UTFstring &u) {
  return wxString(u.c_str());
}

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
    return wxEmptyString;
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
  string retval;

  int len    = wxConvUTF8.WC2MB(NULL, src.c_str(), 0);
  char *utf8 = (char *)safemalloc(len + 1);
  wxConvUTF8.WC2MB(utf8, src.c_str(), len + 1);
  retval = utf8;
  safefree(utf8);

  return retval;
}

wxString
from_utf8(const wxString &src) {
  return src;
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
  return wxDateTime(date_time).Format(wxT("%Y-%m-%d %H:%M:%S"));
}

#if defined(SYS_WINDOWS)
wxString
format_tooltip(const wxString &s) {
  static bool first = true;
  wxString tooltip(s), nl(wxT("\n"));
  unsigned int i;

  if (!first)
    return s;

  for (i = 60; i < tooltip.length(); ++i)
    if (wxT(' ') == tooltip[i]) {
      first = false;
      return tooltip.Left(i) + nl + tooltip.Right(tooltip.length() - i - 1);
    } else if (wxT('(') == tooltip[i]) {
      first = false;
      return tooltip.Left(i) + nl + tooltip.Right(tooltip.length() - i);
    }

  return tooltip;
}
#endif

wxString
get_temp_dir() {
  wxString temp_dir;

  wxGetEnv(wxT("TMP"), &temp_dir);
  if (temp_dir == wxEmptyString)
    wxGetEnv(wxT("TEMP"), &temp_dir);
  if ((temp_dir == wxEmptyString) && wxDirExists(wxT("/tmp")))
    temp_dir = wxT("/tmp");
  if (temp_dir != wxEmptyString)
    temp_dir += wxT(PATHSEP);

  return temp_dir;
}

wxString
create_track_order(bool all) {
  int i;
  wxString s, format;
  string temp;

  fix_format("%d:" LLD, temp);
  format = wxU(temp.c_str());
  for (i = 0; i < tracks.size(); i++) {
    if (!all && (!tracks[i]->enabled || tracks[i]->appending))
      continue;
    if (s.length() > 0)
      s += wxT(",");
    s += wxString::Format(format, tracks[i]->source, tracks[i]->id);
  }

  return s;
}

wxString
create_append_mapping() {
  int i;
  wxString s, format;
  string temp;

  fix_format("%d:" LLD ":%d:" LLD, temp);
  format = wxU(temp.c_str());
  for (i = 1; i < tracks.size(); i++) {
    if (!tracks[i]->enabled || !tracks[i]->appending)
      continue;
    if (s.length() > 0)
      s += wxT(",");
    s += wxString::Format(format, tracks[i]->source, tracks[i]->id,
                          tracks[i - 1]->source, tracks[i - 1]->id);
  }

  return s;
}

int
default_track_checked(char type) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if ((tracks[i]->type == type) && (1 == tracks[i]->default_track))
      return i;
  return -1;
}

void
set_combobox_selection(wxComboBox *cb,
                       const wxString wanted) {
  int i, count;

  cb->SetValue(wanted);
  count = cb->GetCount();
  for (i = 0; count > i; ++i)
    if (cb->GetString(i) == wanted) {
      cb->SetSelection(i);
      break;
    }
}

#if defined(USE_WXBITMAPCOMBOBOX)
void
set_combobox_selection(wxBitmapComboBox *cb,
                       const wxString wanted) {
  int i, count;

  cb->SetValue(wanted);
  count = cb->GetCount();
  for (i = 0; count > i; ++i)
    if (cb->GetString(i) == wanted) {
      cb->SetSelection(i);
      break;
    }
}
#endif  // USE_WXBITMAPCOMBOBOX

void
wxdie(const wxString &errmsg) {
  wxMessageBox(errmsg, wxT("A serious error has occured"),
               wxOK | wxICON_ERROR);
  exit(1);
}

mmg_dialog::mmg_dialog():
  wxFrame(NULL, wxID_ANY, wxEmptyString) {
  wxBoxSizer *bs_main;
  wxPanel *panel;

  mdlg = this;

#if defined(__WXGTK__)
  // GTK seems to call bindtextdomain() after our call to it.
  // So lets re-initialize the UI locale in case the user has
  // selected a language in mmg's preferences that doesn't match
  // his environment variable's language.
  app->init_ui_locale();
#endif

  SetTitle(wxString::Format(Z("mkvmerge GUI v%s ('%s')"), wxU(VERSION).c_str(), wxU(VERSIONNAME).c_str()));

  log_window = new wxLogWindow(this, Z("mmg debug output"), false);
  wxLog::SetActiveTarget(log_window);

  file_menu = new wxMenu();
  file_menu->Append(ID_M_FILE_NEW, Z("&New\tCtrl-N"), Z("Start with empty settings"));
  file_menu->Append(ID_M_FILE_LOAD, Z("&Load settings\tCtrl-L"), Z("Load muxing settings from a file"));
  file_menu->Append(ID_M_FILE_SAVE, Z("&Save settings\tCtrl-S"), Z("Save muxing settings to a file"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_SETOUTPUT, Z("Set &output file"), Z("Select the file you want to write to"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_OPTIONS, Z("Op&tions\tCtrl-P"), Z("Change mmg's preferences and options"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_HEADEREDITOR, Z("&Header editor\tCtrl-E"), Z("Run the header field editor"));
  file_menu->AppendSeparator();
  file_menu->Append(ID_M_FILE_EXIT, Z("&Quit\tCtrl-Q"), Z("Quit the application"));

  file_menu_sep = false;
  update_file_menu();

  wxMenu *muxing_menu = new wxMenu();
  muxing_menu->Append(ID_M_MUXING_START, Z("Sta&rt muxing (run mkvmerge)\tCtrl-R"), Z("Run mkvmerge and start the muxing process"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_SHOW_CMDLINE, Z("S&how the command line"), Z("Show the command line mmg creates for mkvmerge"));
  muxing_menu->Append(ID_M_MUXING_COPY_CMDLINE, Z("&Copy command line to clipboard"), Z("Copy the command line to the clipboard"));
  muxing_menu->Append(ID_M_MUXING_SAVE_CMDLINE, Z("Sa&ve command line"), Z("Save the command line to a file"));
  muxing_menu->Append(ID_M_MUXING_CREATE_OPTIONFILE, Z("Create &option file"), Z("Save the command line to an option file that can be read by mkvmerge"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_ADD_TO_JOBQUEUE, Z("&Add to job queue"), Z("Adds the current settings as a new job entry to the job queue"));
  muxing_menu->Append(ID_M_MUXING_MANAGE_JOBS, Z("&Manage jobs\tCtrl-J"), Z("Brings up the job queue editor"));
  muxing_menu->AppendSeparator();
  muxing_menu->Append(ID_M_MUXING_ADD_CLI_OPTIONS, Z("Add &command line options"), Z("Lets you add arbitrary options to the command line"));

  chapter_menu = new wxMenu();
  chapter_menu->Append(ID_M_CHAPTERS_NEW, Z("&New chapters"), Z("Create a new chapter file"));
  chapter_menu->Append(ID_M_CHAPTERS_LOAD, Z("&Load"), Z("Load a chapter file (simple/OGM format or XML "
                           "format)"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVE, Z("&Save"), Z("Save the current chapters to a XML file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVETOKAX, Z("Save to &Matroska file"), Z("Save the current chapters to an existing Matroska file"));
  chapter_menu->Append(ID_M_CHAPTERS_SAVEAS, Z("Save &as"), Z("Save the current chapters to a file with another name"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_VERIFY, Z("&Verify"), Z("Verify the current chapter entries to see if there are any errors"));
  chapter_menu->AppendSeparator();
  chapter_menu->Append(ID_M_CHAPTERS_SETDEFAULTS, Z("Set &default values"));
  chapter_menu_sep = false;
  update_chapter_menu();

  wxMenu *window_menu = new wxMenu();
  window_menu->Append(ID_M_WINDOW_INPUT, Z("&Input\tAlt-1"));
  window_menu->Append(ID_M_WINDOW_ATTACHMENTS, Z("&Attachments\tAlt-2"));
  window_menu->Append(ID_M_WINDOW_GLOBAL, Z("&Global options\tAlt-3"));
  window_menu->AppendSeparator();
  window_menu->Append(ID_M_WINDOW_CHAPTEREDITOR, Z("&Chapter editor\tAlt-4"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HELP_HELP, Z("&Help\tF1"), Z("Show the guide to mkvmerge GUI"));
  help_menu->Append(ID_M_HELP_ABOUT, Z("&About"), Z("Show program information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(file_menu, Z("&File"));
  menu_bar->Append(muxing_menu, Z("&Muxing"));
  menu_bar->Append(chapter_menu, Z("&Chapter Editor"));
  menu_bar->Append(window_menu, Z("&Window"));
  menu_bar->Append(help_menu, Z("&Help"));
  SetMenuBar(menu_bar);

  status_bar = new wxStatusBar(this, -1);
  SetStatusBar(status_bar);
  status_bar_timer.SetOwner(this, ID_T_STATUSBAR);

  panel = new wxPanel(this, -1);

  bs_main = new wxBoxSizer(wxVERTICAL);
  panel->SetSizer(bs_main);
  panel->SetAutoLayout(true);

  notebook =
    new wxNotebook(panel, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  input_page = new tab_input(notebook);
  attachments_page = new tab_attachments(notebook);
  global_page = new tab_global(notebook);
  chapter_editor_page = new tab_chapters(notebook, chapter_menu);

  notebook->AddPage(input_page, Z("Input"));
  notebook->AddPage(attachments_page, Z("Attachments"));
  notebook->AddPage(global_page, Z("Global"));
  notebook->AddPage(chapter_editor_page, Z("Chapter Editor"));

  bs_main->Add(notebook, 1, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxGROW, 5);

  wxStaticBox *sb_low = new wxStaticBox(panel, -1, Z("Output filename"));
  wxStaticBoxSizer *sbs_low = new wxStaticBoxSizer(sb_low, wxHORIZONTAL);
  bs_main->Add(sbs_low, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxGROW, 5);

  tc_output = new wxTextCtrl(panel, ID_TC_OUTPUT, wxEmptyString);
  sbs_low->AddSpacer(5);
  sbs_low->Add(tc_output, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxGROW, 2);
  sbs_low->AddSpacer(5);

  b_browse_output = new wxButton(panel, ID_B_BROWSEOUTPUT, Z("Browse"));
  sbs_low->Add(b_browse_output, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);

  wxBoxSizer *bs_buttons = new wxBoxSizer(wxHORIZONTAL);

  b_start_muxing = new wxButton(panel, ID_B_STARTMUXING, Z("Sta&rt muxing"));
  bs_buttons->Add(b_start_muxing, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  b_copy_to_clipboard = new wxButton(panel, ID_B_COPYTOCLIPBOARD, Z("&Copy to clipboard"));
  bs_buttons->Add(b_copy_to_clipboard, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  b_add_to_jobqueue = new wxButton(panel, ID_B_ADD_TO_JOBQUEUE, Z("&Add to job queue"));
  bs_buttons->Add(b_add_to_jobqueue, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);

  bs_main->Add(bs_buttons, 0, wxALIGN_CENTER_HORIZONTAL);

#ifdef SYS_WINDOWS
  SetSizeHints(700, 680);
  SetSize(700, 680);
#else
  SetSizeHints(700, 660);
  SetSize(700, 660);
#endif

  load_preferences();

  log_window->Show(options.gui_debugging);
  set_on_top(options.on_top);
  query_mkvmerge_capabilities();

  muxing_in_progress = false;
  last_open_dir      = wxEmptyString;
  cmdline            = wxString::Format(wxU("\"%s\" -o \"%s\""), options.mkvmerge.c_str(), tc_output->GetValue().c_str());

  load_job_queue();

  SetIcon(wxIcon(matroskalogo_xpm));

  help = NULL;

  set_status_bar(Z("mkvmerge GUI ready"));
}

mmg_dialog::~mmg_dialog() {
  delete help;
}

void
mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, Z("Choose an output file"), last_open_dir,
                   tc_output->GetValue().AfterLast(PSEP),
                   wxString::Format(Z("Matroska A/V files (*.mka;*.mkv)|*.mkv;*.mka|%s"), ALLFILES.c_str()),
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tc_output->SetValue(dlg.GetPath());
  remember_previous_output_directory();
}

void
mmg_dialog::set_status_bar(wxString text) {
  status_bar_timer.Stop();
  status_bar->SetStatusText(text);
  status_bar_timer.Start(4000, true);
}

void
mmg_dialog::on_clear_status_bar(wxTimerEvent &evt) {
  status_bar->SetStatusText(wxEmptyString);
}

void
mmg_dialog::on_quit(wxCommandEvent &evt) {
  Close(true);
}

void
mmg_dialog::on_file_new(wxCommandEvent &evt) {
  wxString tmp_name;

  tmp_name.Printf(wxT("%stempsettings-%d.mmg"), get_temp_dir().c_str(), (int)wxGetProcessId());
  wxFileConfig cfg(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), tmp_name);
  tc_output->SetValue(wxEmptyString);

  input_page->load(&cfg, MMG_CONFIG_FILE_VERSION_MAX);
  input_page->on_file_new(evt);
  attachments_page->load(&cfg, MMG_CONFIG_FILE_VERSION_MAX);
  global_page->load(&cfg, MMG_CONFIG_FILE_VERSION_MAX);
  notebook->SetSelection(0);

  wxRemoveFile(tmp_name);

  set_status_bar(Z("Configuration cleared."));
}

void
mmg_dialog::on_file_load(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, Z("Choose an input file"), last_open_dir, wxEmptyString, wxString::Format(Z("mkvmerge GUI settings (*.mmg)|*.mmg|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  if (!wxFileExists(dlg.GetPath()) || wxDirExists(dlg.GetPath())) {
    wxMessageBox(Z("The file does not exist."), Z("Error loading settings"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  last_open_dir = dlg.GetDirectory();
  load(dlg.GetPath());
}

void
mmg_dialog::load(wxString file_name,
                 bool used_for_jobs) {
  wxString s;
  int version;

  wxFileConfig cfg(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), file_name);
  cfg.SetPath(wxT("/mkvmergeGUI"));
  if (!cfg.Read(wxT("file_version"), &version) || (1 > version) || (MMG_CONFIG_FILE_VERSION_MAX < version)) {
    if (used_for_jobs)
      return;
    wxMessageBox(Z("The file does not seem to be a valid mkvmerge GUI settings file."), Z("Error loading settings"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  cfg.Read(wxT("output_file_name"), &s);
  tc_output->SetValue(s);
  cfg.Read(wxT("cli_options"), &cli_options, wxEmptyString);

  input_page->load(&cfg, version);
  attachments_page->load(&cfg, version);
  global_page->load(&cfg, version);

  if (!used_for_jobs) {
    set_last_settings_in_menu(file_name);
    set_status_bar(Z("Configuration loaded."));
  }
}

void
mmg_dialog::on_file_save(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, Z("Choose an output file"), last_open_dir, wxEmptyString, wxString::Format(Z("mkvmerge GUI settings (*.mmg)|*.mmg|%s"), ALLFILES.c_str()), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  if (wxFileExists(dlg.GetPath()))
    wxRemoveFile(dlg.GetPath());
  save(dlg.GetPath());
}

void
mmg_dialog::save(wxString file_name,
                 bool used_for_jobs) {
  wxFileConfig *cfg;

  cfg = new wxFileConfig(wxT("mkvmerge GUI"), wxT("Moritz Bunkus"), file_name);
  cfg->SetPath(wxT("/mkvmergeGUI"));
  cfg->Write(wxT("file_version"), MMG_CONFIG_FILE_VERSION_MAX);
  cfg->Write(wxT("gui_version"), wxT(VERSION));
  cfg->Write(wxT("output_file_name"), tc_output->GetValue());
  cfg->Write(wxT("cli_options"), cli_options);

  input_page->save(cfg);
  attachments_page->save(cfg);
  global_page->save(cfg);

  delete cfg;

  if (!used_for_jobs) {
    set_status_bar(Z("Configuration saved."));
    set_last_settings_in_menu(file_name);
  }
}

void
mmg_dialog::set_last_settings_in_menu(wxString name) {
  uint32_t i;
  wxConfigBase *cfg;
  wxString s;

  i = 0;
  while (i < last_settings.size()) {
    if (last_settings[i] == name)
      last_settings.erase(last_settings.begin() + i);
    else
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
  wxConfigBase *cfg;
  wxString s;

  i = 0;
  while (i < last_chapters.size()) {
    if (last_chapters[i] == name)
      last_chapters.erase(last_chapters.begin() + i);
    else
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

bool
mmg_dialog::check_before_overwriting() {
  wxFileName file_name(tc_output->GetValue());
  wxString dir, name, ext;
  wxArrayString files_in_output_dir;
  int i;

  dir = file_name.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);

  if (!global_page->cb_split->GetValue()) {
    if (wxFile::Exists(tc_output->GetValue()) &&
        (wxMessageBox(wxString::Format(Z("The output file '%s' already exists. Do you want to overwrite it?"), tc_output->GetValue().c_str()), Z("Overwrite existing file?"), wxYES_NO) != wxYES))
      return false;
    return true;
  }

  name = file_name.GetName() + wxT("-");
  ext = file_name.GetExt();

  wxDir::GetAllFiles(dir, &files_in_output_dir, wxEmptyString, wxDIR_FILES);
  for (i = 0; i < files_in_output_dir.Count(); i++) {
    wxFileName test_name(files_in_output_dir[i]);

    if (test_name.GetName().StartsWith(name) &&
        (test_name.HasExt() == file_name.HasExt()) &&
        (test_name.GetExt() == ext)) {
      if (file_name.HasExt())
        ext = wxU(".") + ext;
      if (wxMessageBox(wxString::Format(Z("Splitting is active, and at least one of the potential output files '%s%s*%s' already exists. Do you want to overwrite them?"),
                                        dir.c_str(), name.c_str(), ext.c_str()),
                       Z("Overwrite existing file(s)?"), wxYES_NO) != wxYES)
        return false;
      return true;
    }
  }

  return true;
}

void
mmg_dialog::on_run(wxCommandEvent &evt) {
  if (muxing_in_progress) {
    wxMessageBox(Z("Another muxing job in still in progress. Please wait until it has finished or abort it manually before starting a new one."),
                 Z("Cannot start second muxing job"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  update_command_line();

  if (tc_output->GetValue().Length() == 0) {
    wxMessageBox(Z("You have not yet selected an output file."), Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings())
    return;

  if (!chapter_editor_page->is_empty() &&
      (global_page->tc_chapters->GetValue() == wxEmptyString) &&
      (!warned_chapter_editor_not_empty || options.warn_usage)) {
    warned_chapter_editor_not_empty = true;
    if (wxMessageBox(Z("The chapter editor has been used and contains data. "
                       "However, no chapter file has been selected on the global page. "
                       "In mmg, the chapter editor is independant of the muxing process. "
                       "The chapters present in the editor will NOT be muxed into the output file. "
                       "Only the various 'save' functions from the chapter editor menu will cause the chapters to be written to the hard disk.\n\n"
                       "Do you really want to continue muxing?\n\n"
                       "Note: This warning can be deactivated on the 'settings' page. "
                       "Turn off the 'Warn about usage...' option."),
                     Z("Chapter editor is not empty"),
                     wxYES_NO | wxICON_QUESTION) != wxYES)
      return;
  }

  if (options.ask_before_overwriting && !check_before_overwriting())
    return;

  remember_previous_output_directory();

  set_on_top(false);
  muxing_in_progress = true;
  new mux_dialog(this);
}

void
mmg_dialog::muxing_has_finished() {
  muxing_in_progress = false;
  restore_on_top();
}

void
mmg_dialog::on_help(wxCommandEvent &evt) {
  display_help(notebook->GetCurrentPage() == chapter_editor_page ? HELP_ID_CHAPTER_EDITOR : HELP_ID_CONTENTS);
}

void
mmg_dialog::display_help(int id) {
  if (help == NULL) {
    wxDirDialog dlg(this, Z("Choose the location of the mkvmerge GUI help files"));
    vector<wxString> potential_help_paths;
    vector<wxString>::const_iterator php;
    wxString help_path;
    wxConfigBase *cfg;
    bool first;

#if defined(SYS_WINDOWS)
    cfg = new wxConfig(wxT("mkvmergeGUI"), wxEmptyString, wxEmptyString, wxEmptyString, wxCONFIG_USE_GLOBAL_FILE);
    cfg->SetPath(wxT("/GUI"));

    if (cfg->Read(wxT("installation_path"), &help_path)) {
      help_path += wxT("/doc");
      potential_help_paths.push_back(help_path);
    }

    delete cfg;

#else
    // Debian, probably others
    potential_help_paths.push_back(wxT("/usr/share/doc/mkvtoolnix"));
    potential_help_paths.push_back(wxT("/usr/share/doc/mkvtoolnix/doc"));
    potential_help_paths.push_back(wxT("/usr/share/doc/mkvtoolnix-gui"));
    // SuSE
    potential_help_paths.push_back(wxT("/usr/share/doc/packages/mkvtoolnix"));
    // Fedora Core
    potential_help_paths.push_back(wxT("/usr/share/doc/mkvtoolnix-" VERSION));
    potential_help_paths.push_back(wxT("/usr/share/doc/mkvtoolnix-gui-" VERSION));
    // (Almost the) same for /usr/local
    potential_help_paths.push_back(wxT("/usr/local/share/doc/mkvtoolnix"));
    potential_help_paths.push_back(wxT("/usr/local/share/doc/packages/mkvtoolnix"));
    potential_help_paths.push_back(wxT("/usr/local/share/doc/mkvtoolnix-" VERSION));
    potential_help_paths.push_back(wxT("/usr/local/share/doc/mkvtoolnix-gui-" VERSION));
    // New location
    potential_help_paths.push_back(wxT(MTX_PKG_DATA_DIR));
    potential_help_paths.push_back(wxT(MTX_PKG_DATA_DIR "-" VERSION));
#endif

    cfg = wxConfigBase::Get();
    cfg->SetPath(wxT("/GUI"));
    if (cfg->Read(wxT("help_path"), &help_path))
      potential_help_paths.push_back(help_path);

    potential_help_paths.push_back(wxGetCwd() + wxT("/doc"));
    potential_help_paths.push_back(wxGetCwd());

    help_path = wxEmptyString;
    mxforeach(php, potential_help_paths)
      if (wxFileExists(*php + wxT("/mkvmerge-gui.hhp"))) {
        help_path = *php;
        break;
      }

    first = true;
    while (!wxFileExists(help_path + wxT("/mkvmerge-gui.hhp"))) {
      if (first) {
        wxMessageBox(Z("The mkvmerge GUI help file was not found. "
                       "This indicates that it has never before been opened, or that the installation path has since been changed.\n\n"
                       "Please select the location of the 'mkvmerge-gui.hhp' file."),
                     Z("Help file not found"),
                     wxOK | wxICON_INFORMATION);
        first = false;
      } else
        wxMessageBox(Z("The mkvmerge GUI help file was not found in the path you've selected. "
                       "Please try again, or abort by pressing the 'abort' button."),
                     Z("Help file not found"),
                     wxOK | wxICON_INFORMATION);

      dlg.SetPath(help_path);
      if (dlg.ShowModal() == wxID_CANCEL)
        return;
      help_path = dlg.GetPath();
      cfg->Write(wxT("help_path"), help_path);
    }
    help = new wxHtmlHelpController;
    help->AddBook(wxFileName(help_path + wxT("/mkvmerge-gui.hhp")), false);
  }

  help->Display(id);
}

void
mmg_dialog::on_about(wxCommandEvent &evt) {
  wxMessageBox(wxString::Format(Z("mkvmerge GUI v%s ('%s')\n"
                                  "built on %s %s\n\n"
                                  "This GUI was written by Moritz Bunkus <moritz@bunkus.org>"
                                  "\nBased on mmg by Florian Wagner <flo.wagner@gmx.de>\n"
                                  "mkvmerge GUI is licensed under the GPL.\n"
                                  "http://www.bunkus.org/videotools/mkvtoolnix/\n"
                                  "\n"
                                  "Help is available in form of tool tips, from the\n"
                                  "'Help' menu or by pressing the 'F1' key."),
                                wxU(VERSION).c_str(), wxU(VERSIONNAME).c_str(), wxU(__DATE__).c_str(), wxU(__TIME__).c_str()),
               Z("About mkvmerge's GUI"),
               wxOK | wxCENTER | wxICON_INFORMATION);
}

void
mmg_dialog::on_show_cmdline(wxCommandEvent &evt) {
  update_command_line();

  show_text_dlg dlg(this, Z("Current command line"), cmdline);
  dlg.ShowModal();
}

void
mmg_dialog::on_save_cmdline(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, Z("Choose an output file"), last_open_dir, wxEmptyString, ALLFILES, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    update_command_line();

    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = cmdline + wxT("\n");
    file->Write(s);
    delete file;

    set_status_bar(Z("Command line saved."));
  }
}

void
mmg_dialog::on_create_optionfile(wxCommandEvent &evt) {
  const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
  uint32_t i;
  string arg_utf8;
  wxFile *file;

  wxFileDialog dlg(NULL, Z("Choose an output file"), last_open_dir, wxEmptyString, ALLFILES, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    update_command_line();

    last_open_dir = dlg.GetDirectory();
    try {
      file = new wxFile(dlg.GetPath(), wxFile::write);
      file->Write(utf8_bom, 3);
    } catch (...) {
      wxMessageBox(Z("Could not create the specified file."), Z("File creation failed"), wxOK | wxCENTER | wxICON_ERROR);
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

    set_status_bar(Z("Option file created."));
  }
}

void
mmg_dialog::on_copy_to_clipboard(wxCommandEvent &evt) {
  update_command_line();
  if (wxTheClipboard->Open()) {
    wxTheClipboard->SetData(new wxTextDataObject(cmdline));
    wxTheClipboard->Close();
    set_status_bar(Z("Command line copied to clipboard."));
  } else
    set_status_bar(Z("Could not open the clipboard."));
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
  unsigned int i;

  wxString old_cmdline = cmdline;
  cmdline              = wxT("\"") + options.mkvmerge + wxT("\" -o \"") + tc_output->GetValue() + wxT("\" ");

  clargs.Clear();
  clargs.Add(options.mkvmerge);
  clargs.Add(wxT("--output-charset"));
  clargs.Add(wxT("UTF-8"));
  clargs.Add(wxT("-o"));
  clargs.Add(tc_output->GetValue());

#if defined(HAVE_LIBINTL_H)
  if (!app->m_ui_locale.empty()) {
    clargs.Add(wxT("--ui-language"));
    clargs.Add(wxCS2WS(app->m_ui_locale));
  }
#endif  // HAVE_LIBINTL_H

  unsigned int args_start = clargs.Count();

  if (options.priority != wxT("normal")) {
    clargs.Add(wxT("--priority"));
    clargs.Add(options.priority);
  }

  unsigned int fidx;
  for (fidx = 0; files.size() > fidx; fidx++) {
    mmg_file_cptr &f          = files[fidx];
    bool tracks_selected_here = false;
    bool no_audio             = true;
    bool no_video             = true;
    bool no_subs              = true;
    wxString aids             = wxEmptyString;
    wxString sids             = wxEmptyString;
    wxString dids             = wxEmptyString;

    unsigned int tidx;
    for (tidx = 0; f->tracks.size() > tidx; tidx++) {
      mmg_track_cptr &t = f->tracks[tidx];
      if (!t->enabled)
        continue;

      tracks_selected_here = true;
      wxString sid         = wxLongLong(t->id).ToString();

      if (t->type == wxT('a')) {
        no_audio = false;
        if (aids.length() > 0)
          aids += wxT(",");
        aids += sid;

        if (!t->appending && (t->aac_is_sbr || t->aac_is_sbr_detected)) {
          clargs.Add(wxT("--aac-is-sbr"));
          clargs.Add(wxString::Format(wxT("%s:%d"), sid.c_str(), t->aac_is_sbr ? 1 : 0));
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

        if ((t->sub_charset.Length() > 0) && (t->sub_charset != wxT("default"))) {
          clargs.Add(wxT("--sub-charset"));
          clargs.Add(sid + wxT(":") + shell_escape(t->sub_charset));
        }
      }

      if (!t->appending && (t->language != wxT("und"))) {
        clargs.Add(wxT("--language"));
        clargs.Add(sid + wxT(":") + extract_language_code(t->language));
      }

      if (!t->appending && (t->cues != wxT("default"))) {
        clargs.Add(wxT("--cues"));
        if (t->cues == wxT("only for I frames"))
          clargs.Add(sid + wxT(":iframes"));
        else if (t->cues == wxT("for all frames"))
          clargs.Add(sid + wxT(":all"));
        else if (t->cues == wxT("none"))
          clargs.Add(sid + wxT(":none"));
      }

      if ((t->delay.Length() > 0) || (t->stretch.Length() > 0)) {
        wxString arg = sid + wxT(":");
        if (t->delay.Length() > 0)
          arg += t->delay;
        else
          arg += wxT("0");
        if (t->stretch.Length() > 0) {
          arg += wxT(",") + t->stretch;
          if (t->stretch.Find(wxT("/")) < 0)
            arg += wxT("/1");
        }
        clargs.Add(wxT("--sync"));
        clargs.Add(arg);
      }

      if (!t->appending && ((t->track_name.Length() > 0) || t->track_name_was_present)) {
        clargs.Add(wxT("--track-name"));
        clargs.Add(sid + wxT(":") + t->track_name);
      }

      if (!t->appending && (0 != t->default_track)) {
        clargs.Add(wxT("--default-track"));
        clargs.Add(sid + wxT(":") + (1 == t->default_track ? wxT("yes") : wxT("no")));
      }

      if (!t->appending && (t->tags.Length() > 0)) {
        clargs.Add(wxT("--tags"));
        clargs.Add(sid + wxT(":") + t->tags);
      }

      if (!t->appending && !t->display_dimensions_selected && (t->aspect_ratio.Length() > 0)) {
        clargs.Add(wxT("--aspect-ratio"));
        clargs.Add(sid + wxT(":") + t->aspect_ratio);

      } else if (!t->appending && t->display_dimensions_selected && (t->dwidth.Length() > 0) && (t->dheight.Length() > 0)) {
        clargs.Add(wxT("--display-dimensions"));
        clargs.Add(sid + wxT(":") + t->dwidth + wxT("x") + t->dheight);
      }

      if (!t->appending && (t->fourcc.Length() > 0)) {
        clargs.Add(wxT("--fourcc"));
        clargs.Add(sid + wxT(":") + t->fourcc);
      }

      if (t->fps.Length() > 0) {
        clargs.Add(wxT("--default-duration"));
        clargs.Add(sid + wxT(":") + t->fps + wxT("fps"));
      }

      if (0 != t->nalu_size_length) {
        clargs.Add(wxT("--nalu-size-length"));
        clargs.Add(wxString::Format(wxT("%s:%d"), sid.c_str(), t->nalu_size_length));
      }

      if (!t->appending && (0 != t->stereo_mode)) {
        clargs.Add(wxT("--stereo-mode"));
        clargs.Add(wxString::Format(wxT("%s:%d"), sid.c_str(), t->stereo_mode - 1));
      }

      if (!t->appending && (t->compression.Length() > 0)) {
        wxString compression = t->compression;
        if (compression == wxT("none"))
          compression = wxT("none");
        clargs.Add(wxT("--compression"));
        clargs.Add(sid + wxT(":") + compression);
      }

      if (!t->appending && (t->timecodes.Length() > 0)) {
        clargs.Add(wxT("--timecodes"));
        clargs.Add(sid + wxT(":") + t->timecodes);
      }

      if (t->user_defined.Length() > 0) {
        vector<wxString> opts = split(t->user_defined, wxString(wxT(" ")));
        for (i = 0; opts.size() > i; i++) {
          wxString opt = strip(opts[i]);
          opt.Replace(wxT("<TID>"), sid, true);
          clargs.Add(opt);
        }
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
      else {
        std::vector<mmg_attached_file_cptr>::iterator att_file = f->attached_files.begin();
        std::vector<wxString> att_file_ids;

        while (att_file != f->attached_files.end()) {
          if ((*att_file)->enabled)
            att_file_ids.push_back(wxString::Format(wxT("%ld"), (*att_file)->id));
          ++att_file;
        }

        if (!att_file_ids.empty()) {
          clargs.Add(wxT("--attachments"));
          clargs.Add(join(wxT(","), att_file_ids));

        } else if (!f->attached_files.empty())
          clargs.Add(wxT("--no-attachments"));
      }

      if (f->no_tags)
        clargs.Add(wxT("--no-tags"));

      if (no_video)
        clargs.Add(wxT("-D"));
      if (no_audio)
        clargs.Add(wxT("-A"));
      if (no_subs)
        clargs.Add(wxT("-S"));

      if (f->appending)
        clargs.Add(wxString(wxT("+")) + f->file_name);
      else
        clargs.Add(f->file_name);
    }
  }

  wxString track_order = create_track_order(false);
  if (track_order.length() > 0) {
    clargs.Add(wxT("--track-order"));
    clargs.Add(track_order);
  }

  wxString append_mapping = create_append_mapping();
  if (append_mapping.length() > 0) {
    clargs.Add(wxT("--append-to"));
    clargs.Add(append_mapping);
  }

  for (fidx = 0; attachments.size() > fidx; fidx++) {
    mmg_attachment_cptr &a = attachments[fidx];

    clargs.Add(wxT("--attachment-mime-type"));
    clargs.Add(a->mime_type);
    if (a->description.Length() > 0) {
      clargs.Add(wxT("--attachment-description"));
      clargs.Add(no_cr(a->description));
    }
    clargs.Add(wxT("--attachment-name"));
    clargs.Add(a->stored_name);
    if (a->style == 0)
      clargs.Add(wxT("--attach-file"));
    else
      clargs.Add(wxT("--attach-file-once"));
    clargs.Add(a->file_name);
  }

  if (title_was_present || (global_page->tc_title->GetValue().Length() > 0)) {
    clargs.Add(wxT("--title"));
    clargs.Add(global_page->tc_title->GetValue());
  }

  if (global_page->cb_split->IsChecked()) {
    clargs.Add(wxT("--split"));
    if (global_page->rb_split_by_size->GetValue())
      clargs.Add(wxT("size:") + global_page->cob_split_by_size->GetValue());
    else if (global_page->rb_split_by_time->GetValue())
      clargs.Add(wxT("duration:") + global_page->cob_split_by_time->GetValue());
    else
      clargs.Add(wxT("timecodes:") + global_page->tc_split_after_timecodes->GetValue());

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
      clargs.Add(extract_language_code(global_page->cob_chap_language->GetValue()));
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
    vector<wxString> opts = split(cli_options, wxString(wxT(" ")));
    for (i = 0; i < opts.size(); i++)
      clargs.Add(strip(opts[i]));
  }

  if (options.always_use_simpleblock) {
    clargs.Add(wxT("--engage"));
    clargs.Add(wxT("use_simpleblock"));
  }

  for (i = args_start; i < clargs.Count(); i++) {
    if (clargs[i].Find(wxT(" ")) >= 0)
      cmdline += wxT(" \"") + shell_escape(clargs[i]) + wxT("\"");
    else
      cmdline += wxT(" ") + shell_escape(clargs[i]);
  }
}

void
mmg_dialog::on_file_load_last(wxCommandEvent &evt) {
  if ((evt.GetId() < ID_M_FILE_LOADLAST1) || ((evt.GetId() - ID_M_FILE_LOADLAST1) >= last_settings.size()))
    return;

  load(last_settings[evt.GetId() - ID_M_FILE_LOADLAST1]);
}

void
mmg_dialog::on_chapters_load_last(wxCommandEvent &evt) {
  if ((evt.GetId() < ID_M_CHAPTERS_LOADLAST1) || ((evt.GetId() - ID_M_CHAPTERS_LOADLAST1) >= last_chapters.size()))
    return;

  notebook->SetSelection(4);
  chapter_editor_page->load(last_chapters[evt.GetId() - ID_M_CHAPTERS_LOADLAST1]);
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
  if (!options.autoset_output_filename || new_output.empty() || !tc_output->GetValue().empty())
    return;

  bool has_video = false, has_audio = false;
  vector<mmg_track_t *>::iterator t;

  mxforeach(t, tracks) {
    if ('v' == (*t)->type) {
      has_video = true;
      break;
    } else if ('a' == (*t)->type)
      has_audio = true;
  }

  wxString output;
  wxFileName filename(new_output);

  if (ODM_PREVIOUS == options.output_directory_mode)
    output = previous_output_directory;
  else if (ODM_FIXED == options.output_directory_mode)
    output = options.output_directory;

  if (output.IsEmpty())
    output = filename.GetPath();

  output += wxFileName::GetPathSeparator() + filename.GetName() + (has_video ? wxU(".mkv") : has_audio ? wxU(".mka") : wxU(".mks"));

  tc_output->SetValue(output);
}

void
mmg_dialog::remove_output_filename() {
  if (options.autoset_output_filename)
    tc_output->SetValue(wxEmptyString);
}

void
mmg_dialog::on_add_to_jobqueue(wxCommandEvent &evt) {
  wxString description, line;
  job_t job;
  int i, result;
  bool ok;

  if (tc_output->GetValue().Length() == 0) {
    wxMessageBox(Z("You have not yet selected an output file."), Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  if (!input_page->validate_settings() ||
      !attachments_page->validate_settings() ||
      !global_page->validate_settings())
    return;

  line.Printf(Z("The output file '%s' already exists. Do you want to overwrite it?"), tc_output->GetValue().c_str());
  if (   options.ask_before_overwriting
      && wxFile::Exists(tc_output->GetValue())
      && (wxMessageBox(break_line(line, 60), Z("Overwrite existing file?"), wxYES_NO) != wxYES))
    return;

  description = tc_output->GetValue().AfterLast(wxT('/')).AfterLast(wxT('\\')).AfterLast(wxT('/')).BeforeLast(wxT('.'));
  ok = false;
  do {
    description = wxGetTextFromUser(Z("Please enter a description for the new job:"), Z("Job description"), description);
    if (description.length() == 0)
      return;

    if (!options.ask_before_overwriting)
      break;
    break_line(line, 60);
    ok = true;
    for (i = 0; i < jobs.size(); i++)
      if (description == *jobs[i].description) {
        ok = false;
        line.Printf(Z("A job with the description '%s' already exists. Do you really want to add another one with the same description?"), description.c_str());
        result = wxMessageBox(line, Z("Description already exists"), wxYES_NO | wxCANCEL);
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
  job.status = JOBS_PENDING;
  job.added_on = wxGetUTCTime();
  job.started_on = -1;
  job.finished_on = -1;
  job.description = new wxString(description);
  job.log = new wxString();
  jobs.push_back(job);

  description.Printf(wxT("/jobs/%d.mmg"), job.id);
  save(wxGetCwd() + description);

  save_job_queue();

  remember_previous_output_directory();

  if (options.filenew_after_add_to_jobqueue) {
    wxCommandEvent dummy;
    on_file_new(dummy);
  }

  set_status_bar(Z("Job added to job queue"));
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

  for (i = 0; i < jobs.size(); i++) {
    delete jobs[i].description;
    delete jobs[i].log;
  }
  jobs.clear();

  for (i = 0; i < num; i++) {
    cfg->SetPath(wxT("/jobs"));
    s.Printf(wxT("%u"), i);
    if (!cfg->HasGroup(s))
      continue;
    cfg->SetPath(s);
    cfg->Read(wxT("id"), &value);
    job.id = value;
    cfg->Read(wxT("status"), &s);
    job.status =
      s == wxT("pending") ? JOBS_PENDING :
      s == wxT("done") ? JOBS_DONE :
      s == wxT("done_warnings") ? JOBS_DONE_WARNINGS :
      s == wxT("aborted") ? JOBS_ABORTED :
      JOBS_FAILED;
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
               jobs[i].status == JOBS_PENDING ? wxT("pending") :
               jobs[i].status == JOBS_DONE ? wxT("done") :
               jobs[i].status == JOBS_DONE_WARNINGS ? wxT("done_warnings") :
               jobs[i].status == JOBS_ABORTED ? wxT("aborted") :
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
mmg_dialog::save_preferences() {
  wxConfigBase *cfg = (wxConfig *)wxConfigBase::Get();
  int x, y;

  cfg->SetPath(wxU("/GUI"));

  GetPosition(&x, &y);
  cfg->Write(wxU("window_position_x"), x);
  cfg->Write(wxU("window_position_y"), y);
  cfg->Write(wxU("warned_chapter_editor_not_empty"), warned_chapter_editor_not_empty);
  cfg->Write(wxU("previous_output_directory"), previous_output_directory);

  cfg->Write(wxU("mkvmerge_executable"), options.mkvmerge);
  cfg->Write(wxU("process_priority"), options.priority);
  cfg->Write(wxU("autoset_output_filename"), options.autoset_output_filename);
  cfg->Write(wxU("output_directory_mode"), (long)options.output_directory_mode);
  cfg->Write(wxU("output_directory"), options.output_directory);
  cfg->Write(wxU("ask_before_overwriting"), options.ask_before_overwriting);
  cfg->Write(wxU("filenew_after_add_to_jobqueue"), options.filenew_after_add_to_jobqueue);
  cfg->Write(wxU("on_top"), options.on_top);
  cfg->Write(wxU("warn_usage"), options.warn_usage);
  cfg->Write(wxU("gui_debugging"), options.gui_debugging);
  cfg->Write(wxU("always_use_simpleblock"), options.always_use_simpleblock);
  cfg->Write(wxU("set_delay_from_filename"), options.set_delay_from_filename);

  cfg->Flush();
}

void
mmg_dialog::load_preferences() {
  wxConfigBase *cfg = wxConfigBase::Get();
  wxString s;
  int window_pos_x, window_pos_y;

  cfg->SetPath(wxT("/GUI"));

  if (cfg->Read(wxT("window_position_x"), &window_pos_x) &&
      cfg->Read(wxT("window_position_y"), &window_pos_y) &&
      (0 < window_pos_x) && (0xffff > window_pos_x) &&
      (0 < window_pos_y) && (0xffff > window_pos_y))
    Move(window_pos_x, window_pos_y);

  cfg->Read(wxU("warned_chapterditor_not_empty"), &warned_chapter_editor_not_empty, false);
  cfg->Read(wxU("previous_output_directory"), &previous_output_directory, wxU(""));

  cfg->Read(wxU("mkvmerge_executable"), &options.mkvmerge, wxU("mkvmerge"));
  cfg->Read(wxU("process_priority"), &options.priority, wxU("normal"));
  cfg->Read(wxU("autoset_output_filename"), &options.autoset_output_filename, true);
  cfg->Read(wxU("output_directory_mode"), (long *)&options.output_directory_mode, ODM_FROM_FIRST_INPUT_FILE);
  cfg->Read(wxU("output_directory"), &options.output_directory, wxU(""));
  cfg->Read(wxU("ask_before_overwriting"), &options.ask_before_overwriting, true);
  cfg->Read(wxU("filenew_after_add_to_jobqueue"), &options.filenew_after_add_to_jobqueue, false);
  cfg->Read(wxU("on_top"), &options.on_top, false);
  cfg->Read(wxU("warn_usage"), &options.warn_usage, true);
  cfg->Read(wxU("gui_debugging"), &options.gui_debugging, false);
  cfg->Read(wxU("always_use_simpleblock"), &options.always_use_simpleblock, false);
  cfg->Read(wxU("set_delay_from_filename"), &options.set_delay_from_filename, true);

  options.validate();
}

void
mmg_dialog::remember_previous_output_directory() {
  wxFileName filename(tc_output->GetValue());
  wxString path = filename.GetPath();

  if (!path.IsEmpty())
    previous_output_directory = path;
}

void
mmg_dialog::on_add_cli_options(wxCommandEvent &evt) {
  cli_options_dlg dlg(this);

  if (dlg.go(cli_options))
    update_command_line();
}

void
mmg_dialog::on_close(wxCloseEvent &evt) {
  save_preferences();
  Destroy();
}

void
mmg_dialog::set_on_top(bool on_top) {
  long style;

  style = GetWindowStyleFlag();
  if (on_top)
    style |= wxSTAY_ON_TOP;
  else
    style &= ~wxSTAY_ON_TOP;
  SetWindowStyleFlag(style);
}

void
mmg_dialog::restore_on_top() {
  set_on_top(options.on_top);
}

void
mmg_dialog::on_file_options(wxCommandEvent &evt) {
  options_dialog dlg(this, options);

  if (dlg.ShowModal() != wxID_OK)
    return;

  log_window->Show(options.gui_debugging);
  set_on_top(options.on_top);
  query_mkvmerge_capabilities();
}

void
mmg_dialog::on_run_header_editor(wxCommandEvent &evt) {
  header_editor_frame_c *window  = new header_editor_frame_c(this);

  window->Show();
}

void
mmg_dialog::query_mkvmerge_capabilities() {
  wxString tmp;
  wxArrayString output;
  vector<wxString> parts;
  int result, i;

  wxLogMessage(Z("Querying mkvmerge's capabilities"));
  tmp = wxT("\"") + options.mkvmerge + wxT("\" --capabilities");
#if defined(SYS_WINDOWS)
  result = wxExecute(tmp, output);
#else
  // Workaround for buggy behaviour of some wxWindows/GTK combinations.
  wxProcess *process;
  wxInputStream *out;
  int c;
  string tmps;

  process = new wxProcess(this, 1);
  process->Redirect();
  result = wxExecute(tmp, wxEXEC_ASYNC, process);
  if (result == 0)
    return;

  out = process->GetInputStream();
  tmps = "";
  c = 0;
  while (1) {
    if (!out->Eof()) {
      c = out->GetC();
      if (c == '\n') {
        output.Add(wxU(tmps.c_str()));
        tmps = "";
      } else if (c < 0)
        break;
      else if (c != '\r')
        tmps += c;
    } else
      break;
  }
  if (tmps.length() > 0)
    output.Add(wxU(tmps.c_str()));
  result = 0;
#endif

  if (result == 0) {
    capabilities.clear();
    for (i = 0; i < output.Count(); i++) {
      tmp = output[i];
      strip(tmp);
      wxLogMessage(wxT("Capability: %s"), tmp.c_str());
      parts = split(tmp, wxU("="), 2);
      if (parts.size() == 1)
        capabilities[parts[0]] = wxT("true");
      else
        capabilities[parts[0]] = parts[1];
    }
  }
}

IMPLEMENT_CLASS(cli_options_dlg, wxDialog);
BEGIN_EVENT_TABLE(cli_options_dlg, wxDialog)
  EVT_COMBOBOX(ID_CLIOPTIONS_COB, cli_options_dlg::on_option_changed)
  EVT_BUTTON(ID_CLIOPTIONS_ADD,   cli_options_dlg::on_add_clicked)
END_EVENT_TABLE();

IMPLEMENT_CLASS(show_text_dlg, wxDialog);
BEGIN_EVENT_TABLE(show_text_dlg, wxDialog)
END_EVENT_TABLE();

IMPLEMENT_CLASS(mmg_dialog, wxFrame);
BEGIN_EVENT_TABLE(mmg_dialog, wxFrame)
  EVT_BUTTON(ID_B_BROWSEOUTPUT,           mmg_dialog::on_browse_output)
  EVT_BUTTON(ID_B_STARTMUXING,            mmg_dialog::on_run)
  EVT_BUTTON(ID_B_COPYTOCLIPBOARD,        mmg_dialog::on_copy_to_clipboard)
  EVT_BUTTON(ID_B_ADD_TO_JOBQUEUE,        mmg_dialog::on_add_to_jobqueue)
  EVT_TIMER(ID_T_UPDATECMDLINE,           mmg_dialog::on_update_command_line)
  EVT_TIMER(ID_T_STATUSBAR,               mmg_dialog::on_clear_status_bar)
  EVT_MENU(ID_M_FILE_EXIT,                mmg_dialog::on_quit)
  EVT_MENU(ID_M_FILE_NEW,                 mmg_dialog::on_file_new)
  EVT_MENU(ID_M_FILE_LOAD,                mmg_dialog::on_file_load)
  EVT_MENU(ID_M_FILE_SAVE,                mmg_dialog::on_file_save)
  EVT_MENU(ID_M_FILE_OPTIONS,             mmg_dialog::on_file_options)
  EVT_MENU(ID_M_FILE_HEADEREDITOR,        mmg_dialog::on_run_header_editor)
  EVT_MENU(ID_M_FILE_SETOUTPUT,           mmg_dialog::on_browse_output)
  EVT_MENU(ID_M_MUXING_START,             mmg_dialog::on_run)
  EVT_MENU(ID_M_MUXING_SHOW_CMDLINE,      mmg_dialog::on_show_cmdline)
  EVT_MENU(ID_M_MUXING_COPY_CMDLINE,      mmg_dialog::on_copy_to_clipboard)
  EVT_MENU(ID_M_MUXING_SAVE_CMDLINE,      mmg_dialog::on_save_cmdline)
  EVT_MENU(ID_M_MUXING_CREATE_OPTIONFILE, mmg_dialog::on_create_optionfile)
  EVT_MENU(ID_M_MUXING_ADD_TO_JOBQUEUE,   mmg_dialog::on_add_to_jobqueue)
  EVT_MENU(ID_M_MUXING_MANAGE_JOBS,       mmg_dialog::on_manage_jobs)
  EVT_MENU(ID_M_MUXING_ADD_CLI_OPTIONS,   mmg_dialog::on_add_cli_options)
  EVT_MENU(ID_M_HELP_HELP,                mmg_dialog::on_help)
  EVT_MENU(ID_M_HELP_ABOUT,               mmg_dialog::on_about)
  EVT_MENU(ID_M_FILE_LOADLAST1,           mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST2,           mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST3,           mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_FILE_LOADLAST4,           mmg_dialog::on_file_load_last)
  EVT_MENU(ID_M_CHAPTERS_NEW,             mmg_dialog::on_new_chapters)
  EVT_MENU(ID_M_CHAPTERS_LOAD,            mmg_dialog::on_load_chapters)
  EVT_MENU(ID_M_CHAPTERS_SAVE,            mmg_dialog::on_save_chapters)
  EVT_MENU(ID_M_CHAPTERS_SAVEAS,          mmg_dialog::on_save_chapters_as)
  EVT_MENU(ID_M_CHAPTERS_SAVETOKAX,       mmg_dialog::on_save_chapters_to_kax_file)
  EVT_MENU(ID_M_CHAPTERS_VERIFY,          mmg_dialog::on_verify_chapters)
  EVT_MENU(ID_M_CHAPTERS_SETDEFAULTS,     mmg_dialog::on_set_default_chapter_values)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST1,       mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST2,       mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST3,       mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_CHAPTERS_LOADLAST4,       mmg_dialog::on_chapters_load_last)
  EVT_MENU(ID_M_WINDOW_INPUT,             mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_ATTACHMENTS,       mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_GLOBAL,            mmg_dialog::on_window_selected)
  EVT_MENU(ID_M_WINDOW_CHAPTEREDITOR,     mmg_dialog::on_window_selected)
  EVT_CLOSE(mmg_dialog::on_close)
END_EVENT_TABLE();

void
mmg_app::init_ui_locale() {
  std::string locale;

#if defined(HAVE_LIBINTL_H)
  wxString w_locale;

  translation_c::initialize_available_translations();

  wxConfigBase *cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  if (cfg->Read(wxT("ui_locale"), &w_locale)) {
    locale = wxMB(w_locale);
    if (-1 == translation_c::look_up_translation(locale))
      locale = "";
  }

  if (locale.empty()) {
    locale = translation_c::get_default_ui_locale();
    if (-1 == translation_c::look_up_translation(locale))
      locale = "";
  }

  m_ui_locale = locale;
#endif  // HAVE_LIBINTL_H

  init_locales(locale);
}

bool
mmg_app::OnInit() {
  wxConfigBase *cfg;
  uint32_t i;
  wxString k, v;
  int index;

  cfg = new wxConfig(wxT("mkvmergeGUI"));
  wxConfigBase::Set(cfg);

  init_stdio();
  init_ui_locale();
  mm_file_io_c::setup();
  cc_local_utf8 = utf8_init("");
  xml_element_map_init();

  cfg->SetPath(wxT("/GUI"));
  cfg->Read(wxT("last_directory"), &last_open_dir, wxEmptyString);
  for (i = 0; i < 4; i++) {
    k.Printf(wxT("last_settings %u"), i);
    if (cfg->Read(k, &v))
      last_settings.push_back(v);
    k.Printf(wxT("last_chapters %u"), i);
    if (cfg->Read(k, &v))
      last_chapters.push_back(v);
  }
  cfg->SetPath(wxT("/chapter_editor"));
  cfg->Read(wxT("default_language"), &k, wxT("und"));
  default_chapter_language = to_utf8(k).c_str();
  index = map_to_iso639_2_code(default_chapter_language.c_str());
  if (-1 == index)
    default_chapter_language = "und";
  else
    default_chapter_language = iso639_languages[index].iso639_2_code;
  if (cfg->Read(wxT("default_country"), &k) && (0 < k.length()))
    default_chapter_country = to_utf8(k).c_str();
  if (!is_valid_cctld(default_chapter_country.c_str()))
    default_chapter_country = "";

  app = this;
  mdlg = new mmg_dialog();
  mdlg->Show(true);

  handle_command_line_arguments();

  return true;
}

void
mmg_app::handle_command_line_arguments() {
  if (1 >= app->argc)
    return;

  if (wxString(app->argv[1]) == wxT("--edit-headers")) {
    if (2 >= app->argc)
      wxMessageBox(Z("Missing file name after for the option '--edit-headers'."), Z("Missing file name"), wxOK | wxCENTER | wxICON_ERROR);
    else {
      header_editor_frame_c *window  = new header_editor_frame_c(mdlg);
      window->Show();
      window->open_file(wxFileName(app->argv[2]));
    }

    return;
  }

  wxString file = app->argv[1];
  if (!wxFileExists(file) || wxDirExists(file))
    wxMessageBox(wxString::Format(Z("The file '%s' does not exist."), file.c_str()), Z("Error loading settings"), wxOK | wxCENTER | wxICON_ERROR);
  else {
#ifdef SYS_WINDOWS
    if ((file.Length() > 3) && (file.c_str()[1] != wxT(':')) && (file.c_str()[0] != wxT('\\')))
      file = wxGetCwd() + wxT("\\") + file;
#else
    if ((file.Length() > 0) && (file.c_str()[0] != wxT('/')))
      file = wxGetCwd() + wxT("/") + file;
#endif

    if (wxFileName(file).GetExt() == wxU("mmg"))
      mdlg->load(file);
    else
      mdlg->input_page->add_file(file, false);
  }
}

int
mmg_app::OnExit() {
  wxString s;
  wxConfigBase *cfg;

  cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("last_directory"), last_open_dir);
  cfg->SetPath(wxT("/chapter_editor"));
  cfg->Write(wxT("default_language"), wxString(default_chapter_language.c_str(), wxConvUTF8));
  cfg->Write(wxT("default_country"), wxString(default_chapter_country.c_str(), wxConvUTF8));
  cfg->Flush();

  delete cfg;

  utf8_done();

  return 0;
}

IMPLEMENT_APP(mmg_app)
