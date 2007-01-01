/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "input" tab -- "Format specific track options" sub-tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/file.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/regex.h"
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

tab_input_format::tab_input_format(wxWindow *parent,
                                   tab_input *ti):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL),
  input(ti) {

  wxFlexGridSizer *siz_fg;
  wxBoxSizer *siz_all, *siz_line;
  int i;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(TOPBOTTOMSPACING);

  siz_fg = new wxFlexGridSizer(4);
  siz_fg->AddGrowableCol(1);
  siz_fg->AddGrowableCol(3);

  rb_aspect_ratio =
    new wxRadioButton(this, ID_RB_ASPECTRATIO, wxT("Aspect ratio:"),
                      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_aspect_ratio->SetValue(true);
  siz_fg->Add(rb_aspect_ratio, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_aspect_ratio =
    new wxComboBox(this, ID_CB_ASPECTRATIO, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);

  for (i = 0; NULL != predefined_aspect_ratios[i]; ++i)
    cob_aspect_ratio->Append(predefined_aspect_ratios[i]);

  cob_aspect_ratio->SetToolTip(TIP("Sets the display aspect ratio of the "
                                   "track. The format can be either 'a/b' in "
                                   "which case both numbers must be integer "
                                   "(e.g. 16/9) or just a single floting "
                                   "point number 'f' (e.g. 2.35)."));
  cob_aspect_ratio->SetSizeHints(0, -1);
  siz_fg->Add(cob_aspect_ratio, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  rb_display_dimensions =
    new wxRadioButton(this, ID_RB_DISPLAYDIMENSIONS,
                      wxT("Display width/height:"));
  rb_display_dimensions->SetValue(false);
  siz_fg->Add(rb_display_dimensions, 0, wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  tc_display_width = new wxTextCtrl(this, ID_TC_DISPLAYWIDTH, wxT(""));
  tc_display_width->SetToolTip(TIP("Sets the display width of the track."
                                   "The height must be set as well, or this "
                                   "field will be ignored."));
  tc_display_width->SetSizeHints(0, -1);
  siz_line->Add(tc_display_width, 1, wxGROW | wxALL, STDSPACING);

  st_x = new wxStaticText(this, wxID_STATIC, wxT("x"));
  st_x->Enable(false);
  siz_line->Add(st_x, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_display_height = new wxTextCtrl(this, ID_TC_DISPLAYHEIGHT, wxT(""));
  tc_display_height->SetToolTip(TIP("Sets the display height of the track."
                                    "The width must be set as well, or this "
                                    "field will be ignored."));
  tc_display_height->SetSizeHints(0, -1);
  siz_line->Add(tc_display_height, 1, wxGROW | wxALL, STDSPACING);
  siz_fg->Add(siz_line, 1, wxGROW, 0);

  st_fourcc = new wxStaticText(this, -1, wxT("FourCC:"));
  st_fourcc->Enable(false);
  siz_fg->Add(st_fourcc, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_fourcc =
    new wxComboBox(this, ID_CB_FOURCC, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_fourcc->SetToolTip(TIP("Forces the FourCC of the video track to this "
                             "value. Note that this only works for video "
                             "tracks that use the AVI compatibility mode "
                             "or for QuickTime video tracks. This option "
                             "CANNOT be used to change Matroska's CodecID."));
  cob_fourcc->SetSizeHints(0, -1);
  siz_fg->Add(cob_fourcc, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_stereo_mode = new wxStaticText(this, -1, wxT("Stereo mode:"));
  st_stereo_mode->Enable(false);
  siz_fg->Add(st_stereo_mode, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_stereo_mode =
    new wxComboBox(this, ID_CB_STEREO_MODE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_READONLY);
  cob_stereo_mode->SetToolTip(TIP("Sets the stereo mode of the video track to "
                                  "this value. If left empty then the track's "
                                  "original stereo mode will be kept or, if "
                                  "it didn't have one, none will be set at "
                                  "all."));
  cob_stereo_mode->SetSizeHints(0, -1);
  siz_fg->Add(cob_stereo_mode, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_fps = new wxStaticText(this, -1, wxT("FPS:"));
  st_fps->Enable(false);
  siz_fg->Add(st_fps, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_fps =
    new wxComboBox(this, ID_CB_FPS, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_fps->SetToolTip(TIP("Sets the default duration or number of frames "
                          "per second for a track. This is only possible "
                          "for input formats from which mkvmerge cannot "
                          "get this information itself. At the moment this "
                          "only includes AVC/h.264 elementary streams. "
                          "This can either be a floating point number or "
                          "a fraction."));
  cob_fps->SetSizeHints(0, -1);
  siz_fg->Add(cob_fps, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  siz_fg->Add(0, 0, 0, 0, 0);
  siz_fg->Add(0, 0, 0, 0, 0);

  st_delay = new wxStaticText(this, wxID_STATIC, wxT("Delay (in ms):"));
  st_delay->Enable(false);
  siz_fg->Add(st_delay, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_delay = new wxTextCtrl(this, ID_TC_DELAY, wxT(""));
  tc_delay->SetToolTip(TIP("Delay this track by a couple of ms. Can be "
                           "negative. Only applies to audio and subtitle "
                           "tracks. Some audio formats cannot be delayed at "
                           "the moment."));
  tc_delay->SetSizeHints(0, -1);
  siz_fg->Add(tc_delay, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_stretch = new wxStaticText(this, wxID_STATIC, wxT("Stretch by:"));
  st_stretch->Enable(false);
  siz_fg->Add(st_stretch, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_stretch = new wxTextCtrl(this, ID_TC_STRETCH, wxT(""));
  tc_stretch->SetToolTip(TIP("Stretch the audio or subtitle track by a "
                             "factor. This entry can have two formats. It is "
                             "either a positive floating point number, or a "
                             "fraction like e.g. 1200/1253. Not all formats "
                             "can be stretched at the moment."));
  tc_stretch->SetSizeHints(0, -1);
  siz_fg->Add(tc_stretch, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_sub_charset = new wxStaticText(this, wxID_STATIC,
                                    wxT("Subtitle charset:"));
  st_sub_charset->Enable(false);
  siz_fg->Add(st_sub_charset, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_sub_charset =
    new wxComboBox(this, ID_CB_SUBTITLECHARSET, wxT(""),
                   wxDefaultPosition, wxDefaultSize, 0, NULL,
                   wxCB_DROPDOWN | wxCB_READONLY);
  cob_sub_charset->SetToolTip(TIP("Selects the character set a subtitle file "
                                  "was written with. Only needed for non-UTF "
                                  "files that mkvmerge does not detect "
                                  "correctly."));
  cob_sub_charset->Append(wxT("default"));
  cob_sub_charset->SetSizeHints(0, -1);
  siz_fg->Add(cob_sub_charset, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_compression = new wxStaticText(this, -1, wxT("Compression:"));
  st_compression->Enable(false);
  siz_fg->Add(st_compression, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_compression =
    new wxComboBox(this, ID_CB_COMPRESSION, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_compression->SetToolTip(TIP("Sets the compression used for VobSub "
                                  "subtitles. If nothing is chosen then the "
                                  "VobSubs will be automatically compressed "
                                  "with zlib. 'none' results is files that "
                                  "are a lot larger."));
  cob_compression->SetSizeHints(0, -1);
  siz_fg->Add(cob_compression, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  cb_aac_is_sbr =
    new wxCheckBox(this, ID_CB_AACISSBR, wxT("AAC is SBR/HE-AAC/AAC+"));
  cb_aac_is_sbr->SetValue(false);
  cb_aac_is_sbr->SetToolTip(TIP("This track contains SBR AAC/HE-AAC/AAC+ data."
                                " Only needed for AAC input files, because SBR"
                                " AAC cannot be detected automatically for "
                                "these files. Not needed for AAC tracks read "
                                "from MP4 or Matroska files."));

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cb_aac_is_sbr, 0, wxALL, STDSPACING);
  siz_all->Add(siz_line, 0, wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_all->AddSpacer(TOPBOTTOMSPACING);

  SetSizer(siz_all);

  setup_control_contents();
}

void
tab_input_format::setup_control_contents() {
  int i;

  for (i = 0; NULL != predefined_fourccs[i]; ++i)
    cob_fourcc->Append(predefined_fourccs[i]);

  cob_stereo_mode->Append(wxT(""));
  cob_stereo_mode->Append(wxT("None"));
  cob_stereo_mode->Append(wxT("Left eye"));
  cob_stereo_mode->Append(wxT("Right eye"));
  cob_stereo_mode->Append(wxT("Both eyes"));

  cob_fps->Append(wxT(""));
  cob_fps->Append(wxT("24"));
  cob_fps->Append(wxT("25"));
  cob_fps->Append(wxT("30"));
  cob_fps->Append(wxT("30000/1001"));
  cob_fps->Append(wxT("24000/1001"));

  if (sorted_charsets.Count() == 0) {
    for (i = 0; sub_charsets[i] != NULL; i++)
      sorted_charsets.Add(wxU(sub_charsets[i]));
    sorted_charsets.Sort();
  }

  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_sub_charset->Append(sorted_charsets[i]);

  cob_compression->Append(wxT(""));
  cob_compression->Append(wxT("none"));
  cob_compression->Append(wxT("zlib"));
  if (capabilities[wxT("BZ2")] == wxT("true"))
    cob_compression->Append(wxT("bz2"));
  if (capabilities[wxT("LZO")] == wxT("true"))
    cob_compression->Append(wxT("lzo"));
}

void
tab_input_format::set_track_mode(mmg_track_t *t) {
  char type = t ? t->type : 'n';
  wxString ctype = t ? t->ctype : wxT("");
  bool appending = t ? t->appending : false;
  bool video = ('v' == type) && !appending;
  bool audio_app = ('a' == type);
  bool subs_app = ('s' == type);
  bool avc_es = video && (ctype.Find(wxT("MPEG-4 part 10 ES")) >= 0) &&
    (FILE_TYPE_AVC_ES == files[t->source].container);

  ctype = ctype.Lower();

  st_delay->Enable(audio_app || subs_app);
  tc_delay->Enable(audio_app || subs_app);
  st_stretch->Enable(audio_app || subs_app);
  tc_stretch->Enable(audio_app || subs_app);
  st_sub_charset->Enable(subs_app && (ctype.Find(wxT("vobsub")) < 0));
  cob_sub_charset->Enable(subs_app && (ctype.Find(wxT("vobsub")) < 0));
  st_fourcc->Enable(video);
  cob_fourcc->Enable(video);
  st_stereo_mode->Enable(video);
  cob_stereo_mode->Enable(video);
  st_fps->Enable(avc_es);
  cob_fps->Enable(avc_es);
  st_compression->Enable((ctype.Find(wxT("vobsub")) >= 0) && !appending);
  cob_compression->Enable((ctype.Find(wxT("vobsub")) >= 0) && !appending);

  bool ar_enabled = (NULL != t) && !t->display_dimensions_selected;
  rb_aspect_ratio->Enable(video);
  cob_aspect_ratio->Enable(video && ar_enabled);
  rb_display_dimensions->Enable(video);
  tc_display_width->Enable(video && !ar_enabled);
  st_x->Enable(video && !ar_enabled);
  tc_display_height->Enable(video && !ar_enabled);

  rb_aspect_ratio->SetValue(video && ar_enabled);
  rb_display_dimensions->SetValue(video && !ar_enabled);

  cb_aac_is_sbr->Enable(audio_app &&
                        ((ctype.Find(wxT("aac")) >= 0) ||
                         (ctype.Find(wxT("mp4a")) >= 0)));

  if (NULL == t) {
    bool saved_dcvn = input->dont_copy_values_now;
    input->dont_copy_values_now = true;

    set_combobox_selection(cob_aspect_ratio, wxT(""));
    tc_display_width->SetValue(wxT(""));
    tc_display_height->SetValue(wxT(""));
    set_combobox_selection(cob_fourcc, wxT(""));
    set_combobox_selection(cob_fps, wxT(""));
    set_combobox_selection(cob_stereo_mode, wxT(""));
    tc_delay->SetValue(wxT(""));
    tc_stretch->SetValue(wxT(""));
    set_combobox_selection(cob_sub_charset, wxU(""));
    set_combobox_selection(cob_compression, wxT(""));
    cb_aac_is_sbr->SetValue(false);

    input->dont_copy_values_now = saved_dcvn;
  }
}

void
tab_input_format::on_aac_is_sbr_clicked(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->aac_is_sbr = cb_aac_is_sbr->GetValue();
}

void
tab_input_format::on_subcharset_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->sub_charset =
    cob_sub_charset->GetStringSelection();
}

void
tab_input_format::on_delay_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->delay = tc_delay->GetValue();
}

void
tab_input_format::on_stretch_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->stretch = tc_stretch->GetValue();
}

void
tab_input_format::on_aspect_ratio_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->aspect_ratio =
    cob_aspect_ratio->GetStringSelection();
}

void
tab_input_format::on_fourcc_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->fourcc =
    cob_fourcc->GetStringSelection();
}

void
tab_input_format::on_fps_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->fps = cob_fps->GetStringSelection();
}

void
tab_input_format::on_stereo_mode_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->stereo_mode =
    cob_stereo_mode->GetSelection();
}

void
tab_input_format::on_compression_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->compression =
    cob_compression->GetStringSelection();
}

void
tab_input_format::on_display_width_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->dwidth = tc_display_width->GetValue();
}

void
tab_input_format::on_display_height_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->dheight = tc_display_height->GetValue();
}

void
tab_input_format::on_aspect_ratio_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  track = tracks[input->selected_track];
  track->display_dimensions_selected = false;
  set_track_mode(track);
}

void
tab_input_format::on_display_dimensions_selected(wxCommandEvent &evt) {
  mmg_track_t *track;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  track = tracks[input->selected_track];
  track->display_dimensions_selected = true;
  set_track_mode(track);
}

IMPLEMENT_CLASS(tab_input_format, wxPanel);
BEGIN_EVENT_TABLE(tab_input_format, wxPanel)
  EVT_CHECKBOX(ID_CB_AACISSBR, tab_input_format::on_aac_is_sbr_clicked)
  EVT_COMBOBOX(ID_CB_SUBTITLECHARSET, tab_input_format::on_subcharset_selected)
  EVT_COMBOBOX(ID_CB_ASPECTRATIO, tab_input_format::on_aspect_ratio_changed)
  EVT_COMBOBOX(ID_CB_FOURCC, tab_input_format::on_fourcc_changed)
  EVT_COMBOBOX(ID_CB_FPS, tab_input_format::on_fps_changed)
  EVT_COMBOBOX(ID_CB_STEREO_MODE, tab_input_format::on_stereo_mode_changed)
  EVT_TEXT(ID_CB_SUBTITLECHARSET, tab_input_format::on_subcharset_selected)
  EVT_TEXT(ID_TC_DELAY, tab_input_format::on_delay_changed)
  EVT_TEXT(ID_TC_STRETCH, tab_input_format::on_stretch_changed)
  EVT_RADIOBUTTON(ID_RB_ASPECTRATIO, tab_input_format::on_aspect_ratio_selected)
  EVT_RADIOBUTTON(ID_RB_DISPLAYDIMENSIONS,
                  tab_input_format::on_display_dimensions_selected)
  EVT_TEXT(ID_TC_DISPLAYWIDTH, tab_input_format::on_display_width_changed)
  EVT_TEXT(ID_TC_DISPLAYHEIGHT, tab_input_format::on_display_height_changed)
  EVT_COMBOBOX(ID_CB_COMPRESSION, tab_input_format::on_compression_selected)
END_EVENT_TABLE();
