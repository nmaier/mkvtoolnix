/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "input" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/dir.h>
#include <wx/dnd.h>
#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/regex.h>
#include <wx/statline.h>

#include "common/extern_data.h"
#include "common/file_types.h"
#include "common/iso639.h"
#include "common/mm_io_x.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "mmg/message_dialog.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/additional_parts_dlg.h"
#include "mmg/tabs/ask_scan_for_playlists_dlg.h"
#include "mmg/tabs/attachments.h"
#include "mmg/tabs/input.h"
#include "mmg/tabs/global.h"
#include "mmg/tabs/select_scanned_file_dlg.h"
#include "mmg/tabs/scanning_for_playlists_dlg.h"

wxArrayString sorted_iso_codes;
wxArrayString sorted_charsets;
bool title_was_present = false;

const wxChar *predefined_aspect_ratios[] = {
  wxEmptyString,
  wxT("4/3"),
  wxT("1.66"),
  wxT("16/9"),
  wxT("1.85"),
  wxT("2.00"),
  wxT("2.21"),
  wxT("2.35"),
  nullptr
};

const wxChar *predefined_fourccs[] = {
  wxEmptyString,
  wxT("DIVX"),
  wxT("DIV3"),
  wxT("DX50"),
  wxT("MP4V"),
  wxT("XVID"),
  nullptr
};

class input_drop_target_c: public wxFileDropTarget {
private:
  tab_input *owner;
public:
  input_drop_target_c(tab_input *n_owner):
    owner(n_owner) {}
  virtual bool OnDropFiles(wxCoord /* x */, wxCoord /* y */, const wxArrayString &dropped_files) {
    owner->add_dropped_files(dropped_files);
    return true;
  }
};

wxEventType const tab_input::ms_event{wxNewEventType()};

tab_input::tab_input(wxWindow *parent)
  : wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL)
  , selected_file{-1}
  , selected_track{-1}
{
  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(TOPBOTTOMSPACING);

  st_input_files       = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(st_input_files, 0, wxALL, STDSPACING);
  siz_all->Add(siz_line, 0, wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  lb_input_files = new wxListBox(this, ID_LB_INPUTFILES);

  b_add_file         = new wxButton(this, ID_B_ADDFILE,          wxEmptyString);
  b_remove_file      = new wxButton(this, ID_B_REMOVEFILE,       wxEmptyString);
  b_append_file      = new wxButton(this, ID_B_APPENDFILE,       wxEmptyString);
  b_remove_all_files = new wxButton(this, ID_B_REMOVE_ALL_FILES, wxEmptyString);
  b_additional_parts = new wxButton(this, ID_B_ADDITIONAL_PARTS, wxEmptyString);

  wxBoxSizer *siz_buttons_all  = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer *siz_buttons_line = new wxBoxSizer(wxHORIZONTAL);

  siz_buttons_line->Add(b_add_file,    0, wxGROW | wxALL, STDSPACING);
  siz_buttons_line->Add(b_append_file, 0, wxGROW | wxALL, STDSPACING);
  siz_buttons_all->Add(siz_buttons_line);

  siz_buttons_line = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons_line->Add(b_remove_file,      0, wxGROW | wxALL, STDSPACING);
  siz_buttons_line->Add(b_remove_all_files, 0, wxGROW | wxALL, STDSPACING);
  siz_buttons_all->Add(siz_buttons_line);

  siz_buttons_all->Add(b_additional_parts, 0, wxGROW | wxALL, STDSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(lb_input_files, 1, wxGROW | wxALL, STDSPACING);
  siz_line->Add(siz_buttons_all, 0, wxGROW | wxALL, STDSPACING);

  siz_all->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_line  = new wxBoxSizer(wxHORIZONTAL);
  st_tracks = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  st_tracks->Enable(false);
  siz_line->Add(st_tracks, 0, wxALL, STDSPACING);
  siz_all->Add(siz_line, 0, wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer *siz_column = new wxBoxSizer(wxVERTICAL);
  clb_tracks = new wxCheckListBox(this, ID_CLB_TRACKS);
  clb_tracks->Enable(false);
  siz_line->Add(clb_tracks, 1, wxGROW | wxALIGN_TOP | wxALL, STDSPACING);
  b_track_up = new wxButton(this, ID_B_TRACKUP, wxEmptyString);
  b_track_up->Enable(false);
  siz_column->Add(b_track_up, 0, wxGROW | wxALL, STDSPACING);
  b_track_down = new wxButton(this, ID_B_TRACKDOWN, wxEmptyString);
  b_track_down->Enable(false);
  siz_column->Add(b_track_down, 0, wxGROW | wxALL, STDSPACING);
  siz_line->Add(siz_column);
  siz_all->Add(siz_line, 3, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  nb_options = new wxNotebook(this, ID_NB_OPTIONS, wxDefaultPosition, wxDefaultSize, wxNB_TOP);

  ti_general = new tab_input_general(nb_options, this);
  ti_format  = new tab_input_format(nb_options,  this);
  ti_extra   = new tab_input_extra(nb_options,   this);

  nb_options->AddPage(ti_general, wxEmptyString);
  nb_options->AddPage(ti_format,  wxEmptyString);
  nb_options->AddPage(ti_extra,   wxEmptyString);

  siz_all->Add(nb_options, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_all->AddSpacer(TOPBOTTOMSPACING);

  SetSizer(siz_all);

  translate_ui();

  set_track_mode(nullptr);
  enable_file_controls();

  dont_copy_values_now = false;
  value_copy_timer.SetOwner(this, ID_T_INPUTVALUES);
  value_copy_timer.Start(333);

  SetDropTarget(new input_drop_target_c(this));

  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();

  cfg->SetPath(wxT("/GUI"));
}

void
tab_input::translate_ui() {
  dont_copy_values_now = true;

  st_input_files->SetLabel(Z("Input files:"));
  b_add_file->SetLabel(Z("add"));
  b_remove_file->SetLabel(Z("remove"));
  b_append_file->SetLabel(Z("append"));
  b_remove_all_files->SetLabel(Z("remove all"));
  st_tracks->SetLabel(Z("Tracks, chapters and tags:"));
  b_track_up->SetLabel(Z("up"));
  b_track_down->SetLabel(Z("down"));
  b_additional_parts->SetLabel(Z("additional parts"));
  nb_options->SetPageText(0, Z("General track options"));
  nb_options->SetPageText(1, Z("Format specific options"));
  nb_options->SetPageText(2, Z("Extra options"));

  media_files.Empty();

  ti_extra->translate_ui();
  ti_format->translate_ui();
  ti_general->translate_ui();

  unsigned int track_idx;
  for (track_idx = 0; tracks.size() > track_idx; ++track_idx) {
    clb_tracks->SetString(track_idx, tracks[track_idx]->create_label());
    clb_tracks->Check(    track_idx, tracks[track_idx]->enabled);
  }

  dont_copy_values_now = true;
}

void
tab_input::set_track_mode(mmg_track_t *t) {
  ti_general->set_track_mode(t);
  ti_format->set_track_mode(t);
  ti_extra->set_track_mode(t);
}

wxString
tab_input::setup_file_type_filter() {
  if (!media_files.empty())
    return media_files;

  std::vector<file_type_t> &file_types = file_type_t::get_supported();

  std::map<wxString, bool> all_extensions_map;
  wxString filters;

  for (auto &file_type : file_types) {
    std::vector<wxString> extensions = split(wxU(file_type.extensions), wxU(" "));
    std::vector<wxString> extensions_full;

    for (auto &extension : extensions) {
      all_extensions_map[extension] = true;
      extensions_full.push_back(wxString::Format(wxT("*.%s"), extension.c_str()));

#if !defined(SYS_WINDOWS)
      wxString extension_upper = extension.Upper();
      all_extensions_map[extension_upper] = true;
      if (extension_upper != extension)
        extensions_full.push_back(wxString::Format(wxT("*.%s"), extension_upper.c_str()));
#endif  // !SYS_WINDOWS
    }

    wxString filter_ext  = join(wxT(";"), extensions_full);
    filters             += wxString::Format(wxT("|%s (%s)|%s"), wxU(file_type.title).c_str(), filter_ext.c_str(), filter_ext.c_str());
  }

  std::vector<wxString> all_extensions;
  for (auto &extension : all_extensions_map)
    all_extensions.push_back(wxString::Format(wxT("*.%s"), extension.first.c_str()));

  std::sort(all_extensions.begin(), all_extensions.end());

  media_files.Printf(Z("All supported media files|%s%s|%s"), join(wxT(";"), all_extensions).c_str(), filters.c_str(), ALLFILES.c_str());

  return media_files;
}

void
tab_input::select_file(bool append) {
  wxFileDialog dlg(nullptr, append ? Z("Choose an input file to append") : Z("Choose an input file to add"), last_open_dir, wxEmptyString, setup_file_type_filter(), wxFD_OPEN | wxFD_MULTIPLE);
  if(dlg.ShowModal() != wxID_OK)
    return;

  wxArrayString selected_files;
  size_t i;

  dlg.GetPaths(selected_files);
  for (i = 0; i < selected_files.Count(); i++)
    add_file(selected_files[i], append);
}

void
tab_input::on_add_file(wxCommandEvent &) {
  select_file(false);
}

void
tab_input::on_append_file(wxCommandEvent &) {
  select_file(true);
}

void
tab_input::parse_track_line(mmg_file_cptr file,
                            wxString const &line,
                            wxString const &delay_from_file_name,
                            std::map<char, bool> &default_track_found_for) {
  auto track = std::make_shared<mmg_track_t>();
  file->tracks.push_back(track);

  auto id          = line.AfterFirst(wxT(' ')).AfterFirst(wxT(' ')).BeforeFirst(wxT(':'));
  auto type        = line.AfterFirst(wxT(':')).BeforeFirst(wxT('(')).Mid(1).RemoveLast();
  track->ctype     = line.AfterFirst(wxT('(')).BeforeFirst(wxT(')'));
  auto info        = line.AfterFirst(wxT('[')).BeforeLast(wxT(']'));

  track->appending = file->appending;
  track->type      = type == wxT("audio")     ? 'a'
                   : type == wxT("video")     ? 'v'
                   : type == wxT("subtitles") ? 's'
                   :                            '?';

  parse_number(to_utf8(id), track->id);

  if (track->is_audio())
    track->delay = delay_from_file_name;

  if (info.IsEmpty())
    return;

  for (auto &arg : split(info, wxU(" "))) {
    auto pair = split(arg, wxU(":"), 2);
    if (pair.size() != 2)
      continue;

    if (pair[0] == wxT("track_name")) {
      track->track_name             = unescape(pair[1]);
      track->track_name_was_present = true;

    } else if (pair[0] == wxT("language"))
      track->language = unescape(pair[1]);

    else if (pair[0] == wxT("cropping"))
      track->cropping = unescape(pair[1]);

    else if (pair[0] == wxT("display_dimensions")) {
      int64_t width, height;

      std::vector<wxString> dims = split(pair[1], wxU("x"));
      if ((dims.size() == 2) && parse_number(wxMB(dims[0]), width) && parse_number(wxMB(dims[1]), height)) {
        std::string format;
        fix_format(LLD, format);
        track->dwidth.Printf(wxU(format), width);
        track->dheight.Printf(wxU(format), height);
        track->display_dimensions_selected = true;
      }

    } else if (pair[0] == wxT("default_track")) {
      if (pair[1] != wxT("1"))
        track->default_track = 2; // A definitive 'no'.

      else if ((track->is_audio() || track->is_video() || track->is_subtitles()) && !default_track_found_for[track->type]) {
        track->default_track                 = 1; // A definitive 'yes'.
        default_track_found_for[track->type] = true;
      }

    } else if (pair[0] == wxT("stereo_mode")) {
      parse_number(wxMB(pair[1]), track->stereo_mode);
      track->stereo_mode += 1;

    } else if (pair[0] == wxT("aac_is_sbr"))
      track->aac_is_sbr = track->aac_is_sbr_detected = pair[1] == wxT("true");

    else if (pair[0] == wxT("forced_track"))
      track->forced_track = pair[1] == wxT("1");

    else if (pair[0] == wxT("packetizer"))
      track->packetizer = pair[1];

  }
}

void
tab_input::parse_container_line(mmg_file_cptr file,
                                wxString const &line) {
  auto container  = line.Mid(11).BeforeFirst(wxT(' '));
  auto info       = line.Mid(11).AfterFirst(wxT('[')).BeforeLast(wxT(']'));

  file->container = container == wxT("AAC")                     ? FILE_TYPE_AAC
                  : container == wxT("AC3")                     ? FILE_TYPE_AC3
                  : container == wxT("AVC/h.264")               ? FILE_TYPE_AVC_ES
                  : container == wxT("AVI")                     ? FILE_TYPE_AVI
                  : container == wxT("Dirac elementary stream") ? FILE_TYPE_DIRAC
                  : container == wxT("DTS")                     ? FILE_TYPE_DTS
                  : container == wxT("IVF")                     ? FILE_TYPE_IVF
                  : container == wxT("Matroska")                ? FILE_TYPE_MATROSKA
                  : container == wxT("MP2/MP3")                 ? FILE_TYPE_MP3
                  : container == wxT("Ogg/OGM")                 ? FILE_TYPE_OGM
                  : container == wxT("Quicktime/MP4")           ? FILE_TYPE_QTMP4
                  : container == wxT("RealMedia")               ? FILE_TYPE_REAL
                  : container == wxT("SRT")                     ? FILE_TYPE_SRT
                  : container == wxT("SSA/ASS")                 ? FILE_TYPE_SSA
                  : container == wxT("VC1 elementary stream")   ? FILE_TYPE_VC1
                  : container == wxT("VobSub")                  ? FILE_TYPE_VOBSUB
                  : container == wxT("WAV")                     ? FILE_TYPE_WAV
                  :                                               FILE_TYPE_IS_UNKNOWN;

  if (info.IsEmpty())
    return;

  std::string segment_uid, next_segment_uid, previous_segment_uid;

  for (auto &arg : split(info, wxU(" "))) {
    auto pair = split(arg, wxU(":"), 2);

    if ((pair.size() == 2) && (pair[0] == wxT("title"))) {
      file->title             = unescape(pair[1]);
      file->title_was_present = true;
      title_was_present       = true;

    } else if ((pair.size() == 2) && (pair[0] == wxT("other_file")))
      file->other_files.push_back(wxFileName(unescape(pair[1])));

    else if ((pair.size() == 2) && (pair[0] == wxT("playlist_file")))
      file->playlist_files.push_back(wxFileName(unescape(pair[1])));

    else if ((pair.size() == 2) && (pair[0] == wxT("segment_uid")))
      segment_uid = unescape(to_utf8(pair[1]));

    else if ((pair.size() == 2) && (pair[0] == wxT("next_segment_uid")))
      next_segment_uid = unescape(to_utf8(pair[1]));

    else if ((pair.size() == 2) && (pair[0] == wxT("previous_segment_uid")))
      previous_segment_uid = unescape(to_utf8(pair[1]));
  }

  if (   (!next_segment_uid.empty() || !previous_segment_uid.empty())
      && mdlg->global_page->tc_segment_uid->         GetValue().IsEmpty()
      && mdlg->global_page->tc_next_segment_uid->    GetValue().IsEmpty()
      && mdlg->global_page->tc_previous_segment_uid->GetValue().IsEmpty()) {

    mdlg->global_page->tc_segment_uid->         SetValue(wxU(segment_uid));
    mdlg->global_page->tc_next_segment_uid->    SetValue(wxU(next_segment_uid));
    mdlg->global_page->tc_previous_segment_uid->SetValue(wxU(previous_segment_uid));
  }
}

void
tab_input::parse_attachment_line(mmg_file_cptr file,
                                 wxString const &line) {
  auto a = std::make_shared<mmg_attached_file_t>();

  wxRegEx re_att_base(       wxT("^Attachment *ID ([[:digit:]]+): *type *\"([^\"]+)\", *size *([[:digit:]]+)"), wxRE_ICASE);
  wxRegEx re_att_description(wxT("description *\"([^\"]+)\""),                                                  wxRE_ICASE);
  wxRegEx re_att_name(       wxT("name *\"([^\"]+)\""),                                                         wxRE_ICASE);

  if (!re_att_base.Matches(line))
    return;

  re_att_base.GetMatch(line, 1).ToLong(&a->id);
  re_att_base.GetMatch(line, 3).ToLong(&a->size);
  a->mime_type = unescape(re_att_base.GetMatch(line, 2));

  if (re_att_description.Matches(line))
    a->description = unescape(re_att_description.GetMatch(line, 1));
  if (re_att_name.Matches(line))
    a->name = unescape(re_att_name.GetMatch(line, 1));

  a->source = file.get();

  file->attached_files.push_back(a);

  wxLogMessage(wxT("Attached file ID %ld MIME type '%s' size %ld description '%s' name '%s'"),
               a->id, a->mime_type.c_str(), a->size, a->description.c_str(), a->name.c_str());
}

void
tab_input::parse_chapters_line(mmg_file_cptr file,
                               wxString const &line) {
  auto track = std::make_shared<mmg_track_t>();

  parse_number(wxMB(line.AfterFirst(wxT(' ')).BeforeFirst(wxT(' '))), track->num_entries);
  track->type = 'c';
  track->id   = TRACK_ID_CHAPTERS;

  file->tracks.push_back(track);
}

void
tab_input::parse_global_tags_line(mmg_file_cptr file,
                                  wxString const &line) {
  auto track = std::make_shared<mmg_track_t>();

  parse_number(wxMB(line.AfterFirst(wxT(':')).AfterFirst(wxT(' ')).BeforeFirst(wxT(' '))), track->num_entries);
  track->type = 't';
  track->id   = TRACK_ID_GLOBAL_TAGS;

  file->tracks.push_back(track);
}

void
tab_input::parse_tags_line(mmg_file_cptr file,
                           wxString const &line) {
  auto track = std::make_shared<mmg_track_t>();

  parse_number(wxMB(line.BeforeFirst(wxT(':')).AfterLast(wxT(' '))), track->id);
  parse_number(wxMB(line.AfterFirst(wxT(':')).AfterFirst(wxT(' ')).BeforeFirst(wxT(' '))), track->num_entries);
  track->type  = 't';
  track->id   += TRACK_ID_TAGS_BASE;

  file->tracks.push_back(track);
}

bool
tab_input::run_mkvmerge_identification(wxString const &file_name,
                                       wxArrayString &output) {
  auto opt_file_name = wxString::Format(wxT("%smmg-mkvmerge-options-%d-%d"), get_temp_dir().c_str(), (int)wxGetProcessId(), (int)wxGetUTCTime());

  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    wxFile opt_file{opt_file_name, wxFile::write};
    opt_file.Write(utf8_bom, 3);
    opt_file.Write(wxT("--output-charset\nUTF-8\n--identify-for-mmg\n"));
    auto arg_utf8 = escape(wxMB(file_name));
    opt_file.Write(arg_utf8.c_str(), arg_utf8.length());
    opt_file.Write(wxT("\n"));

  } catch (mtx::mm_io::exception &ex) {
    wxString error;
    error.Printf(Z("Could not create a temporary file for mkvmerge's command line option called '%s' (error code %d, %s)."), opt_file_name.c_str(), errno, wxUCS(ex.error()));
    wxMessageBox(error, Z("File creation failed"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  wxString command = wxT("\"") + mdlg->options.mkvmerge_exe() + wxT("\" \"@") + opt_file_name + wxT("\"");

  wxLogMessage(wxT("identify 1: command: ``%s''"), command.c_str());

  wxArrayString errors;
  auto result = wxExecute(command, output, errors);

  wxLogMessage(wxT("identify 1: result: %d"), result);
  for (size_t i = 0; i < output.Count(); i++)
    wxLogMessage(wxT("identify 1: output[%d]: ``%s''"), i, output[i].c_str());
  for (size_t i = 0; i < errors.Count(); i++)
    wxLogMessage(wxT("identify 1: errors[%d]: ``%s''"), i, errors[i].c_str());

  wxRemoveFile(opt_file_name);

  if ((0 == result) || (1 == result))
    return true;

  wxString error_message, error_heading;

  if (3 == result) {
    wxString container = Z("unknown");

    int pos;
    if (output.Count() && (wxNOT_FOUND != (pos = output[0].Find(wxT("container:")))))
      container = output[0].Mid(pos + 11);

    wxString info;
    info.Printf(Z("The file is an unsupported container format (%s)."), container.c_str());
    break_line(info, 60);

    wxMessageBox(info, Z("Unsupported format"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }


  if ((0 > result) || (1 < result)) {
    for (size_t i = 0; i < output.Count(); i++)
      error_message += output[i] + wxT("\n");
    error_message += wxT("\n");
    for (size_t i = 0; i < errors.Count(); i++)
      error_message += errors[i] + wxT("\n");

    error_heading.Printf(Z("File identification failed for '%s'. Return code: %d"), file_name.c_str(), result);

  } else if (0 < result) {
    error_message.Printf(Z("File identification failed. Return code: %d. Errno: %d (%s). Make sure that you've selected a mkvmerge executable in the settings dialog."),
                         result, errno, wxUCS(strerror(errno)));
    error_heading = Z("File identification failed");
  }

  if (error_message.IsEmpty())
    return true;

  error_message =
      Z("Command line used:")
    + wxT("\n\n")
    + wxString::Format(wxT("\"%s\" --output-charset UTF-8 --identify-for-mmg \"%s\""), mdlg->options.mkvmerge_exe().c_str(), file_name.c_str())
    + wxT("\n\n")
    + Z("Output:")
    + wxT("\n\n")
    + error_message;

  message_dialog::show(this, Z("File identification failed"), error_heading, error_message);

  return false;
}

void
tab_input::parse_identification_output(mmg_file_cptr file,
                                       wxArrayString const &output) {
  wxString delay_from_file_name;
  if (mdlg->options.set_delay_from_filename) {
    wxRegEx re_delay(wxT("delay[[:blank:]]+(-?[[:digit:]]+)"), wxRE_ICASE);
    if (re_delay.Matches(file->file_name))
      delay_from_file_name = re_delay.GetMatch(file->file_name, 1);
  }

  std::map<char, bool> default_track_found_for;
  default_track_found_for['a'] = -1 != default_track_checked('a');
  default_track_found_for['v'] = -1 != default_track_checked('v');
  default_track_found_for['s'] = -1 != default_track_checked('s');

  for (size_t i = 0; i < output.Count(); i++) {
    int pos;

    if (output[i].Find(wxT("Track")) == 0)
      parse_track_line(file, output[i], delay_from_file_name, default_track_found_for);

    else if ((pos = output[i].Find(wxT("container:"))) != wxNOT_FOUND)
      parse_container_line(file, output[i].Mid(pos));

    else if (output[i].Find(wxT("Attachment")) == 0)
      parse_attachment_line(file, output[i]);

    else if (output[i].Find(wxT("Chapters")) == 0)
      parse_chapters_line(file, output[i]);

    else if (output[i].Find(wxT("Global tags")) == 0)
      parse_global_tags_line(file, output[i]);

    else if (output[i].Find(wxT("Tags")) == 0)
      parse_tags_line(file, output[i]);
  }
}

void
tab_input::insert_file_in_controls(mmg_file_cptr file,
                                   bool append) {
  // Look for a place to insert the new file-> If the file is only "added",
  // then it will be added to the back of the list. If it is "appended",
  // then it should be inserted right after the currently selected file->
  int new_file_pos;
  if (!append)
    new_file_pos = lb_input_files->GetCount();
  else {
    new_file_pos = lb_input_files->GetSelection();
    if (wxNOT_FOUND == new_file_pos)
      new_file_pos = lb_input_files->GetCount();
    else {
      do {
        ++new_file_pos;
      } while ((files.size() > static_cast<size_t>(new_file_pos)) && files[new_file_pos]->appending);
    }
  }

  wxFileName fn{file->file_name};
  lb_input_files->Insert(wxString::Format(wxT("%s%s (%s)"), append ? wxT("++> ") : wxEmptyString, fn.GetFullName().c_str(), fn.GetPath().c_str()), new_file_pos);

  files.insert(files.begin() + new_file_pos, file);

  // After inserting the file the "source" index is wrong for all files
  // after the insertion position.
  for (auto &track : tracks)
    if (track->source >= new_file_pos)
      ++track->source;

  auto i = 0u;
  for (auto &t : file->tracks) {
    int new_track_pos;

    t->enabled = !mdlg->global_page->cb_webm_mode->IsChecked() || t->is_webm_compatible();
    t->source  = new_file_pos;

    // Look for a place to insert this new track. If the file is "added" then
    // the new track is simply appended to the list of existing tracks.
    // If the file is "appended" then it should be put to the end of the
    // chain of tracks being appended. The n'th track from this new file
    // should be appended to the n'th track of the "old" file this new file is
    // appended to. So first I have to find the n'th track of the "old" file->
    // Then I have to skip over all the other tracks that are already appended
    // to that n'th track. The insertion point is right after that.
    if (append) {
      unsigned int nth_old_track = 0;
      new_track_pos              = 0;
      while ((tracks.size() > static_cast<size_t>(new_track_pos)) && (nth_old_track < (i + 1))) {
        if (tracks[new_track_pos]->source == (new_file_pos - 1))
          ++nth_old_track;
        ++new_track_pos;
      }

      // Either we've found the n'th track from the "old" file or we're at
      // the end of the list. In the latter case we just append the track
      // at the end and let the user figure out which track he really wants
      // to append it to.
      if (nth_old_track == (i + 1))
        while ((tracks.size() > static_cast<size_t>(new_track_pos)) && tracks[new_track_pos]->appending)
          ++new_track_pos;

    } else
      new_track_pos = tracks.size();

    tracks.insert(tracks.begin() + new_track_pos, t.get());
    clb_tracks->Insert(t->create_label(), new_track_pos);
    clb_tracks->Check(new_track_pos, t->enabled);

    ++i;
  }

  for (auto &attached_file : file->attached_files)
    mdlg->attachments_page->add_attached_file(attached_file);
}

void
tab_input::add_file(wxString const &file_name,
                    bool append) {
  wxFileName file_name_obj(file_name);
  for (auto file : files)
    for (auto &other_file_name : file->other_files)
      if (file_name_obj == other_file_name) {
        wxMessageBox(wxString::Format(Z("The file '%s' is already processed in combination with the file '%s'. It cannot be added a second time."),
                                      file_name_obj.GetFullPath().c_str(), other_file_name.GetFullPath().c_str()),
                     Z("File is already processed"), wxOK | wxCENTER | wxICON_ERROR);
        return;
      }

  last_open_dir = file_name_obj.GetPath();

  wxArrayString output;
  if (!run_mkvmerge_identification(file_name, output))
    return;

  wxString actual_file_name = append ? file_name : check_for_and_handle_playlist_file(file_name, output);

  if (actual_file_name.IsEmpty())
    return;

  auto file       = std::make_shared<mmg_file_t>();
  file->appending = append;
  file->file_name = actual_file_name;

  parse_identification_output(file, output);

  if (file->tracks.empty()) {
    wxMessageBox(wxString::Format(Z("The input file '%s' does not contain any tracks."), file->file_name.c_str()), Z("No tracks found"));
    return;
  }

  insert_file_in_controls(file, append);

  mdlg->set_title_maybe(file->title);
  if (file->container == FILE_TYPE_OGM)
    for (auto &track : file->tracks)
      if (track->is_video() && !track->track_name.IsEmpty()) {
        mdlg->set_title_maybe(track->track_name);
        break;
      }

  mdlg->set_output_maybe(file->file_name);

  st_tracks->Enable(true);
  clb_tracks->Enable(true);
  enable_file_controls();
}

void
tab_input::add_dropped_files(wxArrayString const &files) {
  for (auto &file : files)
    dropped_files.Add(file);

  wxCommandEvent evt(tab_input::ms_event, tab_input::dropped_files_added);
  wxPostEvent(this, evt);
}

void
tab_input::on_dropped_files_added(wxCommandEvent &) {
  for (auto &file : dropped_files)
    add_file(file, false);
  dropped_files.clear();
}

void
tab_input::on_remove_file(wxCommandEvent &) {
  mmg_track_t *t;
  std::vector<mmg_file_t>::iterator eit;

  if (-1 == selected_file)
    return;

  mmg_file_cptr &f = files[selected_file];

  // Files may not be removed if something else is still being appended to
  // them.
  unsigned int i;
  for (i = 0; i < (tracks.size() - 1); i++)
    if ((tracks[i]->source == selected_file) && tracks[i + 1]->appending && (tracks[i]->source != tracks[i + 1]->source)) {
      wxString err;

      err.Printf(Z("The current file (number %d) cannot be removed. "
                   "There are other files -- at least file number %d -- whose tracks are supposed to be appended to tracks from this file. "
                   "Please remove those files first."),
                 selected_file + 1, tracks[i + 1]->source + 1);
      wxMessageBox(err, Z("File removal not possible"), wxOK | wxICON_ERROR | wxCENTER);
      return;
    }

  dont_copy_values_now = true;

  i = 0;
  while (i < tracks.size()) {
    t = tracks[i];
    if (t->source == selected_file) {
      clb_tracks->Delete(i);
      tracks.erase(tracks.begin() + i);
    } else {
      if (t->source > selected_file)
        t->source--;
      i++;
    }
  }

  // Unset the "file/segment title" box if the segment title came from this
  // source file and the user hasn't changed it since.
  if ((f->title != wxEmptyString) && f->title_was_present && (f->title == mdlg->global_page->tc_title->GetValue()))
    mdlg->global_page->tc_title->SetValue(wxEmptyString);

  mdlg->attachments_page->remove_attached_files_for(files[selected_file]);

  files.erase(files.begin() + selected_file);
  lb_input_files->Delete(selected_file);
  selected_file = lb_input_files->GetSelection();
  enable_file_controls();
  b_track_up->Enable(-1 != selected_file);
  b_track_down->Enable(-1 != selected_file);

  selected_track = clb_tracks->GetSelection();
  set_track_mode(-1 == selected_track ? nullptr : tracks[selected_track]);
  if (tracks.empty()) {
    st_tracks->Enable(false);
    clb_tracks->Enable(false);
    mdlg->remove_output_filename();
  }

  dont_copy_values_now = false;
}

void
tab_input::on_remove_all_files(wxCommandEvent &) {
  dont_copy_values_now = true;

  mdlg->attachments_page->remove_all_attached_files();

  lb_input_files->Clear();
  clb_tracks->Clear();

  tracks.clear();
  files.clear();

  selected_file  = -1;
  selected_track = -1;
  st_tracks->Enable(false);
  clb_tracks->Enable(false);
  b_track_up->Enable(false);
  b_track_down->Enable(false);

  set_track_mode(nullptr);
  enable_file_controls();

  mdlg->remove_output_filename();

  dont_copy_values_now = false;
}

void
tab_input::on_additional_parts(wxCommandEvent &) {
  if (0 > selected_file)
    return;

  additional_parts_dialog dlg{this, wxFileName{ files[selected_file]->file_name }, files[selected_file]->other_files};
  files[selected_file]->other_files = dlg.get_file_names();
}

void
tab_input::on_move_track_up(wxCommandEvent &) {
  if (1 > selected_track)
    return;

  // Appended tracks may not be at the top.
  if ((1 == selected_track) && tracks[selected_track]->appending)
    return;

  dont_copy_values_now      = true;

  int current_track         = selected_track;

  mmg_track_t *t            = tracks[current_track - 1];
  tracks[current_track - 1] = tracks[current_track];
  tracks[current_track]     = t;
  wxString s                = clb_tracks->GetString(current_track - 1);

  clb_tracks->SetString(current_track - 1, clb_tracks->GetString(current_track));
  clb_tracks->SetString(current_track, s);
  clb_tracks->SetSelection(current_track - 1);
  clb_tracks->Check(current_track - 1, tracks[current_track - 1]->enabled);
  clb_tracks->Check(current_track, tracks[current_track]->enabled);
  current_track--;

  b_track_up->Enable(current_track > 0);
  b_track_down->Enable(true);

  selected_track       = current_track;
  dont_copy_values_now = false;
}

void
tab_input::on_move_track_down(wxCommandEvent &) {
  if ((0 > selected_track) || (static_cast<size_t>(selected_track) >= tracks.size() - 1))
    return;

  // Appended tracks may not be at the top.
  if ((0 == selected_track) && (tracks.size() > 1) && tracks[1]->appending)
    return;

  dont_copy_values_now      = true;

  int current_track         = selected_track;

  mmg_track_t *t            = tracks[current_track + 1];
  tracks[current_track + 1] = tracks[current_track];
  tracks[current_track]     = t;
  wxString s                = clb_tracks->GetString(current_track + 1);

  clb_tracks->SetString(current_track + 1, clb_tracks->GetString(current_track));
  clb_tracks->SetString(current_track, s);
  clb_tracks->SetSelection(current_track + 1);
  clb_tracks->Check(current_track + 1, tracks[current_track + 1]->enabled);
  clb_tracks->Check(current_track, tracks[current_track]->enabled);
  current_track++;

  b_track_up->Enable(true);
  b_track_down->Enable(current_track < static_cast<int>(tracks.size() - 1));

  selected_track       = current_track;
  dont_copy_values_now = false;
}

void
tab_input::on_file_selected(wxCommandEvent &) {
  selected_file = lb_input_files->GetSelection();
  enable_file_controls();
}

void
tab_input::on_track_selected(wxCommandEvent &) {
  dont_copy_values_now = true;

  selected_track       = -1;
  int new_sel          = clb_tracks->GetSelection();

  if (0 > new_sel)
    return;

  mmg_track_t *t = tracks[new_sel];

  b_track_up->Enable(new_sel > 0);
  b_track_down->Enable(new_sel < static_cast<int>(tracks.size() - 1));

  set_track_mode(t);

  wxString lang = t->language;
  unsigned int i;
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    if (extract_language_code(sorted_iso_codes[i]) == lang) {
      lang = sorted_iso_codes[i];
      break;
    }

  ti_extra->cob_cues->SetValue(ti_extra->cob_cues_translations.to_translated(t->cues));
  ti_extra->cob_compression->SetValue(ti_extra->cob_compression_translations.to_translated(t->compression));
  ti_extra->tc_user_defined->SetValue(t->user_defined);

  ti_format->cb_aac_is_sbr->SetValue(t->aac_is_sbr);
  ti_format->cb_fix_bitstream_timing_info->SetValue(t->fix_bitstream_timing_info);
  ti_format->cob_nalu_size_length->SetSelection(t->nalu_size_length / 2);
  ti_format->cob_stereo_mode->SetSelection(t->stereo_mode);
  ti_format->cob_sub_charset->SetValue(ti_format->cob_sub_charset_translations.to_translated(t->sub_charset));
  ti_format->tc_cropping->SetValue(t->cropping);
  ti_format->tc_delay->SetValue(t->delay);
  ti_format->tc_display_height->SetValue(t->dheight);
  ti_format->tc_display_width->SetValue(t->dwidth);
  ti_format->tc_stretch->SetValue(t->stretch);
  set_combobox_selection(ti_format->cob_aspect_ratio, t->aspect_ratio);
  set_combobox_selection(ti_format->cob_fourcc,       t->fourcc);
  set_combobox_selection(ti_format->cob_fps,          t->fps);

  ti_general->cob_default->SetSelection(t->default_track);
  ti_general->cob_forced->SetSelection(t->forced_track ? 1 : 0);
  ti_general->cob_language->SetValue(lang);
  ti_general->tc_tags->SetValue(t->tags);
  ti_general->tc_timecodes->SetValue(t->timecodes);
  ti_general->tc_track_name->SetFocus();
  ti_general->tc_track_name->SetValue(t->track_name);

  selected_track       = new_sel;
  dont_copy_values_now = false;
}

void
tab_input::on_track_enabled(wxCommandEvent &) {
  unsigned int i;
  for (i = 0; i < tracks.size(); i++) {
    if (mdlg->global_page->cb_webm_mode->IsChecked() && clb_tracks->IsChecked(i) && !tracks[i]->is_webm_compatible()) {
      wxMessageBox(Z("This track is not compatible with the WebM mode and cannot be enabled."), Z("Incompatible track"), wxOK | wxCENTER | wxICON_ERROR);
      clb_tracks->Check(i, false);
    }

    tracks[i]->enabled = clb_tracks->IsChecked(i);
  }

  mdlg->update_output_file_name_extension();
}

void
tab_input::on_value_copy_timer(wxTimerEvent &) {
  if (dont_copy_values_now || (selected_track == -1))
    return;

  mmg_track_t *t  = tracks[selected_track];
  t->aspect_ratio = ti_format->cob_aspect_ratio->GetValue();
  t->fourcc       = ti_format->cob_fourcc->GetValue();
  t->fps          = ti_format->cob_fps->GetValue();
}

void
tab_input::on_file_new(wxCommandEvent &) {
  enable_file_controls();
}

void
tab_input::enable_file_controls() {
  bool enable = 0 <= selected_file;

  b_remove_file->Enable(enable);
  b_remove_all_files->Enable(!files.empty());
  b_append_file->Enable(!tracks.empty());
  b_additional_parts->Enable(enable);
}

void
tab_input::save(wxConfigBase *cfg) {
  cfg->SetPath(wxT("/input"));
  cfg->Write(wxT("number_of_files"), (int)files.size());

  unsigned int fidx;
  for (fidx = 0; fidx < files.size(); fidx++) {
    mmg_file_cptr &f = files[fidx];
    wxString s;
    s.Printf(wxT("file %u"), fidx);
    cfg->SetPath(s);
    cfg->Write(wxT("file_name"), f->file_name);
    cfg->Write(wxT("container"), f->container);
    cfg->Write(wxT("appending"), f->appending);

    std::vector<wxString> other_file_names;
    for (auto other_file_name : f->other_files)
      other_file_names.push_back(other_file_name.GetFullPath());
    cfg->Write(wxT("other_files"), join(wxT(":::"), other_file_names));

    cfg->Write(wxT("number_of_tracks"), (int)f->tracks.size());
    unsigned int tidx;
    for (tidx = 0; tidx < f->tracks.size(); tidx++) {
      std::string format;

      mmg_track_cptr &t = f->tracks[tidx];
      s.Printf(wxT("track %u"), tidx);
      cfg->SetPath(s);

      s.Printf(wxT("%c"),                            t->type);
      cfg->Write(wxT("type"),                        s);
      fix_format("" LLD,                             format);
      s.Printf(wxU(format),                          t->id);
      cfg->Write(wxT("id"),                          s);
      cfg->Write(wxT("enabled"),                     t->enabled);
      cfg->Write(wxT("content_type"),                t->ctype);
      cfg->Write(wxT("default_track_2"),             t->default_track);
      cfg->Write(wxT("forced_track"),                t->forced_track);
      cfg->Write(wxT("aac_is_sbr"),                  t->aac_is_sbr);
      cfg->Write(wxT("fix_bitstream_timing_info"),   t->fix_bitstream_timing_info);
      cfg->Write(wxT("language"),                    t->language);
      cfg->Write(wxT("track_name"),                  t->track_name);
      cfg->Write(wxT("cues"),                        t->cues);
      cfg->Write(wxT("delay"),                       t->delay);
      cfg->Write(wxT("stretch"),                     t->stretch);
      cfg->Write(wxT("sub_charset"),                 t->sub_charset);
      cfg->Write(wxT("tags"),                        t->tags);
      cfg->Write(wxT("timecodes"),                   t->timecodes);
      cfg->Write(wxT("display_dimensions_selected"), t->display_dimensions_selected);
      cfg->Write(wxT("aspect_ratio"),                t->aspect_ratio);
      cfg->Write(wxT("cropping"),                    t->cropping);
      cfg->Write(wxT("display_width"),               t->dwidth);
      cfg->Write(wxT("display_height"),              t->dheight);
      cfg->Write(wxT("fourcc"),                      t->fourcc);
      cfg->Write(wxT("fps"),                         t->fps);
      cfg->Write(wxT("nalu_size_length"),            t->nalu_size_length);
      cfg->Write(wxT("stereo_mode"),                 t->stereo_mode);
      cfg->Write(wxT("compression"),                 t->compression);
      cfg->Write(wxT("track_name_was_present"),      t->track_name_was_present);
      cfg->Write(wxT("appending"),                   t->appending);
      cfg->Write(wxT("user_defined"),                t->user_defined);
      cfg->Write(wxT("packetizer"),                  t->packetizer);
      cfg->Write(wxT("num_entries"),                 t->num_entries);

      cfg->SetPath(wxT(".."));
    }

    cfg->Write(wxT("number_of_attached_files"), (int)f->attached_files.size());
    unsigned int afidx;
    for (afidx = 0; f->attached_files.size() > afidx; ++afidx) {
      mmg_attached_file_cptr &a = f->attached_files[afidx];

      cfg->SetPath(wxString::Format(wxT("attached_file %u"), afidx));

      cfg->Write(wxT("id"),          a->id);
      cfg->Write(wxT("size"),        a->size);
      cfg->Write(wxT("name"),        a->name);
      cfg->Write(wxT("mime_type"),   a->mime_type);
      cfg->Write(wxT("description"), a->description);
      cfg->Write(wxT("enabled"),     a->enabled);

      cfg->SetPath(wxT(".."));
    }

    cfg->SetPath(wxT(".."));
  }
  cfg->Write(wxT("track_order"), create_track_order(true));
}

void
tab_input::load(wxConfigBase *cfg,
                int) {
  dont_copy_values_now = true;

  clb_tracks->Clear();
  lb_input_files->Clear();
  set_track_mode(nullptr);
  selected_file  = -1;
  selected_track = -1;

  files.clear();
  tracks.clear();

  cfg->SetPath(wxT("/input"));
  int num_files;
  if (!cfg->Read(wxT("number_of_files"), &num_files) || (0 > num_files)) {
    dont_copy_values_now = false;
    return;
  }

  std::string format;
  fix_format("%u:%lld", format);

  wxString c, track_order;
  long fidx;
  for (fidx = 0; fidx < (long)num_files; fidx++) {
    mmg_file_cptr fi = mmg_file_cptr(new mmg_file_t);

    wxString s;
    s.Printf(wxT("file %ld"), fidx);
    cfg->SetPath(s);

    int num_tracks;
    if (!cfg->Read(wxT("number_of_tracks"), &num_tracks) || (0 > num_tracks)) {
      cfg->SetPath(wxT(".."));
      continue;
    }

    if (!cfg->Read(wxT("file_name"), &fi->file_name)) {
      cfg->SetPath(wxT(".."));
      continue;
    }
    cfg->Read(wxT("title"),     &fi->title);
    cfg->Read(wxT("container"), &fi->container);
    cfg->Read(wxT("appending"), &fi->appending, false);

    cfg->Read(wxT("other_files"), &s, wxT(""));

    if (!s.IsEmpty())
      for (auto &other_file_name : split(s, wxU(":::")))
        fi->other_files.push_back(wxFileName{ other_file_name });

    long tidx;
    for (tidx = 0; tidx < (long)num_tracks; tidx++) {
      mmg_track_cptr tr(new mmg_track_t);
      bool dummy = false;

      s.Printf(wxT("track %ld"), tidx);

      cfg->SetPath(s);
      wxString id;
      if (!cfg->Read(wxT("type"), &c) || (c.Length() != 1) || !cfg->Read(wxT("id"), &id)) {
        cfg->SetPath(wxT(".."));
        continue;
      }

      tr->type = c.c_str()[0];
      if (((tr->type != 'a') && (tr->type != 'v') && (tr->type != 's') && (tr->type != 'c') && (tr->type != 't')) || !parse_number(wxMB(id), tr->id)) {
        cfg->SetPath(wxT(".."));
        continue;
      }

      cfg->Read(wxT("enabled"),                     &tr->enabled);
      cfg->Read(wxT("content_type"),                &tr->ctype);
      if (cfg->Read(wxT("default_track"),           &dummy)) {
        tr->default_track = dummy ? 1 : 0;
      } else
        cfg->Read(wxT("default_track_2"),           &tr->default_track,               0);
      cfg->Read(wxT("forced_track"),                &tr->forced_track,                false);
      cfg->Read(wxT("aac_is_sbr"),                  &tr->aac_is_sbr,                  false);
      cfg->Read(wxT("fix_bitstream_timing_info"),   &tr->fix_bitstream_timing_info,   false);
      cfg->Read(wxT("language"),                    &tr->language);
      cfg->Read(wxT("track_name"),                  &tr->track_name);
      cfg->Read(wxT("cues"),                        &tr->cues);
      cfg->Read(wxT("delay"),                       &tr->delay);
      cfg->Read(wxT("stretch"),                     &tr->stretch);
      cfg->Read(wxT("sub_charset"),                 &tr->sub_charset);
      cfg->Read(wxT("tags"),                        &tr->tags);
      cfg->Read(wxT("display_dimensions_selected"), &tr->display_dimensions_selected, false);
      cfg->Read(wxT("aspect_ratio"),                &tr->aspect_ratio);
      cfg->Read(wxT("cropping"),                    &tr->cropping);
      cfg->Read(wxT("display_width"),               &tr->dwidth);
      cfg->Read(wxT("display_height"),              &tr->dheight);
      cfg->Read(wxT("fourcc"),                      &tr->fourcc);
      cfg->Read(wxT("fps"),                         &tr->fps);
      cfg->Read(wxT("nalu_size_length"),            &tr->nalu_size_length,            4);
      cfg->Read(wxT("stereo_mode"),                 &tr->stereo_mode,                 0);
      cfg->Read(wxT("compression"),                 &tr->compression);
      cfg->Read(wxT("timecodes"),                   &tr->timecodes);
      cfg->Read(wxT("track_name_was_present"),      &tr->track_name_was_present,      false);
      cfg->Read(wxT("appending"),                   &tr->appending,                   false);
      cfg->Read(wxT("user_defined"),                &tr->user_defined);
      cfg->Read(wxT("packetizer"),                  &tr->packetizer);
      cfg->Read(wxT("num_entries"),                 &tr->num_entries);

      tr->source = files.size();
      if (track_order.Length() > 0)
        track_order += wxT(",");
      track_order += wxString::Format(wxUCS(format), static_cast<unsigned int>(files.size()), tr->id);

      fi->tracks.push_back(tr);
      cfg->SetPath(wxT(".."));
    }

    int num_attached_files;
    if (cfg->Read(wxT("number_of_attached_files"), &num_attached_files) && (0 < num_attached_files)) {
      int i;
      for (i = 0; num_attached_files > i; ++i) {
        cfg->SetPath(wxString::Format(wxT("attached_file %d"), i));

        mmg_attached_file_cptr a = mmg_attached_file_cptr(new mmg_attached_file_t);

        cfg->Read(wxT("id"),          &a->id);
        cfg->Read(wxT("size"),        &a->size);
        cfg->Read(wxT("name"),        &a->name);
        cfg->Read(wxT("mime_type"),   &a->mime_type);
        cfg->Read(wxT("description"), &a->description);
        cfg->Read(wxT("enabled"),     &a->enabled, false);

        cfg->SetPath(wxT(".."));

        if (0 != a->id) {
          a->source = fi.get();
          fi->attached_files.push_back(a);
        }
      }
    }

    if (!fi->tracks.empty()) {
      wxFileName fn{fi->file_name};
      lb_input_files->Append(wxString::Format(wxT("%s%s (%s)"), fi->appending ? wxT("++> ") : wxEmptyString, fn.GetFullName().c_str(), fn.GetPath().c_str()));
      files.push_back(fi);
    }

    cfg->SetPath(wxT(".."));
  }

  wxString s = wxEmptyString;
  if (!cfg->Read(wxT("track_order"), &s) || (s.length() == 0))
    s = track_order;
  strip(s);
  if (s.length() > 0) {
    std::vector<wxString> entries = split(s, (wxString)wxT(","));
    size_t i;
    for (i = 0; i < entries.size(); i++) {
      std::vector<wxString> pair = split(entries[i], (wxString)wxT(":"));
      if (pair.size() != 2)
        wxdie(Z("The job file could not have been parsed correctly.\n"
                "Either it is invalid / damaged, or you've just found\n"
                "a bug in mmg. Please report this to the author\n"
                "Moritz Bunkus <moritz@bunkus.org>\n\n"
                "(Problem occured in tab_input::load(), #1)"));
      long tidx;
      if (!pair[0].ToLong(&fidx) || !pair[1].ToLong(&tidx) || (fidx >= static_cast<long>(files.size())))
        wxdie(Z("The job file could not have been parsed correctly.\n"
                "Either it is invalid / damaged, or you've just found\n"
                "a bug in mmg. Please report this to the author\n"
                "Moritz Bunkus <moritz@bunkus.org>\n\n"
                "(Problem occured in tab_input::load(), #2)"));
      bool found = false;
      size_t j;
      for (j = 0; j < files[fidx]->tracks.size(); j++)
        if (files[fidx]->tracks[j]->id == tidx) {
          found = true;
          tidx = j;
        }
      if (!found)
        wxdie(Z("The job file could not have been parsed correctly.\n"
                "Either it is invalid / damaged, or you've just found\n"
                "a bug in mmg. Please report this to the author\n"
                "Moritz Bunkus <moritz@bunkus.org>\n\n"
                "(Problem occured in tab_input::load(), #3)"));
      mmg_track_cptr &t = files[fidx]->tracks[tidx];
      tracks.push_back(t.get());

      clb_tracks->Append(t->create_label());
      clb_tracks->Check(i, t->enabled);
    }
  }
  st_tracks->Enable(!tracks.empty());
  clb_tracks->Enable(!tracks.empty());

  dont_copy_values_now = false;

  enable_file_controls();
}

bool
tab_input::validate_settings() {
  // Appending a file to itself is not allowed.
  unsigned int tidx;
  for (tidx = 2; tidx < tracks.size(); tidx++) {
    if (!tracks[tidx]->enabled || !tracks[tidx]->appending)
      continue;

    auto to_idx = tidx - 1;
    while ((0 < to_idx) && !tracks[to_idx]->enabled)
      --to_idx;

    if (tracks[to_idx]->appending && tracks[to_idx]->enabled && (tracks[to_idx]->source == tracks[tidx]->source)) {
      wxString err;

      err.Printf(Z("Appending a track from a file to another track from the same file is not allowed. "
                   "This is the case for tracks number %u and %u."),
                 to_idx + 1, tidx + 1);
      wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);

      return false;
    }
  }

  unsigned int fidx;
  for (fidx = 0; fidx < files.size(); fidx++) {
    mmg_file_cptr &f = files[fidx];

    for (tidx = 0; tidx < f->tracks.size(); tidx++) {
      mmg_track_cptr &t = f->tracks[tidx];
      if (!t->enabled)
        continue;

      std::string format;
      fix_format("%lld", format);
      wxString sid;
      sid.Printf(wxU(format), t->id);

      std::string s = wxMB(t->delay);
      strip(s);
      int dummy_i;
      if ((s.length() > 0) && !parse_number(s, dummy_i)) {
        wxString err;
        err.Printf(Z("The delay setting for track nr. %s in file '%s' is invalid."), sid.c_str(), f->file_name.c_str());
        wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

      wxString str = t->stretch;
      strip(str);
      if (str.length() > 0) {
        wxRegEx re_stretch(wxT("^[[:digit:]]+([,\\.][[:digit:]]+)?(/[[:digit:]]+([,\\.][[:digit:]]+)?)?$"), wxRE_ICASE);
        if (!re_stretch.Matches(str)) {
          wxString err;
          err.Printf(Z("The stretch setting for track nr. %s in file '%s' is invalid."), sid.c_str(), f->file_name.c_str());
          wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
          return false;
        }
      }

      s = wxMB(t->fourcc);
      strip(s);
      if ((s.length() > 0) && (s.length() != 4)) {
        wxString err;
        err.Printf(Z("The FourCC setting for track nr. %s in file '%s' is not exactly four characters long."), sid.c_str(), f->file_name.c_str());
        wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

      str = t->fps;
      strip(str);
      if (str.length() > 0) {
        wxRegEx re_fps1(wxT("^[[:digit:]]+\\.?[[:digit:]]*[ip]?$"));
        wxRegEx re_fps2(wxT("^[[:digit:]]+/[[:digit:]]+[ip]?$"));
        if (!re_fps1.Matches(str) && !re_fps2.Matches(str)) {
          wxString err;
          err.Printf(Z("The FPS setting for track nr. %s in file '%s' is invalid."), sid.c_str(), f->file_name.c_str());
          wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
          return false;
        }
      }

      s = wxMB(t->aspect_ratio);
      strip(s);
      if (s.length() > 0) {
        bool dot_present = false;
        size_t i         = 0;
        bool ok          = true;
        while (i < s.length()) {
          if (isdigit(s[i]) || (!dot_present && ((s[i] == '.') || (s[i] == ',')))) {
            if ((s[i] == '.') || (s[i] == ','))
              dot_present = true;
            i++;
          } else {
            ok = false;
            break;
          }
        }

        if (!ok) {
          dot_present = false;
          i           = 0;
          ok          = true;
          while (i < s.length()) {
            if (isdigit(s[i]) ||
                (!dot_present && (s[i] == '/'))) {
              if (s[i] == '/')
                dot_present = true;
              i++;
            } else {
              ok = false;
              break;
            }
          }
        }

        if (!ok) {
          wxString err;
          err.Printf(Z("The aspect ratio setting for track nr. %s in file '%s' is invalid."), sid.c_str(), f->file_name.c_str());
          wxMessageBox(err, Z("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
          return false;
        }
      }
    }
  }

  return true;
}

void
tab_input::handle_webm_mode(bool enabled) {
  if (enabled) {
    unsigned int i;
    for (i = 0; tracks.size() > i; ++i) {
      if (tracks[i]->is_webm_compatible())
        continue;

      tracks[i]->enabled = false;
      clb_tracks->Check(i, false);
    }
  }
}

wxString
tab_input::check_for_and_handle_playlist_file(wxString const &file_name,
                                              wxArrayString &original_output) {
  if (SDP_NEVER == mdlg->options.scan_directory_for_playlists)
    return file_name;

  auto properties = parse_properties(wxT("container:"), original_output);

  if (properties[wxT("playlist")] != wxT("1"))
    return file_name;

  auto dir_path = wxFileName{file_name}.GetPath();
  wxDir dir{dir_path};

  if (!dir.IsOpened())
    return file_name;

  std::vector<wxString> other_files;
  wxString other_file_name;

  auto wanted_ext = wxFileName{file_name}.GetExt();
  auto full_name  = wxFileName{file_name}.GetFullName();
  bool cont       = dir.GetFirst(&other_file_name, wxEmptyString, wxDIR_FILES);
  while (cont) {
    if ((full_name != other_file_name) && (wxFileName{other_file_name}.GetExt() == wanted_ext))
      other_files.push_back(dir_path + wxFileName::GetPathSeparator() + other_file_name);
    cont = dir.GetNext(&other_file_name);
  }

  if (2 > other_files.size())
    return file_name;

  if (SDP_ALWAYS_ASK == mdlg->options.scan_directory_for_playlists) {
    ask_scan_for_playlists_dlg dlg{this, other_files.size()};
    if (wxID_CANCEL == dlg.ShowModal())
      return file_name;
  }

  scanning_for_playlists_dlg dlg{this, file_name, original_output, other_files};
  if (dlg.scan() == wxID_CANCEL)
    return wxEmptyString;

  auto playlists = dlg.get_playlists();
  if (playlists.empty())
    return wxEmptyString;

  int idx;
  if (1 == playlists.size())
    idx = 0;

  else {
    select_scanned_file_dlg select_dlg{this, playlists, file_name};
    if (select_dlg.ShowModal() == wxID_CANCEL)
      return wxEmptyString;

    idx = select_dlg.get_selected_playlist_idx();
  }

  auto &playlist  = playlists[idx];
  original_output = playlist->output;

  return playlist->file_name;
}

std::map<wxString, wxString>
tab_input::parse_properties(wxString const &wanted_line,
                            wxArrayString const &output)
  const {
  std::map<wxString, wxString> properties;
  wxString info;
  int pos_wanted, pos_properties;

  for (size_t idx = 0, count = output.GetCount(); idx < count; ++idx)
    if (wxNOT_FOUND != (pos_wanted = output[idx].Find(wanted_line))) {
      if (wxNOT_FOUND != (pos_properties = output[idx].Find(wxT("["))))
        info = output[idx].Mid(pos_wanted + wanted_line.Length()).AfterFirst(wxT('[')).BeforeLast(wxT(']'));
      break;
    }

  if (info.IsEmpty())
    return properties;

  for (auto &arg : split(info, wxU(" "))) {
    auto pair = split(arg, wxU(":"), 2);
    properties[pair[0]] = unescape(pair[1]);
  }

  return properties;
}

IMPLEMENT_CLASS(tab_input, wxPanel);
BEGIN_EVENT_TABLE(tab_input, wxPanel)
  EVT_BUTTON(ID_B_ADDFILE,          tab_input::on_add_file)
  EVT_BUTTON(ID_B_REMOVEFILE,       tab_input::on_remove_file)
  EVT_BUTTON(ID_B_REMOVE_ALL_FILES, tab_input::on_remove_all_files)
  EVT_BUTTON(ID_B_APPENDFILE,       tab_input::on_append_file)
  EVT_BUTTON(ID_B_ADDITIONAL_PARTS, tab_input::on_additional_parts)
  EVT_BUTTON(ID_B_TRACKUP,          tab_input::on_move_track_up)
  EVT_BUTTON(ID_B_TRACKDOWN,        tab_input::on_move_track_down)

  EVT_LISTBOX(ID_LB_INPUTFILES,     tab_input::on_file_selected)
  EVT_LISTBOX(ID_CLB_TRACKS,        tab_input::on_track_selected)
  EVT_CHECKLISTBOX(ID_CLB_TRACKS,   tab_input::on_track_enabled)

  EVT_TIMER(ID_T_INPUTVALUES,       tab_input::on_value_copy_timer)

  EVT_COMMAND(tab_input::dropped_files_added, tab_input::ms_event, tab_input::on_dropped_files_added)
END_EVENT_TABLE();
