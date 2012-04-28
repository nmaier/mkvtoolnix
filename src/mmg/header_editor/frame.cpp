/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// TODO:
// * wxStaticText is not wrapping on Windows.
// * Add "recent files" to the file menu.

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/dnd.h>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/ebml.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/header_editor/frame.h"
#include "mmg/header_editor/ascii_string_value_page.h"
#include "mmg/header_editor/bit_value_page.h"
#include "mmg/header_editor/bool_value_page.h"
#include "mmg/header_editor/empty_page.h"
#include "mmg/header_editor/float_value_page.h"
#include "mmg/header_editor/language_value_page.h"
#include "mmg/header_editor/string_value_page.h"
#include "mmg/header_editor/top_level_page.h"
#include "mmg/header_editor/track_type_page.h"
#include "mmg/header_editor/unsigned_integer_value_page.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"

#if !defined(SYS_WINDOWS)
# include "share/icons/64x64/mkvmergeGUI.h"
#endif

class header_editor_drop_target_c: public wxFileDropTarget {
private:
  header_editor_frame_c *m_owner;

public:
  header_editor_drop_target_c(header_editor_frame_c *owner)
    : m_owner(owner) { };

  virtual bool OnDropFiles(wxCoord x,
                           wxCoord y,
                           const wxArrayString &dropped_files) {
    return m_owner->on_drop_files(x, y, dropped_files);
  }
};

header_editor_frame_c::header_editor_frame_c(wxWindow *parent)
  : wxFrame(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(800, 600), wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL)
  , m_file_menu(nullptr)
  , m_file_menu_sep(false)
  , m_page_panel(nullptr)
  , m_bs_main(nullptr)
  , m_bs_page(nullptr)
  , m_ignore_tree_selection_changes(false)
{
  wxPanel *frame_panel = new wxPanel(this);

  m_tc_tree = new wxTreeCtrl(frame_panel, ID_HE_TC_TREE, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN | wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT | wxTR_SINGLE); //| wxTAB_TRAVERSAL);
  m_root_id = m_tc_tree->AddRoot(wxEmptyString);

  m_tc_tree->SetMinSize(wxSize(250, -1));

  m_page_panel = new wxPanel(frame_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
  m_bs_page    = new wxBoxSizer(wxHORIZONTAL);
  m_page_panel->SetSizer(m_bs_page);

  m_bs_main = new wxBoxSizer(wxHORIZONTAL);
  m_bs_main->Add(m_tc_tree,   2, wxGROW | wxALL, 5);
  m_bs_main->Add(m_page_panel,3, wxGROW | wxALL, 5);

  frame_panel->SetSizer(m_bs_main);

  SetMinSize(wxSize(800, 600));

  clear_pages();

  const wxString dummy(wxU("dummy"));

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN,               dummy);
  m_file_menu->Append(ID_M_HE_FILE_SAVE,               dummy);
  m_file_menu->Append(ID_M_HE_FILE_RELOAD,             dummy);
  m_file_menu->Append(ID_M_HE_FILE_CLOSE,              dummy);
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_QUIT,               dummy);

  m_headers_menu = new wxMenu();
  m_headers_menu->Append(ID_M_HE_HEADERS_EXPAND_ALL,   dummy);
  m_headers_menu->Append(ID_M_HE_HEADERS_COLLAPSE_ALL, dummy);
  m_headers_menu->AppendSeparator();
  m_headers_menu->Append(ID_M_HE_HEADERS_VALIDATE,     dummy);

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HE_HELP_HELP,                 dummy);

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(m_file_menu,                        dummy);
  menu_bar->Append(m_headers_menu,                     dummy);
  menu_bar->Append(help_menu,                          dummy);
  SetMenuBar(menu_bar);

  translate_ui();

  enable_menu_entries();

  m_status_bar = new wxStatusBar(this, wxID_ANY);
  SetStatusBar(m_status_bar);

  m_status_bar_timer.SetOwner(this, ID_T_HE_STATUS_BAR);

  SetIcon(wx_get_png_or_icon(mkvmergeGUI));
  SetDropTarget(new header_editor_drop_target_c(this));

  set_status_bar(Z("Header editor ready."));
}

header_editor_frame_c::~header_editor_frame_c() {
}

void
header_editor_frame_c::translate_ui() {
  set_menu_item_strings(this, ID_M_HE_FILE_OPEN,            Z("&Open\tCtrl-O"),                 Z("Open an existing Matroska file"));
  set_menu_item_strings(this, ID_M_HE_FILE_SAVE,            Z("&Save\tCtrl-S"),                 Z("Save the header values"));
  set_menu_item_strings(this, ID_M_HE_FILE_RELOAD,          Z("&Reload\tCtrl-R"),               Z("Reload the current file without saving"));
  set_menu_item_strings(this, ID_M_HE_FILE_CLOSE,           Z("&Close\tCtrl-W"),                Z("Close the current file without saving"));
  set_menu_item_strings(this, ID_M_HE_FILE_QUIT,            Z("&Quit\tCtrl-Q"),                 Z("Quit the header editor"));
  set_menu_item_strings(this, ID_M_HE_HEADERS_EXPAND_ALL,   Z("&Expand all entries\tCtrl-E"),   Z("Expand all entries so that their sub-entries will be shown"));
  set_menu_item_strings(this, ID_M_HE_HEADERS_COLLAPSE_ALL, Z("&Collapse all entries\tCtrl-P"), Z("Collapse all entries so that none of their sub-entries will be shown"));
  set_menu_item_strings(this, ID_M_HE_HEADERS_VALIDATE,     Z("&Validate\tCtrl-T"),             Z("Validates the content of all changeable headers"));
  set_menu_item_strings(this, ID_M_HE_HELP_HELP,            Z("&Help\tF1"),                     Z("Display usage information"));
  set_menu_label(this, 0, Z("&File"));
  set_menu_label(this, 1, Z("H&eaders"));
  set_menu_label(this, 2, Z("&Help"));

  set_window_title();

  for (auto &page : m_pages) {
    page->translate_ui();
    m_tc_tree->SetItemText(page->m_page_id, page->get_title());
  }

  // Really force wxWidgets/GTK to re-calculate the sizes of all
  // controls. Layout() on its own is not enough -- it won't calculate
  // the width/height for the new labels.
  Layout();
  SetSize(GetSize().GetWidth() + 1, GetSize().GetHeight());
}

void
header_editor_frame_c::set_window_title() {
  if (!m_analyzer)
    SetTitle(Z("Header editor"));
  else
    SetTitle(wxString::Format(Z("Header editor: %s"), m_file_name.GetFullName().c_str()));
}

bool
header_editor_frame_c::have_been_modified() {
  for (auto &page : m_top_level_pages)
    if (page->has_been_modified())
      return true;

  return false;
}

void
header_editor_frame_c::do_modifications() {
  for (auto &page : m_top_level_pages)
    page->do_modifications();
}

wxTreeItemId
header_editor_frame_c::validate_pages() {
  for (auto &page : m_top_level_pages) {
    wxTreeItemId result = page->validate();
    if (result.IsOk())
      return result;
  }

  return wxTreeItemId();
}

void
header_editor_frame_c::clear_pages() {
  m_ignore_tree_selection_changes = true;

  for (auto &page : m_pages)
    if (page->IsShown())
      page->Hide();

  m_bs_page->Clear();
  m_bs_main->Hide(m_tc_tree);

  m_tc_tree->DeleteChildren(m_root_id);
  m_pages.clear();
  m_top_level_pages.clear();

  he_empty_page_c *page = new he_empty_page_c(this, YT("No file loaded"), YT("No file has been loaded yet. You can open a file by selecting\n'Open' from the 'File' menu."));

  append_page(page);
  m_tc_tree->SelectItem(page->m_page_id);

  m_bs_main->Show(m_tc_tree);
  m_bs_main->Layout();

  m_ignore_tree_selection_changes = false;
}

void
header_editor_frame_c::on_file_open(wxCommandEvent &) {
  if (debugging_requested("he_open_std")) {
    wxString home;
    wxGetEnv(wxT("HOME"), &home);
    open_file(wxFileName(wxString::Format(wxT("%s/prog/video/mkvtoolnix/data/muh.mkv"), home.c_str())));
    return;
  }

  wxFileDialog dlg(this, Z("Open a Matroska file"), last_open_dir, wxEmptyString, wxString::Format(Z("Matroska files (*.mkv;*.mka;*.mks;*.mk3d)|*.mkv;*.mka;*.mks;*.mk3d|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if (dlg.ShowModal() != wxID_OK)
    return;

  open_file(wxFileName(dlg.GetPath()));
}

bool
header_editor_frame_c::open_file(wxFileName file_name) {
  if (!kax_analyzer_c::probe(wxMB(file_name.GetFullPath()))) {
    wxMessageBox(Z("The file you tried to open is not a Matroska file."), Z("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  m_e_segment_info.reset();
  m_e_tracks.reset();

  m_analyzer = wx_kax_analyzer_cptr(new wx_kax_analyzer_c(this, wxMB(file_name.GetFullPath())));

  if (!m_analyzer->process(kax_analyzer_c::parse_mode_fast)) {
    wxMessageBox(Z("This file could not be opened or parsed."), Z("File parsing failed"), wxOK | wxCENTER | wxICON_ERROR);
    m_analyzer.reset();

    return false;
  }

  m_file_name = file_name;
  m_file_name.GetTimes(nullptr, &m_file_mtime, nullptr);

  set_window_title();

  m_ignore_tree_selection_changes = true;

  enable_menu_entries();

  m_bs_main->Hide(m_tc_tree);

  for (auto &page : m_pages)
    if (page->IsShown())
      page->Hide();

  m_tc_tree->DeleteChildren(m_root_id);
  m_bs_page->Clear();
  m_pages.clear();
  m_top_level_pages.clear();

  for (auto &data : m_analyzer->m_data)
    if (data->m_id == KaxInfo::ClassInfos.GlobalId) {
      handle_segment_info(data.get());
      break;
    }

  for (auto &data : m_analyzer->m_data)
    if (data->m_id == KaxTracks::ClassInfos.GlobalId) {
      handle_tracks(data.get());
      break;
    }

  m_analyzer->close_file();

  m_bs_main->Show(m_tc_tree);
  m_bs_main->Layout();

  last_open_dir                   = file_name.GetPath();

  m_ignore_tree_selection_changes = false;

  return true;
}

void
header_editor_frame_c::handle_segment_info(kax_analyzer_data_c *data) {
  m_e_segment_info = m_analyzer->read_element(data);
  if (!m_e_segment_info)
    return;

  he_top_level_page_c *page = new he_top_level_page_c(this, YT("Segment information"), m_e_segment_info);
  page->init();

  KaxInfo *info    = static_cast<KaxInfo *>(m_e_segment_info.get());
  he_value_page_c *child_page;

  child_page = new he_string_value_page_c(this, page, info, KaxTitle::ClassInfos, YT("Title"), YT("The title for the whole movie."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxSegmentFilename::ClassInfos, YT("Segment filename"), YT("The file name for this segment."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxPrevFilename::ClassInfos, YT("Previous filename"), YT("An escaped filename corresponding to\nthe previous segment."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxNextFilename::ClassInfos, YT("Next filename"), YT("An escaped filename corresponding to\nthe next segment."));
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxSegmentUID::ClassInfos, YT("Segment unique ID"),
                                       YT("A randomly generated unique ID to identify the current\nsegment between many others (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxPrevUID::ClassInfos, YT("Previous segment's unique ID"),
                                       YT("A unique ID to identify the previous chained\nsegment (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxNextUID::ClassInfos, YT("Next segment's unique ID"),
                                       YT("A unique ID to identify the next chained\nsegment (128 bits)."), 128);
  child_page->init();

  // m_tc_tree->ExpandAllChildren(page->m_page_id);
}

void
header_editor_frame_c::handle_tracks(kax_analyzer_data_c *data) {
  m_e_tracks = m_analyzer->read_element(data);
  if (!m_e_tracks)
    return;

  he_track_type_page_c *last_track_page = nullptr;

  KaxTracks *kax_tracks = static_cast<KaxTracks *>(m_e_tracks.get());
  int track_type        = -1;
  size_t i;
  for (i = 0; kax_tracks->ListSize() > i; ++i) {
    KaxTrackEntry *k_track_entry = dynamic_cast<KaxTrackEntry *>((*kax_tracks)[i]);
    if (!k_track_entry)
      continue;

    KaxTrackType *k_track_type = dynamic_cast<KaxTrackType *>(FindChild<KaxTrackType>(k_track_entry));
    if (!k_track_type)
      continue;

    unsigned int track_number = 0;
    KaxTrackNumber *k_track_number = dynamic_cast<KaxTrackNumber *>(FindChild<KaxTrackNumber>(k_track_entry));
    if (k_track_number)
      track_number = uint64(*k_track_number);

    wxString title;
    track_type = uint64(*k_track_type);

    he_track_type_page_c *page = new he_track_type_page_c(this, track_type, track_number, m_e_tracks, *k_track_entry);
    last_track_page            = page;
    page->init();

    he_value_page_c *child_page;

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackNumber::ClassInfos, YT("Track number"),
                                                      YT("The track number as used in the Block Header."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackUID::ClassInfos, YT("Track UID"),
                                                      YT("A unique ID to identify the Track. This should be\nkept the same when making a direct stream copy\nof the Track to another file."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagDefault::ClassInfos, YT("'Default track' flag"),
                                          YT("Set if that track (audio, video or subs) SHOULD\nbe used if no language found matches the\nuser preference."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagEnabled::ClassInfos, YT("'Track enabled' flag"), YT("Set if the track is used."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagForced::ClassInfos, YT("'Forced display' flag"),
                                          YT("Set if that track MUST be used during playback.\n"
                                            "There can be many forced track for a kind (audio,\nvideo or subs). "
                                            "The player should select the one\nwhose language matches the user preference or the\ndefault + forced track."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackMinCache::ClassInfos, YT("Minimum cache"),
                                                      YT("The minimum number of frames a player\nshould be able to cache during playback.\n"
                                                        "If set to 0, the reference pseudo-cache system\nis not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackMaxCache::ClassInfos, YT("Maximum cache"),
                                                      YT("The maximum number of frames a player\nshould be able to cache during playback.\n"
                                                        "If set to 0, the reference pseudo-cache system\nis not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackDefaultDuration::ClassInfos, YT("Default duration"),
                                                      YT("Number of nanoseconds (not scaled) per frame."));
    child_page->init();

    child_page = new he_float_value_page_c(this, page, k_track_entry, KaxTrackTimecodeScale::ClassInfos, YT("Timecode scaling"),
                                           YT("The scale to apply on this track to work at normal\nspeed in relation with other tracks "
                                             "(mostly used\nto adjust video speed when the audio length differs)."));
    child_page->init();

    child_page = new he_string_value_page_c(this, page, k_track_entry, KaxTrackName::ClassInfos, YT("Name"), YT("A human-readable track name."));
    child_page->init();

    child_page = new he_language_value_page_c(this, page, k_track_entry, KaxTrackLanguage::ClassInfos, YT("Language"),
                                              YT("Specifies the language of the track in the\nMatroska languages form."));
    child_page->init();

    child_page = new he_ascii_string_value_page_c(this, page, k_track_entry, KaxCodecID::ClassInfos, YT("Codec ID"), YT("An ID corresponding to the codec."));
    child_page->init();

    child_page = new he_string_value_page_c(this, page, k_track_entry, KaxCodecName::ClassInfos, YT("Codec name"), YT("A human-readable string specifying the codec."));
    child_page->init();

    if (track_video == track_type) {
      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelWidth::ClassInfos,
                                                        YT("Video pixel width"), YT("Width of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelHeight::ClassInfos,
                                                        YT("Video pixel height"), YT("Height of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayWidth::ClassInfos,
                                                        YT("Video display width"), YT("Width of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayHeight::ClassInfos,
                                                        YT("Video display height"), YT("Height of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayUnit::ClassInfos,
                                                        YT("Video display unit"), YT("Type of the unit for DisplayWidth/Height\n(0: pixels, 1: centimeters, 2: inches)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropLeft::ClassInfos,
                                                        YT("Video crop left"), YT("The number of video pixels to remove\non the left of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropTop::ClassInfos,
                                                        YT("Video crop top"), YT("The number of video pixels to remove\non the top of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropRight::ClassInfos,
                                                        YT("Video crop right"), YT("The number of video pixels to remove\non the right of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropBottom::ClassInfos,
                                                        YT("Video crop bottom"), YT("The number of video pixels to remove\non the bottom of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoAspectRatio::ClassInfos, YT("Video aspect ratio type"),
                                                        YT("Specify the possible modifications to the aspect ratio\n(0: free resizing, 1: keep aspect ratio, 2: fixed)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoStereoMode::ClassInfos, YT("Video stereo mode"), YT("Stereo-3D video mode (0 - 11, see documentation)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

    } else if (track_audio == track_type) {
      child_page = new he_float_value_page_c(this, page, k_track_entry, KaxAudioSamplingFreq::ClassInfos, YT("Audio sampling frequency"), YT("Sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_float_value_page_c(this, page, k_track_entry, KaxAudioOutputSamplingFreq::ClassInfos,
                                             YT("Audio output sampling frequency"), YT("Real output sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxAudioChannels::ClassInfos, YT("Audio channels"), YT("Numbers of channels in the track."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxAudioBitDepth::ClassInfos, YT("Audio bit depth"), YT("Bits per sample, mostly used for PCM."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();
    }

    // m_tc_tree->ExpandAllChildren(page->m_page_id);
  }

  if (last_track_page)
    last_track_page->set_is_last_track(true);
}

void
header_editor_frame_c::on_file_save(wxCommandEvent &) {
  wxDateTime curr_mtime;
  m_file_name.GetTimes(nullptr, &curr_mtime, nullptr);

  if (curr_mtime != m_file_mtime) {
    wxMessageBox(Z("The file has been changed by another program since it was read by the header editor. Therefore you have to re-load it. "
                   "Unfortunately this means that all of your changes will be lost."), Z("File modified"), wxOK | wxICON_ERROR, this);
    return;
  }

  if (!have_been_modified()) {
    wxMessageBox(Z("None of the header fields has been modified. Nothing has been saved."), Z("No fields modified"), wxOK | wxICON_INFORMATION, this);
    return;
  }

  wxTreeItemId page_id = validate_pages();

  if (page_id.IsOk()) {
    m_tc_tree->SelectItem(page_id);
    wxMessageBox(Z("There were errors in the header values preventing the headers from being saved. The first error has been selected."), Z("Header validation"), wxOK | wxICON_ERROR, this);
    return;
  }

  do_modifications();

  bool tracks_written = false;
  for (auto &page : m_top_level_pages) {
    if (!page->has_been_modified())
      continue;

    if (page->m_l1_element->Generic().GlobalId == KaxTracks::ClassInfos.GlobalId) {
      if (tracks_written)
        continue;
      tracks_written = true;
    }

    kax_analyzer_c::update_element_result_e result = m_analyzer->update_element(page->m_l1_element, true);
    if (kax_analyzer_c::uer_success != result)
      display_update_element_result(result);
  }

  open_file(m_file_name);
}

void
header_editor_frame_c::on_file_reload(wxCommandEvent &) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to reload without saving the file?"), Z("Headers modified"), wxYES_NO | wxICON_QUESTION, this)))
    return;

  open_file(m_file_name);
}

void
header_editor_frame_c::on_file_close(wxCommandEvent &) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to close without saving the file?"), Z("Headers modified"), wxYES_NO | wxICON_QUESTION, this)))
    return;

  clear_pages();

  m_analyzer.reset();

  m_file_name.Clear();

  enable_menu_entries();
}

void
header_editor_frame_c::on_close_window(wxCloseEvent &evt) {
  if (!may_close() && evt.CanVeto())
    evt.Veto();
  else {
    mdlg->header_editor_frame_closed(this);
    Destroy();
  }
}

void
header_editor_frame_c::on_file_quit(wxCommandEvent &) {
  Close();
}

void
header_editor_frame_c::on_headers_expand_all(wxCommandEvent &) {
  m_tc_tree->Freeze();
  for (auto &page : m_pages)
    m_tc_tree->Expand(page->m_page_id);
  m_tc_tree->Thaw();
}

void
header_editor_frame_c::on_headers_collapse_all(wxCommandEvent &) {
  m_tc_tree->Freeze();
  for (auto &page : m_pages)
    m_tc_tree->Collapse(page->m_page_id);
  m_tc_tree->Thaw();
}

void
header_editor_frame_c::on_headers_validate(wxCommandEvent &) {
  validate();
}

bool
header_editor_frame_c::validate() {
  wxTreeItemId page_id = validate_pages();

  if (!page_id.IsOk()) {
    wxMessageBox(Z("All header values are OK."), Z("Header validation"), wxOK | wxICON_INFORMATION, this);
    return true;
  }

  m_tc_tree->SelectItem(page_id);
  wxMessageBox(Z("There were errors in the header values preventing the headers from being saved. The first error has been selected."), Z("Header validation"), wxOK | wxICON_ERROR, this);

  return false;
}

void
header_editor_frame_c::on_help_help(wxCommandEvent &) {
  mdlg->display_help(HELP_ID_HEADER_EDITOR);
}

bool
header_editor_frame_c::may_close() {
  return true;
}

void
header_editor_frame_c::update_file_menu() {
  int i;
  for (i = ID_M_HE_FILE_LOADLAST1; i <= ID_M_HE_FILE_LOADLAST4; i++) {
    wxMenuItem *mi = m_file_menu->Remove(i);
    delete mi;
  }
}

void
header_editor_frame_c::enable_menu_entries() {
  m_file_menu->Enable(ID_M_HE_FILE_SAVE,               m_file_name.IsOk());
  m_file_menu->Enable(ID_M_HE_FILE_RELOAD,             m_file_name.IsOk());
  m_file_menu->Enable(ID_M_HE_FILE_CLOSE,              m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_EXPAND_ALL,   m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_COLLAPSE_ALL, m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_VALIDATE,     m_file_name.IsOk());
}

void
header_editor_frame_c::display_update_element_result(kax_analyzer_c::update_element_result_e result) {
  switch (result) {
    case kax_analyzer_c::uer_success:
      return;

    case kax_analyzer_c::uer_error_segment_size_for_element:
      wxMessageBox(Z("The element was written at the end of the file, but the segment size could not be updated. Therefore the element will not be visible. "
                     "The process will be aborted. The file has been changed!"),
                   Z("Error writing Matroska file"), wxCENTER | wxOK | wxICON_ERROR, this);
      break;

    case kax_analyzer_c::uer_error_segment_size_for_meta_seek:
      wxMessageBox(Z("The meta seek element was written at the end of the file, but the segment size could not be updated. Therefore the element will not be visible. "
                     "The process will be aborted. The file has been changed!"),
                   Z("Error writing Matroska file"), wxCENTER | wxOK | wxICON_ERROR, this);
      break;

    case kax_analyzer_c::uer_error_meta_seek:
      wxMessageBox(Z("The Matroska file was modified, but the meta seek entry could not be updated. This means that players might have a hard time finding this element. "
                     "Please use your favorite player to check this file."),
                   Z("File structure warning"), wxOK | wxCENTER | wxICON_EXCLAMATION, this);
      break;

    default:
      wxMessageBox(Z("An unknown error occured. The file has been modified."), Z("Internal program error"), wxOK | wxCENTER | wxICON_ERROR, this);
  }
}

void
header_editor_frame_c::append_sub_page(he_page_base_c *page,
                                       wxTreeItemId parent_id) {
  page->translate_ui();

  wxTreeItemId id = m_tc_tree->AppendItem(parent_id, page->get_title());
  page->m_page_id = id;
  m_pages.push_back(he_page_base_cptr(page));

  if (parent_id == m_root_id)
    m_top_level_pages.push_back(page);
}

void
header_editor_frame_c::append_page(he_page_base_c *page) {
  append_sub_page(page, m_root_id);
}

he_page_base_c *
header_editor_frame_c::find_page_for_item(wxTreeItemId id) {
  for (auto &page : m_pages)
    if (page->m_page_id == id)
      return page.get();

  return nullptr;
}

void
header_editor_frame_c::on_tree_sel_changed(wxTreeEvent &evt) {
  if (m_ignore_tree_selection_changes)
    return;

  if (!evt.GetItem().IsOk())
    return;

  he_page_base_c *page = find_page_for_item(evt.GetItem());
  if (!page)
    return;

  m_page_panel->Freeze();

  for (auto &page : m_pages)
    if (page->IsShown())
      page->Hide();

  page->Show();

  m_bs_page->Clear();
  m_bs_page->Add(page, 1, wxGROW);
  m_bs_page->Layout();

  m_page_panel->Thaw();
}

void
header_editor_frame_c::set_status_bar(const wxString &text) {
  m_status_bar_timer.Stop();
  m_status_bar->SetStatusText(text);
  m_status_bar_timer.Start(4000, true);
}

void
header_editor_frame_c::on_status_bar_timer(wxTimerEvent &) {
  m_status_bar->SetStatusText(wxEmptyString);
}

bool
header_editor_frame_c::on_drop_files(wxCoord,
                                     wxCoord,
                                     const wxArrayString &dropped_files) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to load a new file without saving the current one?"), Z("Headers modified"), wxYES_NO | wxICON_QUESTION, this)))
    return false;

  open_file(wxFileName(dropped_files[0]));

  return true;
}

IMPLEMENT_CLASS(header_editor_frame_c, wxFrame);
BEGIN_EVENT_TABLE(header_editor_frame_c, wxFrame)
  EVT_TREE_SEL_CHANGED(ID_HE_TC_TREE,       header_editor_frame_c::on_tree_sel_changed)
  EVT_MENU(ID_M_HE_FILE_OPEN,               header_editor_frame_c::on_file_open)
  EVT_MENU(ID_M_HE_FILE_SAVE,               header_editor_frame_c::on_file_save)
  EVT_MENU(ID_M_HE_FILE_RELOAD,             header_editor_frame_c::on_file_reload)
  EVT_MENU(ID_M_HE_FILE_CLOSE,              header_editor_frame_c::on_file_close)
  EVT_MENU(ID_M_HE_FILE_QUIT,               header_editor_frame_c::on_file_quit)
  EVT_MENU(ID_M_HE_HEADERS_EXPAND_ALL,      header_editor_frame_c::on_headers_expand_all)
  EVT_MENU(ID_M_HE_HEADERS_COLLAPSE_ALL,    header_editor_frame_c::on_headers_collapse_all)
  EVT_MENU(ID_M_HE_HEADERS_VALIDATE,        header_editor_frame_c::on_headers_validate)
  EVT_MENU(ID_M_HE_HELP_HELP,               header_editor_frame_c::on_help_help)
  EVT_TIMER(ID_T_HE_STATUS_BAR,             header_editor_frame_c::on_status_bar_timer)
  EVT_CLOSE(header_editor_frame_c::on_close_window)
END_EVENT_TABLE();
