/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "input" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <errno.h>

#include <algorithm>

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/file.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statline.h"

#include "common.h"
#include "extern_data.h"
#include "iso639.h"
#include "mkvmerge.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "tab_input.h"
#include "tab_global.h"

using namespace std;

wxArrayString sorted_iso_codes;
wxArrayString sorted_charsets;
bool title_was_present = false;

class input_drop_target_c: public wxFileDropTarget {
private:
  tab_input *owner;
public:
  input_drop_target_c(tab_input *n_owner):
    owner(n_owner) {}
  virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &files) {
    int i;

    for (i = 0; i < files.Count(); i++)
      owner->add_file(files[i], false);

    return true;
  }
};

#if defined(SYS_WINDOWS)
#define GROUPSPACING 10
#else
#define GROUPSPACING 5
#endif

tab_input::tab_input(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  uint32_t i, j;
  bool found;
  wxString language;
  wxArrayString popular_languages;
  wxFlexGridSizer *siz_fg;
  wxStaticBoxSizer *siz_toptions;
  wxBoxSizer *siz_line, *siz_column, *siz_all, *siz_box;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(0, 5, 0, 0, 0);
  siz_all->Add(new wxStaticText(this, wxID_STATIC, wxT("Input files:")),
               0, wxALIGN_LEFT | wxLEFT, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  lb_input_files = new wxListBox(this, ID_LB_INPUTFILES);
  siz_line->Add(lb_input_files, 1, wxGROW, 0);

  siz_column = new wxBoxSizer(wxVERTICAL);
  b_add_file = new wxButton(this, ID_B_ADDFILE, wxT("add"), wxDefaultPosition,
                            wxSize(50, -1));
  b_remove_file = new wxButton(this, ID_B_REMOVEFILE, wxT("remove"),
                               wxDefaultPosition, wxSize(50, -1));
  b_remove_file->Enable(false);
  b_append_file = new wxButton(this, ID_B_APPENDFILE, wxT("append"),
                               wxDefaultPosition, wxSize(60, -1));
  b_append_file->Enable(false);

  siz_column->Add(b_add_file, 0, wxBOTTOM, 10);
  siz_column->Add(b_remove_file, 0, 0, 0);
  siz_line->Add(siz_column, 0, wxLEFT, 10);

  siz_column = new wxBoxSizer(wxVERTICAL);
  siz_column->Add(b_append_file, 0, wxBOTTOM, 10);
  siz_line->Add(siz_column, 0, wxLEFT, 10);

  siz_all->Add(siz_line, 2, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, GROUPSPACING, 0, 0, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  st_file_options = new wxStaticText(this, wxID_STATIC, wxT("File options:"));
  st_file_options->Enable(false);
  siz_line->Add(st_file_options, 0, wxALIGN_CENTER_VERTICAL, 0);
  cb_no_chapters = new wxCheckBox(this, ID_CB_NOCHAPTERS, wxT("No chapters"));
  cb_no_chapters->SetValue(false);
  cb_no_chapters->SetToolTip(TIP("Do not copy chapters from this file. Only "
                                 "applies to Matroska files."));
  cb_no_chapters->Enable(false);
  siz_line->Add(cb_no_chapters, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
  cb_no_attachments = new wxCheckBox(this, ID_CB_NOATTACHMENTS,
                                     wxT("No attachments"));
  cb_no_attachments->SetValue(false);
  cb_no_attachments->SetToolTip(TIP("Do not copy attachments from this file. "
                                    "Only applies to Matroska files."));
  cb_no_attachments->Enable(false);
  siz_line->Add(cb_no_attachments, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
  cb_no_tags = new wxCheckBox(this, ID_CB_NOTAGS, wxT("No tags"));
  cb_no_tags->SetValue(false);
  cb_no_tags->SetToolTip(TIP("Do not copy tags from this file. Only "
                             "applies to Matroska files."));
  cb_no_tags->Enable(false);
  siz_line->Add(cb_no_tags, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
  siz_all->Add(siz_line, 0, wxLEFT, 10);

  siz_all->Add(0, GROUPSPACING, 0, 0, 0);

  st_tracks = new wxStaticText(this, wxID_STATIC, wxT("Tracks:"));
  st_tracks->Enable(false);
  siz_all->Add(st_tracks, 0, wxALIGN_LEFT | wxLEFT, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_column = new wxBoxSizer(wxVERTICAL);
  clb_tracks = new wxCheckListBox(this, ID_CLB_TRACKS);
  clb_tracks->Enable(false);
  siz_line->Add(clb_tracks, 1, wxGROW | wxALIGN_TOP, 0);
  b_track_up = new wxButton(this, ID_B_TRACKUP, wxT("up"), wxDefaultPosition,
                            wxSize(50, -1));
  b_track_up->Enable(false);
  siz_column->Add(b_track_up, 0, wxBOTTOM, 10);
  b_track_down = new wxButton(this, ID_B_TRACKDOWN, wxT("down"),
                              wxDefaultPosition, wxSize(50, -1));
  b_track_down->Enable(false);
  siz_column->Add(0, 1, 0, wxGROW, 0);
  siz_column->Add(b_track_down, 0, 0, 0);
  siz_line->Add(siz_column, 0, wxLEFT, 10);
  siz_all->Add(siz_line, 3, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(0, GROUPSPACING, 0, 0, 0);

  sb_track_options = new wxStaticBox(this, wxID_STATIC,
                                     wxT("Track options"));
  sb_track_options->Enable(false);
  siz_toptions = new wxStaticBoxSizer(sb_track_options, wxVERTICAL);

  siz_fg = new wxFlexGridSizer(4);
  siz_fg->AddGrowableCol(1);
  siz_fg->AddGrowableCol(3);

  for (i = 0; i < siz_fg->GetCols(); i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(0);

  st_language = new wxStaticText(this, wxID_STATIC, wxT("Language:"));
  st_language->Enable(false);
  siz_fg->Add(st_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

  if (sorted_iso_codes.Count() == 0) {
    for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++) {
      if (iso639_languages[i].english_name == NULL)
        language = wxU(iso639_languages[i].iso639_2_code);
      else
        language.Printf(wxT("%s (%s)"),
                        wxUCS(iso639_languages[i].iso639_2_code),
                        wxUCS(iso639_languages[i].english_name));
      sorted_iso_codes.Add(language);
    }
    sorted_iso_codes.Sort();

    for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++) {
      if (!is_popular_language_code(iso639_languages[i].iso639_2_code))
        continue;
      for (j = 0, found = false; j < popular_languages.Count(); j++)
        if (extract_language_code(popular_languages[j]) ==
            wxU(iso639_languages[i].iso639_2_code)) {
          found = true;
          break;
        }
      if (!found) {
        language.Printf(wxT("%s (%s)"),
                        wxUCS(iso639_languages[i].iso639_2_code),
                        wxUCS(iso639_languages[i].english_name));
        popular_languages.Add(language);
      }
    }
    popular_languages.Sort();

    sorted_iso_codes.Insert(wxT("und (Undetermined)"), 0);
    sorted_iso_codes.Insert(wxT("---common---"), 1);
    for (i = 0; i < popular_languages.Count(); i++)
      sorted_iso_codes.Insert(popular_languages[i], i + 2);
    sorted_iso_codes.Insert(wxT("---all---"), i + 2);
  }

  cob_language =
    new wxComboBox(this, ID_CB_LANGUAGE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language->SetToolTip(TIP("Language for this track. Select one of the "
                               "ISO639-2 language codes."));
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language->Append(sorted_iso_codes[i]);
  cob_language->SetSizeHints(0, -1);
  siz_fg->Add(cob_language, 0, wxGROW | wxRIGHT, 15);

  st_delay = new wxStaticText(this, wxID_STATIC, wxT("Delay (in ms):"));
  st_delay->Enable(false);
  siz_fg->Add(st_delay, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  tc_delay = new wxTextCtrl(this, ID_TC_DELAY, wxT(""));
  tc_delay->SetToolTip(TIP("Delay this track by a couple of ms. Can be "
                           "negative. Only applies to audio and subtitle "
                           "tracks. Some audio formats cannot be delayed at "
                           "the moment."));
  tc_delay->SetSizeHints(0, -1);
  siz_fg->Add(tc_delay, 0, wxGROW, 0);

  for (i = 0; i < siz_fg->GetCols(); i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(2);

  st_track_name = new wxStaticText(this, wxID_STATIC, wxT("Track name:"));
  st_track_name->Enable(false);
  siz_fg->Add(st_track_name, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  tc_track_name = new wxTextCtrl(this, ID_TC_TRACKNAME, wxT(""));
  tc_track_name->SetToolTip(TIP("Name for this track, e.g. \"director's "
                                "comments\"."));
  tc_track_name->SetSizeHints(0, -1);
  siz_fg->Add(tc_track_name, 0, wxGROW | wxRIGHT, 15);

  st_stretch = new wxStaticText(this, wxID_STATIC, wxT("Stretch by:"));
  st_stretch->Enable(false);
  siz_fg->Add(st_stretch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  tc_stretch = new wxTextCtrl(this, ID_TC_STRETCH, wxT(""));
  tc_stretch->SetToolTip(TIP("Stretch the audio or subtitle track by a "
                             "factor. This entry can have two formats. It is "
                             "either a positive floating point number, or a "
                             "fraction like e.g. 1200/1253. Not all formats "
                             "can be stretched at the moment."));
  tc_stretch->SetSizeHints(0, -1);
  siz_fg->Add(tc_stretch, 0, wxGROW, 0);

  for (i = 0; i < siz_fg->GetCols(); i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(4);

  st_cues = new wxStaticText(this, wxID_STATIC, wxT("Cues:"));
  st_cues->Enable(false);
  siz_fg->Add(st_cues, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_cues =
    new wxComboBox(this, ID_CB_CUES, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_cues->SetToolTip(TIP("Selects for which blocks mkvmerge will produce "
                           "index entries ( = cue entries). \"default\" is a "
                           "good choice for almost all situations."));
  cob_cues->Append(wxT("default"));
  cob_cues->Append(wxT("only for I frames"));
  cob_cues->Append(wxT("for all frames"));
  cob_cues->Append(wxT("none"));
  cob_cues->SetSizeHints(0, -1);
  siz_fg->Add(cob_cues, 0, wxGROW | wxRIGHT, 15);

  st_sub_charset = new wxStaticText(this, wxID_STATIC,
                                    wxT("Subtitle charset:"));
  st_sub_charset->Enable(false);
  siz_fg->Add(st_sub_charset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_sub_charset =
    new wxComboBox(this, ID_CB_SUBTITLECHARSET, wxT(""),
                   wxDefaultPosition, wxDefaultSize, 0, NULL,
                   wxCB_DROPDOWN | wxCB_READONLY);
  cob_sub_charset->SetToolTip(TIP("Selects the character set a subtitle file "
                                  "was written with. Only needed for non-UTF "
                                  "files that mkvmerge does not detect "
                                  "correctly."));
  cob_sub_charset->Append(wxT("default"));

  if (sorted_charsets.Count() == 0) {
    for (i = 0; sub_charsets[i] != NULL; i++)
      sorted_charsets.Add(wxU(sub_charsets[i]));
    sorted_charsets.Sort();
  }

  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_sub_charset->Append(sorted_charsets[i]);
  cob_sub_charset->SetSizeHints(0, -1);
  siz_fg->Add(cob_sub_charset, 0, wxGROW, 0);

  for (i = 0; i < siz_fg->GetCols(); i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(6);

  st_fourcc = new wxStaticText(this, -1, wxT("FourCC:"));
  st_fourcc->Enable(false);
  siz_fg->Add(st_fourcc, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_fourcc =
    new wxComboBox(this, ID_CB_FOURCC, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_fourcc->Append(wxT(""));
  cob_fourcc->Append(wxT("DIVX"));
  cob_fourcc->Append(wxT("DIV3"));
  cob_fourcc->Append(wxT("DX50"));
  cob_fourcc->Append(wxT("MP4V"));
  cob_fourcc->Append(wxT("XVID"));
  cob_fourcc->SetToolTip(TIP("Forces the FourCC of the video track to this "
                             "value. Note that this only works for video "
                             "tracks that use the AVI compatibility mode "
                             "or for QuickTime video tracks. This option "
                             "CANNOT be used to change Matroska's CodecID."));
  cob_fourcc->SetSizeHints(0, -1);
  siz_fg->Add(cob_fourcc, 0, wxGROW | wxRIGHT, 15);

  st_compression = new wxStaticText(this, -1, wxT("Compression:"));
  st_compression->Enable(false);
  siz_fg->Add(st_compression, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_compression =
    new wxComboBox(this, ID_CB_COMPRESSION, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_compression->Append(wxT(""));
  cob_compression->Append(wxT("none"));
  cob_compression->Append(wxT("zlib"));
  if (capabilities[wxT("BZ2")] == wxT("true"))
    cob_compression->Append(wxT("bz2"));
  if (capabilities[wxT("LZO")] == wxT("true"))
    cob_compression->Append(wxT("lzo"));
  cob_compression->SetToolTip(TIP("Sets the compression used for VobSub "
                                  "subtitles. If nothing is chosen then the "
                                  "VobSubs will be automatically compressed "
                                  "with zlib. 'none' results is files that "
                                  "are a lot larger."));
  cob_compression->SetSizeHints(0, -1);
  siz_fg->Add(cob_compression, 0, wxGROW, 0);
  siz_toptions->Add(siz_fg, 4, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_box = new wxBoxSizer(wxVERTICAL);
  siz_box->Add(0, 1, 1, wxGROW, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  rb_aspect_ratio =
    new wxRadioButton(this, ID_RB_ASPECTRATIO, wxT("Aspect ratio:"),
                      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_aspect_ratio->SetValue(true);
  siz_line->Add(rb_aspect_ratio, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_aspect_ratio =
    new wxComboBox(this, ID_CB_ASPECTRATIO, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_aspect_ratio->Append(wxT(""));
  cob_aspect_ratio->Append(wxT("4/3"));
  cob_aspect_ratio->Append(wxT("1.66"));
  cob_aspect_ratio->Append(wxT("16/9"));
  cob_aspect_ratio->Append(wxT("1.85"));
  cob_aspect_ratio->Append(wxT("2.00"));
  cob_aspect_ratio->Append(wxT("2.21"));
  cob_aspect_ratio->Append(wxT("2.35"));
  cob_aspect_ratio->SetToolTip(TIP("Sets the display aspect ratio of the "
                                   "track. The format can be either 'a/b' in "
                                   "which case both numbers must be integer "
                                   "(e.g. 16/9) or just a single floting "
                                   "point number 'f' (e.g. 2.35)."));
  siz_line->Add(cob_aspect_ratio, 3, wxGROW | wxRIGHT, 10);

  st_or = new wxStaticText(this, wxID_STATIC, wxT("or"));
  st_or->Enable(false);
  siz_line->Add(st_or, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

  rb_display_dimensions =
    new wxRadioButton(this, ID_RB_DISPLAYDIMENSIONS,
                      wxT("Display width/height:"));
  rb_display_dimensions->SetValue(false);
  siz_line->Add(rb_display_dimensions, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT,
                10);
  tc_display_width = new wxTextCtrl(this, ID_TC_DISPLAYWIDTH, wxT(""));
  tc_display_width->SetToolTip(TIP("Sets the display width of the track."
                                   "The height must be set as well, or this "
                                   "field will be ignored."));
  siz_line->Add(tc_display_width, 2, wxGROW, 0);
  st_x = new wxStaticText(this, wxID_STATIC, wxT("x"));
  st_x->Enable(false);
  siz_line->Add(st_x, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_display_height = new wxTextCtrl(this, ID_TC_DISPLAYHEIGHT, wxT(""));
  tc_display_height->SetToolTip(TIP("Sets the display height of the track."
                                    "The width must be set as well, or this "
                                    "field will be ignored."));
  siz_line->Add(tc_display_height, 2, wxGROW, 0);
  siz_box->Add(siz_line, 0, wxGROW, 0);

  siz_box->Add(0, 1, 1, wxGROW, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  cb_default =
    new wxCheckBox(this, ID_CB_MAKEDEFAULT, wxT("Make default track"));
  cb_default->SetValue(false);
  cb_default->SetToolTip(TIP("Make this track the default track for its type "
                             "(audio, video, subtitles). Players should "
                             "prefer tracks with the default track flag "
                             "set."));
  siz_line->Add(cb_default, 0, 0, 0);
  cb_aac_is_sbr =
    new wxCheckBox(this, ID_CB_AACISSBR, wxT("AAC is SBR/HE-AAC/AAC+"));
  cb_aac_is_sbr->SetValue(false);
  cb_aac_is_sbr->SetToolTip(TIP("This track contains SBR AAC/HE-AAC/AAC+ data."
                                " Only needed for AAC input files, because SBR"
                                " AAC cannot be detected automatically for "
                                "these files. Not needed for AAC tracks read "
                                "from MP4 or Matroska files."));
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(cb_aac_is_sbr, 0, 0, 0);
  siz_box->Add(siz_line, 0, wxGROW, 0);
  siz_toptions->Add(siz_box, 2, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_fg = new wxFlexGridSizer(3);
  siz_fg->AddGrowableCol(1);
  for (i = 0; i < 3; i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(0);

  st_tags = new wxStaticText(this, wxID_STATIC, wxT("Tags:"));
  st_tags->Enable(false);
  siz_fg->Add(st_tags, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  tc_tags = new wxTextCtrl(this, ID_TC_TAGS, wxT(""));
  siz_fg->Add(tc_tags, 1, wxGROW | wxRIGHT, 10);
  b_browse_tags = new wxButton(this, ID_B_BROWSETAGS, wxT("Browse"));
  siz_fg->Add(b_browse_tags, 0, wxALIGN_CENTER_VERTICAL, 10);

  for (i = 0; i < 3; i++)
    siz_fg->Add(0, 1, 1, wxGROW, 0);
  siz_fg->AddGrowableRow(2);

  st_timecodes = new wxStaticText(this, wxID_STATIC, wxT("Timecodes:"));
  st_timecodes->Enable(false);
  siz_fg->Add(st_timecodes, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  tc_timecodes = new wxTextCtrl(this, ID_TC_TIMECODES, wxT(""));
  tc_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes from an "
                               "external text file. This feature is a very "
                               "advanced feature. Almost all users should "
                               "leave this entry empty."));
  siz_fg->Add(tc_timecodes, 1, wxGROW | wxRIGHT, 10);
  b_browse_timecodes = new wxButton(this, ID_B_BROWSE_TIMECODES,
                                    wxT("Browse"));
  b_browse_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes "
                                     "from an external text file. This "
                                     "feature is a very advanced feature. "
                                     "Almost all users should leave this "
                                     "entry empty."));
  siz_fg->Add(b_browse_timecodes, 0, wxALIGN_CENTER_VERTICAL);
  siz_toptions->Add(siz_fg, 2, wxGROW | wxLEFT | wxRIGHT, 10);
  siz_all->Add(siz_toptions, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  SetSizer(siz_all);

  no_track_mode();
  selected_file = -1;
  selected_track = -1;

  dont_copy_values_now = false;
  value_copy_timer.SetOwner(this, ID_T_INPUTVALUES);
  value_copy_timer.Start(333);

  SetDropTarget(new input_drop_target_c(this));
}

void
tab_input::no_track_mode() {
  sb_track_options->Enable(false);
  st_language->Enable(false);
  st_delay->Enable(false);
  st_track_name->Enable(false);
  st_stretch->Enable(false);
  st_cues->Enable(false);
  st_sub_charset->Enable(false);
  st_fourcc->Enable(false);
  st_compression->Enable(false);
  st_or->Enable(false);
  st_x->Enable(false);
  st_tags->Enable(false);
  st_timecodes->Enable(false);

  cob_language->Enable(false);
  tc_delay->Enable(false);
  tc_track_name->Enable(false);
  tc_stretch->Enable(false);
  cob_cues->Enable(false);
  cob_sub_charset->Enable(false);
  cb_default->Enable(false);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(false);
  b_browse_tags->Enable(false);
  cob_aspect_ratio->Enable(false);
  tc_display_width->Enable(false);
  tc_display_height->Enable(false);
  rb_aspect_ratio->Enable(false);
  rb_display_dimensions->Enable(false);
  cob_fourcc->Enable(false);
  cob_compression->Enable(false);
  tc_timecodes->Enable(false);
  b_browse_timecodes->Enable(false);
}

void
tab_input::audio_track_mode(wxString ctype,
                            bool appending) {
  wxString lctype;

  sb_track_options->Enable(true);
  st_language->Enable(true && !appending);
  st_delay->Enable(true);
  st_track_name->Enable(true && !appending);
  st_stretch->Enable(true);
  st_cues->Enable(true && !appending);
  st_sub_charset->Enable(false);
  st_fourcc->Enable(false);
  st_compression->Enable(false);
  st_or->Enable(false);
  st_x->Enable(false);
  st_tags->Enable(true && !appending);
  st_timecodes->Enable(true && !appending);

  lctype = ctype.Lower();
  cob_language->Enable(true && !appending);
  tc_delay->Enable(true);
  tc_track_name->Enable(true && !appending);
  tc_stretch->Enable(true);
  cob_cues->Enable(true && !appending);
  cob_sub_charset->Enable(false);
  cb_default->Enable(true && !appending);
  cb_aac_is_sbr->Enable(((lctype.Find(wxT("aac")) >= 0) ||
                         (lctype.Find(wxT("mp4a")) >= 0)));
  tc_tags->Enable(true && !appending);
  b_browse_tags->Enable(true && !appending);
  cob_aspect_ratio->Enable(false);
  tc_display_width->Enable(false);
  tc_display_height->Enable(false);
  rb_aspect_ratio->Enable(false);
  rb_display_dimensions->Enable(false);
  cob_fourcc->Enable(false);
  cob_compression->Enable(false);
  tc_timecodes->Enable(true && !appending);
  b_browse_timecodes->Enable(true && !appending);
}

void
tab_input::video_track_mode(wxString,
                            bool appending) {
  sb_track_options->Enable(true && !appending);
  st_language->Enable(true && !appending);
  st_delay->Enable(false);
  st_track_name->Enable(true && !appending);
  st_stretch->Enable(false);
  st_cues->Enable(true && !appending);
  st_sub_charset->Enable(false);
  st_fourcc->Enable(true && !appending);
  st_compression->Enable(false);
  st_or->Enable(true && !appending);
  st_x->Enable(true && !appending);
  st_tags->Enable(true && !appending);
  st_timecodes->Enable(true && !appending);

  cob_language->Enable(true && !appending);
  tc_delay->Enable(false);
  tc_track_name->Enable(true && !appending);
  tc_stretch->Enable(false);
  cob_cues->Enable(true && !appending);
  cob_sub_charset->Enable(false);
  cb_default->Enable(true && !appending);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(true && !appending);
  b_browse_tags->Enable(true && !appending);
  cob_aspect_ratio->Enable(true && !appending);
  rb_aspect_ratio->Enable(true && !appending);
  rb_display_dimensions->Enable(true && !appending);
  cob_fourcc->Enable(true && !appending);
  cob_compression->Enable(false);
  tc_timecodes->Enable(true && !appending);
  b_browse_timecodes->Enable(true && !appending);
}

void
tab_input::subtitle_track_mode(wxString ctype,
                               bool appending) {
  wxString lctype;

  sb_track_options->Enable(true);
  st_language->Enable(true && !appending);
  st_delay->Enable(true);
  st_track_name->Enable(true && !appending);
  st_stretch->Enable(true);
  st_cues->Enable(true && !appending);
  st_sub_charset->Enable(true);
  st_fourcc->Enable(false);
  st_compression->Enable(true && !appending);
  st_or->Enable(false);
  st_x->Enable(false);
  st_tags->Enable(true && !appending);
  st_timecodes->Enable(true && !appending);

  lctype = ctype.Lower();
  cob_language->Enable(true && !appending);
  tc_delay->Enable(true);
  tc_track_name->Enable(true && !appending);
  tc_stretch->Enable(true);
  cob_cues->Enable(true && !appending);
  cob_sub_charset->Enable(lctype.Find(wxT("vobsub")) < 0);
  cb_default->Enable(true && !appending);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(true && !appending);
  b_browse_tags->Enable(true && !appending);
  cob_aspect_ratio->Enable(false);
  tc_display_width->Enable(false);
  tc_display_height->Enable(false);
  rb_aspect_ratio->Enable(false);
  rb_display_dimensions->Enable(false);
  cob_fourcc->Enable(false);
  cob_compression->Enable((lctype.Find(wxT("vobsub")) >= 0) && !appending);
  tc_timecodes->Enable(true && !appending);
  b_browse_timecodes->Enable(true && !appending);
}

void
tab_input::enable_ar_controls(mmg_track_t *track) {
  bool ar_enabled;

  ar_enabled = !track->display_dimensions_selected;
  cob_aspect_ratio->Enable(ar_enabled);
  tc_display_width->Enable(!ar_enabled);
  tc_display_height->Enable(!ar_enabled);
  rb_aspect_ratio->SetValue(ar_enabled);
  rb_display_dimensions->SetValue(!ar_enabled);
}

void
tab_input::select_file(bool append) {
  static struct { wxChar *title, *extensions; } file_types[] = {
    { wxT("AAC (Advanced Audio Coding)"), wxT("aac m4a mp4") },
    { wxT("A/52 (aka AC3)"), wxT("ac3") },
    { wxT("AVI (Audio/Video Interleaved)"), wxT("avi") },
    { wxT("DTS (Digital Theater System)"), wxT("dts") },
    { wxT("FLAC (Free Lossless Audio Codec)"), wxT("flac ogg") },
    { wxT("MP4 audio/video files"), wxT("mp4") },
    { wxT("MPEG audio files"), wxT("mp2 mp3") },
    { wxT("MPEG program streams"), wxT("mpg mpeg m2v") },
    { wxT("MPEG video elementary streams"), wxT("m1v m2v") },
    { wxT("Matroska audio/video files"), wxT("mka mks mkv") },
    { wxT("QuickTime audio/video files"), wxT("mov") },
    { wxT("Ogg/OGM audio/video files"), wxT("ogg ogm") },
    { wxT("RealMedia audio/video files"), wxT("ra ram rm rmvb rv") },
    { wxT("SRT text subtitles"), wxT("srt") },
    { wxT("SSA/ASS text subtitles"), wxT("ass ssa") },
    { wxT("TTA (The lossless True Audio codec)"), wxT("tta") },
    { wxT("USF text subtitles"), wxT("usf xml") },
    { wxT("VobSub subtitles"), wxT("idx") },
    { wxT("WAVE (uncompressed PCM audio)"), wxT("wav") },
    { wxT("WAVPACK v4 audio"), wxT("wv") },
    { wxT("VobButtons"), wxT("btn") },
    { NULL, NULL}
  };
  wxString media_files, rest, a_exts;
  vector<wxString> all_extensions;
  int ft, ae;

  for (ft = 0; file_types[ft].title != NULL; ft++) {
    vector<wxString> extensions;
    wxString s_exts;
    int e;

    if ((wxString(file_types[ft].title).Find(wxT("FLAC")) >= 0) &&
        (capabilities[wxT("FLAC")] != wxT("true")))
      continue;

    extensions = split(wxString(file_types[ft].extensions), wxU(" "));
    for (e = 0; e < extensions.size(); e++) {
      bool found;

      if (s_exts.Length() > 0)
        s_exts += wxT(";");
      s_exts += wxString::Format(wxT("*.%s"), extensions[e].c_str());

      found = false;
      for (ae = 0; ae < all_extensions.size(); ae++)
        if (all_extensions[ae] == extensions[e]) {
          found = true;
          break;
        }
      if (!found)
        all_extensions.push_back(extensions[e]);
    }

    rest += wxString::Format(wxT("|%s (%s)|%s"), file_types[ft].title,
                             s_exts.c_str(), s_exts.c_str());
  }

  sort(all_extensions.begin(), all_extensions.end());
  for (ae = 0; ae < all_extensions.size(); ae++) {
    if (a_exts.Length() > 0)
      a_exts += wxT(";");
    a_exts += wxString::Format(wxT("*.%s"), all_extensions[ae].c_str());
  }

  media_files = wxT("All supported media files|");
  media_files += a_exts + rest;
  media_files += wxT("|");
  media_files += wxT(ALLFILES);
  wxFileDialog dlg(NULL,
                   append ? wxT("Choose an input file to append") :
                   wxT("Choose an input file to add"),
                   last_open_dir, wxT(""), media_files, wxOPEN | wxMULTIPLE);

  if(dlg.ShowModal() == wxID_OK) {
    wxArrayString files;
    int i;

    last_open_dir = dlg.GetDirectory();
    dlg.GetPaths(files);
    for (i = 0; i < files.Count(); i++)
      add_file(files[i], append);
  }
}

void
tab_input::on_add_file(wxCommandEvent &evt) {
  select_file(false);
}

void
tab_input::on_append_file(wxCommandEvent &evt) {
  select_file(true);
}

void
tab_input::add_file(const wxString &file_name,
                    bool append) {
  mmg_file_t file;
  wxString name, command, id, type, exact, video_track_name, opt_file_name;
  wxArrayString output, errors;
  vector<wxString> args, pair;
  int result, pos, new_file_pos;
  unsigned int i, k;
  wxFile *opt_file;
  string arg_utf8;
  bool default_video_track_found, default_audio_track_found;
  bool default_subtitle_track_found;

  opt_file_name.Printf(wxT("%smmg-mkvmerge-options-%d-%d"),
                       get_temp_dir().c_str(),
                       (int)wxGetProcessId(), (int)time(NULL));
  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    opt_file = new wxFile(opt_file_name, wxFile::write);
    opt_file->Write(utf8_bom, 3);
  } catch (...) {
    wxString error;
    error.Printf(wxT("Could not create a temporary file for mkvmerge's "
                     "command line option called '%s' (error code %d, "
                     "%s)."), opt_file_name.c_str(), errno,
                 wxUCS(strerror(errno)));
    wxMessageBox(error, wxT("File creation failed"), wxOK | wxCENTER |
                 wxICON_ERROR);
    return;
  }
  opt_file->Write(wxT("--output-charset\nUTF-8\n--identify-verbose\n"));
  arg_utf8 = to_utf8(file_name);
  opt_file->Write(arg_utf8.c_str(), arg_utf8.length());
  opt_file->Write(wxT("\n"));
  delete opt_file;

  command = wxT("\"") + mkvmerge_path + wxT("\" \"@") + opt_file_name +
    wxT("\"");

  wxLogMessage(wxT("identify 1: command: ``%s''"), command.c_str());

  result = wxExecute(command, output, errors);

  wxLogMessage(wxT("identify 1: result: %d"), result);
  for (i = 0; i < output.Count(); i++)
    wxLogMessage(wxT("identify 1: output[%d]: ``%s''"), i, output[i].c_str());
  for (i = 0; i < errors.Count(); i++)
    wxLogMessage(wxT("identify 1: errors[%d]: ``%s''"), i, errors[i].c_str());

  wxRemoveFile(opt_file_name);
  if ((result < 0) || (result > 1)) {
    name.Printf(wxT("File identification failed for '%s'. Return code: "
                    "%d\n\n"), file_name.c_str(), result);
    for (i = 0; i < output.Count(); i++)
      name += break_line(output[i]) + wxT("\n");
    name += wxT("\n");
    for (i = 0; i < errors.Count(); i++)
      name += break_line(errors[i]) + wxT("\n");
    wxMessageBox(name, wxT("File identification failed"), wxOK | wxCENTER |
                 wxICON_ERROR);
    return;
  } else if (result > 0) {
    name.Printf(wxT("File identification failed. Return code: %d. Errno: %d "
                    "(%s). Make sure that you've selected a mkvmerge "
                    "executable on the 'settings' tab."), result, errno,
                wxUCS(strerror(errno)));
    break_line(name, 60);
    wxMessageBox(name, wxT("File identification failed"), wxOK | wxCENTER |
                 wxICON_ERROR);
    return;
  }

  default_audio_track_found = -1 != default_track_checked('a');
  default_video_track_found = -1 != default_track_checked('v');
  default_subtitle_track_found = -1 != default_track_checked('s');
  for (i = 0; i < output.Count(); i++) {
    if (output[i].Find(wxT("Track")) == 0) {
      wxString info;
      mmg_track_ptr track(new mmg_track_t);

      id = output[i].AfterFirst(wxT(' ')).AfterFirst(wxT(' ')).
        BeforeFirst(wxT(':'));
      type = output[i].AfterFirst(wxT(':')).BeforeFirst(wxT('(')).Mid(1).
        RemoveLast();
      exact = output[i].AfterFirst(wxT('(')).BeforeFirst(wxT(')'));
      info = output[i].AfterFirst(wxT('[')).BeforeLast(wxT(']'));
      if (type == wxT("audio"))
        track->type = 'a';
      else if (type == wxT("video"))
        track->type = 'v';
      else if (type == wxT("subtitles"))
        track->type = 's';
      else
        track->type = '?';
      parse_int(wxMB(id), track->id);
      track->ctype = exact;
      track->enabled = true;

      if (info.length() > 0) {
        args = split(info, wxU(" "));
        for (k = 0; k < args.size(); k++) {
          pair = split(args[k], wxU(":"), 2);
          if (pair.size() != 2)
            continue;

          if (pair[0] == wxT("track_name")) {
            track->track_name = from_utf8(unescape(pair[1]));
            track->track_name_was_present = true;

          } else if (pair[0] == wxT("language"))
            track->language = unescape(pair[1]);

          else if (pair[0] == wxT("display_dimensions")) {
            vector<wxString> dims;
            int64_t width, height;

            dims = split(pair[1], wxU("x"));
            if ((dims.size() == 2) && parse_int(wxMB(dims[0]), width) &&
                parse_int(wxMB(dims[1]), height)) {
              track->dwidth.Printf(wxT(LLD), width);
              track->dheight.Printf(wxT(LLD), height);
              track->display_dimensions_selected = true;
            }

          } else if ((pair[0] == wxT("default_track")) &&
                     (pair[1] == wxT("1"))) {
            if (('a' == track->type) && !default_audio_track_found) {
              track->default_track = true;
              default_audio_track_found = true;

            } else if (('v' == track->type) && !default_video_track_found) {
              track->default_track = true;
              default_video_track_found = true;

            } else if (('s' == track->type) && !default_subtitle_track_found) {
              track->default_track = true;
              default_subtitle_track_found = true;
            }
          }
        }
      }

      if ((track->type == 'v') && (track->track_name.Length() > 0))
        video_track_name = track->track_name;

      track->appending = append;

      file.tracks.push_back(track);

    } else if ((pos = output[i].Find(wxT("container:"))) > 0) {
      wxString container, info;

      container = output[i].Mid(pos + 11).BeforeFirst(wxT(' '));
      info = output[i].Mid(pos + 11).AfterFirst(wxT('[')).
        BeforeLast(wxT(']'));
      if (container == wxT("AAC"))
        file.container = FILE_TYPE_AAC;
      else if (container == wxT("AC3"))
        file.container = FILE_TYPE_AC3;
      else if (container == wxT("AVI"))
        file.container = FILE_TYPE_AVI;
      else if (container == wxT("DTS"))
        file.container = FILE_TYPE_DTS;
      else if (container == wxT("Matroska"))
        file.container = FILE_TYPE_MATROSKA;
      else if (container == wxT("MP2/MP3"))
        file.container = FILE_TYPE_MP3;
      else if (container == wxT("Ogg/OGM"))
        file.container = FILE_TYPE_OGM;
      else if (container == wxT("Quicktime/MP4"))
        file.container = FILE_TYPE_QTMP4;
      else if (container == wxT("RealMedia"))
        file.container = FILE_TYPE_REAL;
      else if (container == wxT("SRT"))
        file.container = FILE_TYPE_SRT;
      else if (container == wxT("SSA/ASS"))
        file.container = FILE_TYPE_SSA;
      else if (container == wxT("VobSub"))
        file.container = FILE_TYPE_VOBSUB;
      else if (container == wxT("WAV"))
        file.container = FILE_TYPE_WAV;
      else
        file.container = FILE_TYPE_IS_UNKNOWN;

      if (info.length() > 0) {
        args = split(info, wxU(" "));
        for (k = 0; k < args.size(); k++) {
          pair = split(args[k], wxU(":"), 2);
          if ((pair.size() == 2) && (pair[0] == wxT("title"))) {
            file.title = from_utf8(unescape(pair[1]));
            file.title_was_present = true;
            title_was_present = true;
          }
        }
      }
    }
  }

  if (file.tracks.size() == 0) {
    wxMessageBox(wxT("The input file '") + file_name +
                 wxT("' does not contain any tracks."),
                 wxT("No tracks found"));
    return;
  }

  // Look for a place to insert the new file. If the file is only "added",
  // then it will be added to the back of the list. If it is "appended",
  // then it should be inserted right after the currently selected file.
  if (!append)
    new_file_pos = lb_input_files->GetCount();
  else {
    new_file_pos = lb_input_files->GetSelection();
    if (wxNOT_FOUND == new_file_pos)
      new_file_pos = lb_input_files->GetCount();
    else {
      do {
        ++new_file_pos;
      } while ((files.size() > new_file_pos) && files[new_file_pos].appending);
    }
  }

  name.Printf(wxT("%s%s (%s)"), append ? wxT("++> ") : wxT(""),
              file_name.AfterLast(wxT(PSEP)).c_str(),
              file_name.BeforeLast(wxT(PSEP)).c_str());
  lb_input_files->Insert(name, new_file_pos);

  file.file_name = file_name;
  mdlg->set_title_maybe(file.title);
  if ((file.container == FILE_TYPE_OGM) && (video_track_name.Length() > 0))
    mdlg->set_title_maybe(video_track_name);
  mdlg->set_output_maybe(file.file_name);
  file.appending = append;
  files.insert(files.begin() + new_file_pos, file);

  // After inserting the file the "source" index is wrong for all files
  // after the insertion position.
  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->source >= new_file_pos)
      ++tracks[i]->source;

  if (name.StartsWith(wxT("++> ")))
    name.Remove(0, 4);

  for (i = 0; i < file.tracks.size(); i++) {
    string format;
    wxString label;
    int new_track_pos;

    mmg_track_ptr &t = file.tracks[i];
    t->enabled = true;
    t->source = new_file_pos;

    fix_format("%s%s (ID %lld, type: %s) from %s", format);
    label.Printf(wxU(format.c_str()), t->appending ? wxT("++> ") : wxT(""),
                 t->ctype.c_str(), t->id,
                 t->type == 'a' ? wxT("audio") :
                 t->type == 'v' ? wxT("video") :
                 t->type == 's' ? wxT("subtitles") : wxT("unknown"),
                 name.c_str());

    // Look for a place to insert this new track. If the file is "added" then
    // the new track is simply appended to the list of existing tracks.
    // If the file is "appended" then it should be put to the end of the
    // chain of tracks being appended. The n'th track from this new file
    // should be appended to the n'th track of the "old" file this new file is
    // appended to. So first I have to find the n'th track of the "old" file.
    // Then I have to skip over all the other tracks that are already appended
    // to that n'th track. The insertion point is right after that.
    if (append) {
      int nth_old_track;

      nth_old_track = 0;
      new_track_pos = 0;
      while ((tracks.size() > new_track_pos) && (nth_old_track < (i + 1))) {
        if (tracks[new_track_pos]->source == (new_file_pos - 1))
          ++nth_old_track;
        ++new_track_pos;
      }

      // Either we've found the n'th track from the "old" file or we're at
      // the end of the list. In the latter case we just append the track
      // at the end and let the user figure out which track he really wants
      // to append it to.
      if (nth_old_track == (i + 1))
        while ((tracks.size() > new_track_pos) &&
               tracks[new_track_pos]->appending)
          ++new_track_pos;

    } else
      new_track_pos = tracks.size();

    tracks.insert(tracks.begin() + new_track_pos, t.get());
    clb_tracks->Insert(label, new_track_pos);
    clb_tracks->Check(new_track_pos, true);
  }

  st_tracks->Enable(true);
  clb_tracks->Enable(true);
  b_append_file->Enable(true);
}

void
tab_input::on_remove_file(wxCommandEvent &evt) {
  mmg_file_t *f;
  mmg_track_t *t;
  vector<mmg_file_t>::iterator eit;
  vector<mmg_track_t *>::iterator tit;
  uint32_t i;

  if (selected_file == -1)
    return;

  f = &files[selected_file];

  // Files may not be removed if something else is still being appended to
  // them.
  for (i = 0; i < (tracks.size() - 1); i++)
    if ((tracks[i]->source == selected_file) && tracks[i + 1]->appending &&
        (tracks[i]->source != tracks[i + 1]->source)) {
      wxString err;

      err.Printf(wxT("The current file (number %d) cannot be removed. There "
                     "are other files -- at least file number %d -- whose "
                     "tracks are supposed to be appended to tracks from this "
                     "file. Please remove those files first."),
                 selected_file + 1, tracks[i + 1]->source + 1);
      wxMessageBox(err, wxT("File removal not possible"),
                   wxOK | wxICON_ERROR | wxCENTER);
      return;
    }

  dont_copy_values_now = true;

  i = 0;
  tit = tracks.begin();
  while (i < tracks.size()) {
    t = tracks[i];
    if (t->source == selected_file) {
      clb_tracks->Delete(i);
      tracks.erase(tit);
    } else {
      if (t->source > selected_file)
        t->source--;
      i++;
      tit++;
    }
  }

  // Unset the "file/segment title" box if the segment title came from this
  // source file and the user hasn't changed it since.
  if ((f->title != wxT("")) && f->title_was_present &&
      (f->title == mdlg->global_page->tc_title->GetValue()))
    mdlg->global_page->tc_title->SetValue(wxT(""));

  files.erase(files.begin() + selected_file);
  lb_input_files->Delete(selected_file);
  selected_file = -1;
  st_file_options->Enable(false);
  cb_no_chapters->Enable(false);
  cb_no_attachments->Enable(false);
  cb_no_tags->Enable(false);
  b_remove_file->Enable(false);
  b_append_file->Enable(tracks.size() > 0);
  b_track_up->Enable(false);
  b_track_down->Enable(false);
  no_track_mode();
  if (tracks.size() == 0) {
    st_tracks->Enable(false);
    clb_tracks->Enable(false);
    selected_track = -1;
  }

  dont_copy_values_now = false;
}

void
tab_input::on_move_track_up(wxCommandEvent &evt) {
  wxString s;
  mmg_track_t *t;

  if (selected_track < 1)
    return;

  // Appended tracks may not be at the top.
  if ((selected_track == 1) && tracks[selected_track]->appending)
    return;

  dont_copy_values_now = true;

  t = tracks[selected_track - 1];
  tracks[selected_track - 1] = tracks[selected_track];
  tracks[selected_track] = t;
  s = clb_tracks->GetString(selected_track - 1);
  clb_tracks->SetString(selected_track - 1,
                        clb_tracks->GetString(selected_track));
  clb_tracks->SetString(selected_track, s);
  clb_tracks->SetSelection(selected_track - 1);
  clb_tracks->Check(selected_track - 1,
                    tracks[selected_track - 1]->enabled);
  clb_tracks->Check(selected_track, tracks[selected_track]->enabled);
  selected_track--;
  b_track_up->Enable(selected_track > 0);
  b_track_down->Enable(true);

  dont_copy_values_now = false;
}

void
tab_input::on_move_track_down(wxCommandEvent &evt) {
  wxString s;
  mmg_track_t *t;

  if ((selected_track < 0) || (selected_track >= tracks.size() - 1))
    return;

  // Appended tracks may not be at the top.
  if ((selected_track == 0) && (tracks.size() > 1) && tracks[1]->appending)
    return;

  dont_copy_values_now = true;

  t = tracks[selected_track + 1];
  tracks[selected_track + 1] = tracks[selected_track];
  tracks[selected_track] = t;
  s = clb_tracks->GetString(selected_track + 1);
  clb_tracks->SetString(selected_track + 1,
                        clb_tracks->GetString(selected_track));
  clb_tracks->SetString(selected_track, s);
  clb_tracks->SetSelection(selected_track + 1);
  clb_tracks->Check(selected_track + 1,
                    tracks[selected_track + 1]->enabled);
  clb_tracks->Check(selected_track, tracks[selected_track]->enabled);
  selected_track++;
  b_track_up->Enable(true);
  b_track_down->Enable(selected_track < (tracks.size() - 1));

  dont_copy_values_now = false;
}

void
tab_input::on_file_selected(wxCommandEvent &evt) {
  int new_sel, old_track;
  mmg_file_t *f;
  wxString label;

  b_remove_file->Enable(true);
  b_append_file->Enable(true);
  selected_file = -1;
  old_track = selected_track;
  selected_track = -1;
  new_sel = lb_input_files->GetSelection();
  f = &files[new_sel];
  if (f->container == FILE_TYPE_MATROSKA) {
    st_file_options->Enable(true);
    cb_no_chapters->Enable(true);
    cb_no_attachments->Enable(true);
    cb_no_tags->Enable(true);
    cb_no_chapters->SetValue(f->no_chapters);
    cb_no_attachments->SetValue(f->no_attachments);
    cb_no_tags->SetValue(f->no_tags);
  } else {
    st_file_options->Enable(false);
    cb_no_chapters->Enable(false);
    cb_no_attachments->Enable(false);
    cb_no_tags->Enable(false);
    cb_no_chapters->SetValue(false);
    cb_no_attachments->SetValue(false);
    cb_no_tags->SetValue(false);
  }
  selected_file = new_sel;
  selected_track = old_track;
}

void
tab_input::on_nochapters_clicked(wxCommandEvent &evt) {
  if (selected_file != -1)
    files[selected_file].no_chapters = cb_no_chapters->GetValue();
}

void
tab_input::on_noattachments_clicked(wxCommandEvent &evt) {
  if (selected_file != -1)
    files[selected_file].no_attachments = cb_no_attachments->GetValue();
}

void
tab_input::on_notags_clicked(wxCommandEvent &evt) {
  if (selected_file != -1)
    files[selected_file].no_tags = cb_no_tags->GetValue();
}

void
tab_input::on_track_selected(wxCommandEvent &evt) {
  mmg_file_t *f;
  mmg_track_t *t;
  int new_sel;
  uint32_t i;
  wxString lang;

  dont_copy_values_now = true;

  selected_track = -1;
  new_sel = clb_tracks->GetSelection();
  if (0 > new_sel)
    return;

  t = tracks[new_sel];
  f = &files[t->source];

  b_track_up->Enable(new_sel > 0);
  b_track_down->Enable(new_sel < (tracks.size() - 1));

  if (t->type == 'a')
    audio_track_mode(t->ctype, t->appending);
  else if (t->type == 'v') {
    video_track_mode(t->ctype, t->appending);
    enable_ar_controls(t);
  } else if (t->type == 's')
    subtitle_track_mode(t->ctype, t->appending);

  lang = t->language;
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    if (extract_language_code(sorted_iso_codes[i]) == lang) {
      lang = sorted_iso_codes[i];
      break;
    }
  cob_language->SetValue(lang);
  tc_track_name->SetValue(t->track_name);
  cob_cues->SetValue(t->cues);
  tc_delay->SetValue(t->delay);
  tc_stretch->SetValue(t->stretch);
  cob_sub_charset->SetValue(t->sub_charset);
  tc_tags->SetValue(t->tags);
  cb_default->SetValue(t->default_track);
  cb_aac_is_sbr->SetValue(t->aac_is_sbr);
  cob_aspect_ratio->SetValue(t->aspect_ratio);
  tc_display_width->SetValue(t->dwidth);
  tc_display_height->SetValue(t->dheight);
  selected_track = new_sel;
  cob_compression->SetValue(t->compression);
  tc_timecodes->SetValue(t->timecodes);
  cob_fourcc->SetValue(t->fourcc);
  tc_track_name->SetFocus();

  dont_copy_values_now = false;
}

void
tab_input::on_track_enabled(wxCommandEvent &evt) {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    tracks[i]->enabled = clb_tracks->IsChecked(i);
}

void
tab_input::on_default_track_clicked(wxCommandEvent &evt) {
  mmg_track_t *t;

  if (selected_track == -1)
    return;

  t = tracks[selected_track];
  if (cb_default->GetValue()) {
    int idx;

    idx = default_track_checked(t->type);
    if (-1 != idx)
      tracks[idx]->default_track = false;
  }

  t->default_track = cb_default->GetValue();
}

void
tab_input::on_aac_is_sbr_clicked(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->aac_is_sbr = cb_aac_is_sbr->GetValue();
}

void
tab_input::on_language_selected(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->language = cob_language->GetStringSelection();
}

void
tab_input::on_cues_selected(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->cues = cob_cues->GetStringSelection();
}

void
tab_input::on_subcharset_selected(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->sub_charset = cob_sub_charset->GetStringSelection();
}

void
tab_input::on_browse_tags(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  wxFileDialog dlg(NULL, wxT("Choose a tag file"), last_open_dir, wxT(""),
                   wxT("Tag files (*.xml;*.txt)|*.xml;*.txt|" ALLFILES),
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tracks[selected_track]->tags = dlg.GetPath();
    tc_tags->SetValue(dlg.GetPath());
  }
}

void
tab_input::on_browse_timecodes_clicked(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  wxFileDialog dlg(NULL, wxT("Choose a timecodes file"), last_open_dir,
                   wxT(""), wxT("Timecode files (*.tmc;*.txt)|*.tmc;*.txt|"
                                ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tracks[selected_track]->timecodes = dlg.GetPath();
    tc_timecodes->SetValue(dlg.GetPath());
  }
}

void
tab_input::on_tags_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->tags = tc_tags->GetValue();
}

void
tab_input::on_timecodes_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->timecodes = tc_timecodes->GetValue();
}

void
tab_input::on_delay_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->delay = tc_delay->GetValue();
}

void
tab_input::on_stretch_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->stretch = tc_stretch->GetValue();
}

void
tab_input::on_track_name_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->track_name = tc_track_name->GetValue();
}

void
tab_input::on_aspect_ratio_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->aspect_ratio =
    cob_aspect_ratio->GetStringSelection();
}

void
tab_input::on_fourcc_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->fourcc = cob_fourcc->GetStringSelection();
}

void
tab_input::on_compression_selected(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->compression = cob_compression->GetStringSelection();
}

void
tab_input::on_display_width_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->dwidth = tc_display_width->GetValue();
}

void
tab_input::on_display_height_changed(wxCommandEvent &evt) {
  if (selected_track == -1)
    return;

  tracks[selected_track]->dheight = tc_display_height->GetValue();
}

void
tab_input::on_aspect_ratio_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (selected_track == -1)
    return;

  track = tracks[selected_track];
  track->display_dimensions_selected = false;
  enable_ar_controls(track);
}

void
tab_input::on_display_dimensions_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (selected_track == -1)
    return;

  track = tracks[selected_track];
  track->display_dimensions_selected = true;
  enable_ar_controls(track);
}

void
tab_input::on_value_copy_timer(wxTimerEvent &evt) {
  mmg_track_t *t;

  if (dont_copy_values_now || (selected_track == -1))
    return;

  t = tracks[selected_track];
  t->aspect_ratio = cob_aspect_ratio->GetValue();
  t->fourcc = cob_fourcc->GetValue();
}

void
tab_input::save(wxConfigBase *cfg) {
  uint32_t fidx, tidx;
  mmg_file_t *f;
  wxString s;

  cfg->SetPath(wxT("/input"));
  cfg->Write(wxT("number_of_files"), (int)files.size());
  for (fidx = 0; fidx < files.size(); fidx++) {
    f = &files[fidx];
    s.Printf(wxT("file %u"), fidx);
    cfg->SetPath(s);
    cfg->Write(wxT("file_name"), f->file_name);
    cfg->Write(wxT("container"), f->container);
    cfg->Write(wxT("no_chapters"), f->no_chapters);
    cfg->Write(wxT("no_attachments"), f->no_attachments);
    cfg->Write(wxT("no_tags"), f->no_tags);
    cfg->Write(wxT("appending"), f->appending);

    cfg->Write(wxT("number_of_tracks"), (int)f->tracks.size());
    for (tidx = 0; tidx < f->tracks.size(); tidx++) {
      string format;

      mmg_track_ptr &t = f->tracks[tidx];
      s.Printf(wxT("track %u"), tidx);
      cfg->SetPath(s);
      s.Printf(wxT("%c"), t->type);
      cfg->Write(wxT("type"), s);
      fix_format("%lld", format);
      s.Printf(wxU(format.c_str()), t->id);
      cfg->Write(wxT("id"), s);
      cfg->Write(wxT("enabled"), t->enabled);
      cfg->Write(wxT("content_type"), t->ctype);
      cfg->Write(wxT("default_track"), t->default_track);
      cfg->Write(wxT("aac_is_sbr"), t->aac_is_sbr);
      cfg->Write(wxT("language"), t->language);
      cfg->Write(wxT("track_name"), t->track_name);
      cfg->Write(wxT("cues"), t->cues);
      cfg->Write(wxT("delay"), t->delay);
      cfg->Write(wxT("stretch"), t->stretch);
      cfg->Write(wxT("sub_charset"), t->sub_charset);
      cfg->Write(wxT("tags"), t->tags);
      cfg->Write(wxT("timecodes"), t->timecodes);
      cfg->Write(wxT("display_dimensions_selected"),
                 t->display_dimensions_selected);
      cfg->Write(wxT("aspect_ratio"), t->aspect_ratio);
      cfg->Write(wxT("display_width"), t->dwidth);
      cfg->Write(wxT("display_height"), t->dheight);
      cfg->Write(wxT("fourcc"), t->fourcc);
      cfg->Write(wxT("compression"), t->compression);
      cfg->Write(wxT("track_name_was_present"), t->track_name_was_present);
      cfg->Write(wxT("appending"), t->appending);

      cfg->SetPath(wxT(".."));
    }

    cfg->SetPath(wxT(".."));
  }
  cfg->Write(wxT("track_order"), create_track_order(true));
}

void
tab_input::load(wxConfigBase *cfg,
                int version) {
  long fidx, tidx, i;
  wxString s, c, id, track_order;
  string format;
  vector<wxString> pair, entries;
  int num_files, num_tracks;

  dont_copy_values_now = true;

  clb_tracks->Clear();
  lb_input_files->Clear();
  no_track_mode();
  selected_file = -1;
  selected_track = -1;
  b_remove_file->Enable(false);
  b_append_file->Enable(false);

  files.clear();
  tracks.clear();

  cfg->SetPath(wxT("/input"));
  if (!cfg->Read(wxT("number_of_files"), &num_files) || (num_files < 0)) {
    dont_copy_values_now = false;
    return;
  }

  fix_format("%u:%lld", format);
  for (fidx = 0; fidx < num_files; fidx++) {
    mmg_file_t fi;

    s.Printf(wxT("file %ld"), fidx);
    cfg->SetPath(s);
    if (!cfg->Read(wxT("number_of_tracks"), &num_tracks) || (num_tracks < 0)) {
      cfg->SetPath(wxT(".."));
      continue;
    }
    if (!cfg->Read(wxT("file_name"), &fi.file_name)) {
      cfg->SetPath(wxT(".."));
      continue;
    }
    cfg->Read(wxT("title"), &fi.title);
    cfg->Read(wxT("container"), &fi.container);
    cfg->Read(wxT("no_chapters"), &fi.no_chapters, false);
    cfg->Read(wxT("no_attachments"), &fi.no_attachments, false);
    cfg->Read(wxT("no_tags"), &fi.no_tags, false);
    cfg->Read(wxT("appending"), &fi.appending, false);

    for (tidx = 0; tidx < (uint32_t)num_tracks; tidx++) {
      mmg_track_ptr tr(new mmg_track_t);

      s.Printf(wxT("track %ld"), tidx);
      cfg->SetPath(s);
      if (!cfg->Read(wxT("type"), &c) || (c.Length() != 1) ||
          !cfg->Read(wxT("id"), &id)) {
        cfg->SetPath(wxT(".."));
        continue;
      }
      tr->type = c.c_str()[0];
      if (((tr->type != 'a') && (tr->type != 'v') && (tr->type != 's')) ||
          !parse_int(wxMB(id), tr->id)) {
        cfg->SetPath(wxT(".."));
        continue;
      }
      cfg->Read(wxT("enabled"), &tr->enabled);
      cfg->Read(wxT("content_type"), &tr->ctype);
      cfg->Read(wxT("default_track"), &tr->default_track, false);
      cfg->Read(wxT("aac_is_sbr"), &tr->aac_is_sbr, false);
      cfg->Read(wxT("language"), &tr->language);
      cfg->Read(wxT("track_name"), &tr->track_name);
      cfg->Read(wxT("cues"), &tr->cues);
      cfg->Read(wxT("delay"), &tr->delay);
      cfg->Read(wxT("stretch"), &tr->stretch);
      cfg->Read(wxT("sub_charset"), &tr->sub_charset);
      cfg->Read(wxT("tags"), &tr->tags);
      cfg->Read(wxT("display_dimensions_selected"),
                &tr->display_dimensions_selected, false);
      cfg->Read(wxT("aspect_ratio"), &tr->aspect_ratio);
      cfg->Read(wxT("display_width"), &tr->dwidth);
      cfg->Read(wxT("display_height"), &tr->dheight);
      cfg->Read(wxT("fourcc"), &tr->fourcc);
      cfg->Read(wxT("compression"), &tr->compression);
      cfg->Read(wxT("timecodes"), &tr->timecodes);
      cfg->Read(wxT("track_name_was_present"), &tr->track_name_was_present,
                false);
      cfg->Read(wxT("appending"), &tr->appending, false);
      tr->source = files.size();
      if (track_order.Length() > 0)
        track_order += wxT(",");
      track_order += wxString::Format(wxUCS(format.c_str()), files.size(),
                                      tr->id);

      fi.tracks.push_back(tr);
      cfg->SetPath(wxT(".."));
    }

    if (fi.tracks.size() != 0) {
      s = fi.file_name.BeforeLast(PSEP);
      c = fi.file_name.AfterLast(PSEP);
      lb_input_files->Append(wxString::Format(wxT("%s%s (%s)"),
                                              fi.appending ? wxT("++> ") :
                                              wxT(""), c.c_str(), s.c_str()));
      files.push_back(fi);
    }

    cfg->SetPath(wxT(".."));
  }

  s = wxT("");
  if (!cfg->Read(wxT("track_order"), &s) || (s.length() == 0))
    s = track_order;
  strip(s);
  if (s.length() > 0) {
    entries = split(s, (wxString)wxT(","));
    for (i = 0; i < entries.size(); i++) {
      wxString label, name;
      bool found;
      int j;

      pair = split(entries[i], (wxString)wxT(":"));
      if (pair.size() != 2)
        wxdie(wxT("The job file could not have been parsed correctly.\n"
                  "Either it is invalid / damaged, or you've just found\n"
                  "a bug in mmg. Please report this to the author\n"
                  "Moritz Bunkus <moritz@bunkus.org>\n\n"
                  "(Problem occured in tab_input::load(), #1)"));
      if (!pair[0].ToLong(&fidx) || !pair[1].ToLong(&tidx) ||
          (fidx >= files.size()))
        wxdie(wxT("The job file could not have been parsed correctly.\n"
                  "Either it is invalid / damaged, or you've just found\n"
                  "a bug in mmg. Please report this to the author\n"
                  "Moritz Bunkus <moritz@bunkus.org>\n\n"
                  "(Problem occured in tab_input::load(), #2)"));
      found = false;
      for (j = 0; j < files[fidx].tracks.size(); j++)
        if (files[fidx].tracks[j]->id == tidx) {
          found = true;
          tidx = j;
        }
      if (!found)
        wxdie(wxT("The job file could not have been parsed correctly.\n"
                  "Either it is invalid / damaged, or you've just found\n"
                  "a bug in mmg. Please report this to the author\n"
                  "Moritz Bunkus <moritz@bunkus.org>\n\n"
                  "(Problem occured in tab_input::load(), #3)"));
      mmg_track_ptr &t = files[fidx].tracks[tidx];
      tracks.push_back(t.get());

      fix_format("%s%s (ID %lld, type: %s) from %s", format);
      name = files[fidx].file_name.AfterLast(wxT(PSEP));
      name += wxT(" (") + files[fidx].file_name.BeforeLast(wxT(PSEP)) +
        wxT(")");
      label.Printf(wxU(format.c_str()), t->appending ? wxT("++> ") : wxT(""),
                   t->ctype.c_str(), t->id,
                   t->type == 'a' ? wxT("audio") :
                   t->type == 'v' ? wxT("video") :
                   t->type == 's' ? wxT("subtitles") : wxT("unknown"),
                   name.c_str());
      clb_tracks->Append(label);
      clb_tracks->Check(i, t->enabled);
    }
  }
  st_tracks->Enable(tracks.size() > 0);
  clb_tracks->Enable(tracks.size() > 0);
  b_append_file->Enable(files.size() > 0);

  dont_copy_values_now = false;
}

bool
tab_input::validate_settings() {
  uint32_t fidx, tidx, i;
  mmg_file_t *f;
  bool tracks_selected, dot_present, ok;
  int64_t dummy_i;
  string format, s;
  wxString sid;

  // Appending a file to itself is not allowed.
  for (tidx = 2; tidx < tracks.size(); tidx++)
    if (tracks[tidx - 1]->appending && tracks[tidx]->appending &&
        (tracks[tidx - 1]->source == tracks[tidx]->source)) {
      wxString err;

      err.Printf(wxT("Appending a track from a file to another track from "
                     "the same file is not allowed. This is the case for "
                     "tracks number %d and %d."),
                 tidx, tidx + 1);
      wxMessageBox(err, wxT("mkvmerge GUI: error"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return false;
    }

  tracks_selected = false;
  for (fidx = 0; fidx < files.size(); fidx++) {
    f = &files[fidx];

    for (tidx = 0; tidx < f->tracks.size(); tidx++) {
      mmg_track_ptr &t = f->tracks[tidx];
      if (!t->enabled)
        continue;

      tracks_selected = true;
      fix_format("%lld", format);
      sid.Printf(wxU(format.c_str()), t->id);

      s = wxMB(t->delay);
      strip(s);
      if ((s.length() > 0) && !parse_int(s, dummy_i)) {
        wxMessageBox(wxT("The delay setting for track nr. ") + sid +
                     wxT(" in file '") + f->file_name + wxT("' is invalid."),
                     wxT("mkvmerge GUI: error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

      s = wxMB(t->stretch);
      strip(s);
      if (s.length() > 0) {
        dot_present = false;
        i = 0;
        while (i < s.length()) {
          if (isdigit(s[i]) ||
              (!dot_present && ((s[i] == '.') || (s[i] == ',')))) {
            if ((s[i] == '.') || (s[i] == ','))
              dot_present = true;
            i++;
          } else {
            wxMessageBox(wxT("The stretch setting for track nr. ") + sid +
                         wxT(" in file '") + f->file_name +
                         wxT("' is invalid."), wxT("mkvmerge GUI: error"),
                         wxOK | wxCENTER | wxICON_ERROR);
            return false;
          }
        }
      }

      s = wxMB(t->fourcc);
      strip(s);
      if ((s.length() > 0) && (s.length() != 4)) {
        wxMessageBox(wxT("The FourCC setting for track nr. ") + sid +
                     wxT(" in file '") + f->file_name +
                     wxT("' is not excatly four characters long."),
                     wxT("mkvmerge GUI: error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

      s = wxMB(t->aspect_ratio);
      strip(s);
      if (s.length() > 0) {
        dot_present = false;
        i = 0;
        ok = true;
        while (i < s.length()) {
          if (isdigit(s[i]) ||
              (!dot_present && ((s[i] == '.') || (s[i] == ',')))) {
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
          i = 0;
          ok = true;
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
          wxMessageBox(wxT("The aspect ratio setting for track nr. ") + sid +
                       wxT(" in file '") + f->file_name +
                       wxT("' is invalid."), wxT("mkvmerge GUI: error"),
                       wxOK | wxCENTER | wxICON_ERROR);
          return false;
        }
      }
    }
  }

  if (!tracks_selected) {
    wxMessageBox(wxT("You have not yet selected any input file and/or no "
                     "tracks."),
                 wxT("mkvmerge GUI: error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  return true;
}

IMPLEMENT_CLASS(tab_input, wxPanel);
BEGIN_EVENT_TABLE(tab_input, wxPanel)
  EVT_BUTTON(ID_B_ADDFILE, tab_input::on_add_file)
  EVT_BUTTON(ID_B_REMOVEFILE, tab_input::on_remove_file)
  EVT_BUTTON(ID_B_APPENDFILE, tab_input::on_append_file)
  EVT_BUTTON(ID_B_TRACKUP, tab_input::on_move_track_up)
  EVT_BUTTON(ID_B_TRACKDOWN, tab_input::on_move_track_down)
  EVT_BUTTON(ID_B_BROWSETAGS, tab_input::on_browse_tags)
  EVT_BUTTON(ID_B_BROWSE_TIMECODES, tab_input::on_browse_timecodes_clicked)
  EVT_TEXT(ID_TC_TAGS, tab_input::on_tags_changed)
  EVT_TEXT(ID_TC_TIMECODES, tab_input::on_timecodes_changed)

  EVT_LISTBOX(ID_LB_INPUTFILES, tab_input::on_file_selected)
  EVT_LISTBOX(ID_CLB_TRACKS, tab_input::on_track_selected)
  EVT_CHECKLISTBOX(ID_CLB_TRACKS, tab_input::on_track_enabled)

  EVT_CHECKBOX(ID_CB_NOCHAPTERS, tab_input::on_nochapters_clicked)
  EVT_CHECKBOX(ID_CB_NOATTACHMENTS, tab_input::on_noattachments_clicked)
  EVT_CHECKBOX(ID_CB_NOTAGS, tab_input::on_notags_clicked)
  EVT_CHECKBOX(ID_CB_MAKEDEFAULT, tab_input::on_default_track_clicked)
  EVT_CHECKBOX(ID_CB_AACISSBR, tab_input::on_aac_is_sbr_clicked)

  EVT_COMBOBOX(ID_CB_LANGUAGE, tab_input::on_language_selected)
  EVT_COMBOBOX(ID_CB_CUES, tab_input::on_cues_selected)
  EVT_COMBOBOX(ID_CB_SUBTITLECHARSET, tab_input::on_subcharset_selected)
  EVT_COMBOBOX(ID_CB_ASPECTRATIO, tab_input::on_aspect_ratio_changed)
  EVT_COMBOBOX(ID_CB_FOURCC, tab_input::on_fourcc_changed)
  EVT_TEXT(ID_CB_SUBTITLECHARSET, tab_input::on_subcharset_selected)
  EVT_TEXT(ID_TC_DELAY, tab_input::on_delay_changed)
  EVT_TEXT(ID_TC_STRETCH, tab_input::on_stretch_changed)
  EVT_TEXT(ID_TC_TRACKNAME, tab_input::on_track_name_changed)
  EVT_RADIOBUTTON(ID_RB_ASPECTRATIO, tab_input::on_aspect_ratio_selected)
  EVT_RADIOBUTTON(ID_RB_DISPLAYDIMENSIONS,
                  tab_input::on_display_dimensions_selected)
  EVT_TEXT(ID_TC_DISPLAYWIDTH, tab_input::on_display_width_changed)
  EVT_TEXT(ID_TC_DISPLAYHEIGHT, tab_input::on_display_height_changed)
  EVT_COMBOBOX(ID_CB_COMPRESSION, tab_input::on_compression_selected)

  EVT_TIMER(ID_T_INPUTVALUES, tab_input::on_value_copy_timer)

END_EVENT_TABLE();
