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

#include "os.h"

#include <wx/button.h>
#include <wx/dnd.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "commonebml.h"
#include "header_editor_frame.h"
#include "he_ascii_string_value_page.h"
#include "he_bit_value_page.h"
#include "he_bool_value_page.h"
#include "he_empty_page.h"
#include "he_float_value_page.h"
#include "he_language_value_page.h"
#include "he_string_value_page.h"
#include "he_top_level_page.h"
#include "he_unsigned_integer_value_page.h"
#include "matroskalogo.xpm"
#include "mmg.h"
#include "mmg_dialog.h"
#include "wxcommon.h"

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
  : wxFrame(parent, wxID_ANY, Z("Header editor"), wxDefaultPosition, wxSize(800, 600), wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL)
  , m_file_menu(NULL)
  , m_file_menu_sep(false)
  , m_page_panel(NULL)
  , m_bs_main(NULL)
  , m_bs_page(NULL)
  , m_analyzer(NULL)
  , m_e_segment_info(NULL)
  , m_e_tracks(NULL)
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

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN,   Z("&Open\tCtrl-O"),   Z("Open an existing Matroska file"));
  m_file_menu->Append(ID_M_HE_FILE_SAVE,   Z("&Save\tCtrl-S"),   Z("Save the header values"));
  m_file_menu->Append(ID_M_HE_FILE_RELOAD, Z("&Reload\tCtrl-R"), Z("Reload the current file without saving"));
  m_file_menu->Append(ID_M_HE_FILE_CLOSE,  Z("&Close\tCtrl-W"),  Z("Close the current file without saving"));
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_QUIT,  Z("&Quit\tCtrl-Q"),  Z("Quit the header editor"));

  m_headers_menu = new wxMenu();
  m_headers_menu->Append(ID_M_HE_HEADERS_EXPAND_ALL,   Z("&Expand all entries\tCtrl-E"),   Z("Expand all entries so that their sub-entries will be shown"));
  m_headers_menu->Append(ID_M_HE_HEADERS_COLLAPSE_ALL, Z("&Collapse all entries\tCtrl-P"), Z("Collapse all entries so that none of their sub-entries will be shown"));
  m_headers_menu->AppendSeparator();
  m_headers_menu->Append(ID_M_HE_HEADERS_VALIDATE,     Z("&Validate\tCtrl-T"),             Z("Validates the content of all changeable headers"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HE_HELP_HELP, Z("&Help\tF1"), Z("Display usage information"));

  enable_menu_entries();

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(m_file_menu,    Z("&File"));
  menu_bar->Append(m_headers_menu, Z("H&eaders"));
  menu_bar->Append(help_menu,      Z("&Help"));
  SetMenuBar(menu_bar);

  m_status_bar = new wxStatusBar(this, wxID_ANY);
  SetStatusBar(m_status_bar);

  m_status_bar_timer.SetOwner(this, ID_T_HE_STATUS_BAR);

  SetIcon(wxIcon(matroskalogo_xpm));
  SetDropTarget(new header_editor_drop_target_c(this));

  set_status_bar(Z("Header editor ready."));
}

header_editor_frame_c::~header_editor_frame_c() {
  delete m_e_segment_info;
  delete m_e_tracks;
  delete m_analyzer;
}

bool
header_editor_frame_c::have_been_modified() {
  for (int i = 0; m_top_level_pages.size() > i; ++i)
    if (m_top_level_pages[i]->has_been_modified())
      return true;

  return false;
}

void
header_editor_frame_c::do_modifications() {
  for (int i = 0; m_top_level_pages.size() > i; ++i)
    m_top_level_pages[i]->do_modifications();
}

wxTreeItemId
header_editor_frame_c::validate_pages() {
  for (int i = 0; m_top_level_pages.size() > i; ++i) {
    wxTreeItemId result = m_top_level_pages[i]->validate();
    if (result.IsOk())
      return result;
  }

  return wxTreeItemId();
}

void
header_editor_frame_c::clear_pages() {
  for (int i = 0; m_pages.size() > i; ++i)
    m_pages[i]->Hide();

  m_bs_main->Hide(m_tc_tree);

  m_tc_tree->DeleteChildren(m_root_id);
  m_pages.clear();
  m_top_level_pages.clear();

  he_empty_page_c *page = new he_empty_page_c(this, Z("No file loaded"), Z("No file has been loaded yet. You can open a file by selecting\n'Open' from the 'File' menu."));

  append_page(page, Z("No file loaded"));
  m_tc_tree->SelectItem(page->m_page_id);

  m_bs_main->Show(m_tc_tree);
  m_bs_main->Layout();
}

void
header_editor_frame_c::on_file_open(wxCommandEvent &evt) {
  wxString env_value;
  if (wxGetEnv(wxT("MTX_DEBUG"), &env_value) && (env_value == wxT("1"))) {
    wxGetEnv(wxT("HOME"), &env_value);
    open_file(wxFileName(wxString::Format(wxT("%s/prog/video/mkvtoolnix/data/muh.mkv"), env_value.c_str())));
    return;
  }

  wxFileDialog dlg(this, Z("Open a Matroska file"), last_open_dir, wxEmptyString, wxString::Format(Z("Matroska files (*.mkv;*.mka;*.mks)|*.mkv;*.mka;*.mks|%s"), ALLFILES.c_str()), wxFD_OPEN);
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

  delete m_e_segment_info;
  delete m_e_tracks;
  m_e_segment_info = NULL;
  m_e_tracks       = NULL;

  delete m_analyzer;
  m_analyzer = new kax_analyzer_c(this, wxMB(file_name.GetFullPath()));
  if (!m_analyzer->process()) {
    delete m_analyzer;
    m_analyzer = NULL;
    return false;
  }

  m_file_name = file_name;
  m_file_name.GetTimes(NULL, &m_file_mtime, NULL);

  SetTitle(wxString::Format(Z("Header editor: %s"), m_file_name.GetFullName().c_str()));

  enable_menu_entries();

  m_bs_main->Hide(m_tc_tree);

  int i;
  for (i = 0; m_pages.size() > i; ++i)
    m_pages[i]->Hide();

  m_tc_tree->DeleteChildren(m_root_id);
  m_pages.clear();
  m_top_level_pages.clear();

  for (i = 0; m_analyzer->data.size() > i; ++i) {
    analyzer_data_c *data = m_analyzer->data[i];
    if (data->id == KaxInfo::ClassInfos.GlobalId) {
      handle_segment_info(data);
      break;
    }
  }

  for (i = 0; m_analyzer->data.size() > i; ++i) {
    analyzer_data_c *data = m_analyzer->data[i];
    if (data->id == KaxTracks::ClassInfos.GlobalId) {
      handle_tracks(data);
      break;
    }
  }

  m_bs_main->Show(m_tc_tree);
  m_bs_main->Layout();

  last_open_dir = file_name.GetPath();

  return true;
}

void
header_editor_frame_c::handle_segment_info(analyzer_data_c *data) {
  EbmlElement *e = m_analyzer->read_element(data, KaxInfo::ClassInfos);
  if (NULL == e)
    return;

  he_top_level_page_c *page = new he_top_level_page_c(this, Z("Segment information"), e);

  m_e_segment_info = e;
  KaxInfo *info    = static_cast<KaxInfo *>(e);
  he_value_page_c *child_page;

  child_page = new he_string_value_page_c(this, page, info, KaxTitle::ClassInfos, Z("Title"), Z("The title for the whole movie."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxSegmentFilename::ClassInfos, Z("Segment filename"), Z("The file name for this segment."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxPrevFilename::ClassInfos, Z("Previous filename"), Z("An escaped filename corresponding to\nthe previous segment."));
  child_page->init();

  child_page = new he_string_value_page_c(this, page, info, KaxNextFilename::ClassInfos, Z("Next filename"), Z("An escaped filename corresponding to\nthe next segment."));
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxSegmentUID::ClassInfos, Z("Segment unique ID"),
                                       Z("A randomly generated unique ID to identify the current\nsegment between many others (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxPrevUID::ClassInfos, Z("Previous segment's unique ID"),
                                       Z("A unique ID to identify the previous chained\nsegment (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(this, page, info, KaxNextUID::ClassInfos, Z("Next segment's unique ID"),
                                       Z("A unique ID to identify the next chained\nsegment (128 bits)."), 128);
  child_page->init();

  // m_tc_tree->ExpandAllChildren(page->m_page_id);
}

void
header_editor_frame_c::handle_tracks(analyzer_data_c *data) {
  EbmlElement *e = m_analyzer->read_element(data, KaxTracks::ClassInfos);
  if (NULL == e)
    return;

  m_e_tracks        = e;
  KaxTracks *tracks = static_cast<KaxTracks *>(e);
  int track_type    = -1;
  int i;
  for (i = 0; tracks->ListSize() > i; ++i) {
    KaxTrackEntry *k_track_entry = dynamic_cast<KaxTrackEntry *>((*tracks)[i]);
    if (NULL == k_track_entry)
      continue;

    KaxTrackType *k_track_type = dynamic_cast<KaxTrackType *>(FINDFIRST(k_track_entry, KaxTrackType));
    if (NULL == k_track_type)
      continue;

    unsigned int track_number = 0;
    KaxTrackNumber *k_track_number = dynamic_cast<KaxTrackNumber *>(FINDFIRST(k_track_entry, KaxTrackNumber));
    if (NULL != k_track_number)
      track_number = uint64(*k_track_number);

    wxString title;
    track_type = uint64(*k_track_type);
    switch (track_type) {
      case track_audio:
        title.Printf(Z("Audio track %u"), track_number);
        break;
      case track_video:
        title.Printf(Z("Video track %u"), track_number);
        break;
      case track_subtitle:
        title.Printf(Z("Subtitle track %u"), track_number);
        break;
      default:
        continue;
    }

    he_top_level_page_c *page = new he_top_level_page_c(this, title, tracks);

    he_value_page_c *child_page;

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackNumber::ClassInfos, Z("Track number"),
                                                      Z("The track number as used in the Block Header."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackUID::ClassInfos, Z("Track UID"),
                                                      Z("A unique ID to identify the Track. This should be\nkept the same when making a direct stream copy\nof the Track to another file."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagDefault::ClassInfos, Z("'Default track' flag"),
                                          Z("Set if that track (audio, video or subs) SHOULD\nbe used if no language found matches the\nuser preference."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagEnabled::ClassInfos, Z("'Track enabled' flag"), Z("Set if the track is used."));
    child_page->init();

    child_page = new he_bool_value_page_c(this, page, k_track_entry, KaxTrackFlagForced::ClassInfos, Z("'Forced display' flag"),
                                          Z("Set if that track MUST be used during playback.\n"
                                            "There can be many forced track for a kind (audio,\nvideo or subs). "
                                            "The player should select the one\nwhose language matches the user preference or the\ndefault + forced track."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackMinCache::ClassInfos, Z("Minimum cache"),
                                                      Z("The minimum number of frames a player\nshould be able to cache during playback.\n"
                                                        "If set to 0, the reference pseudo-cache system\nis not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackMaxCache::ClassInfos, Z("Maximum cache"),
                                                      Z("The maximum number of frames a player\nshould be able to cache during playback.\n"
                                                        "If set to 0, the reference pseudo-cache system\nis not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxTrackDefaultDuration::ClassInfos, Z("Default duration"),
                                                      Z("Number of nanoseconds (not scaled) per frame."));
    child_page->init();

    child_page = new he_float_value_page_c(this, page, k_track_entry, KaxTrackTimecodeScale::ClassInfos, Z("Timecode scaling"),
                                           Z("The scale to apply on this track to work at normal\nspeed in relation with other tracks "
                                             "(mostly used\nto adjust video speed when the audio length differs)."));
    child_page->init();

    child_page = new he_string_value_page_c(this, page, k_track_entry, KaxTrackName::ClassInfos, Z("Name"), Z("A human-readable track name."));
    child_page->init();

    child_page = new he_language_value_page_c(this, page, k_track_entry, KaxTrackLanguage::ClassInfos, Z("Language"),
                                              Z("Specifies the language of the track in the\nMatroska languages form."));
    child_page->init();

    child_page = new he_ascii_string_value_page_c(this, page, k_track_entry, KaxCodecID::ClassInfos, Z("Codec ID"), Z("An ID corresponding to the codec."));
    child_page->init();

    child_page = new he_string_value_page_c(this, page, k_track_entry, KaxCodecName::ClassInfos, Z("Codec name"), Z("A human-readable string specifying the codec."));
    child_page->init();

    if (track_video == track_type) {
      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelWidth::ClassInfos,
                                                        Z("Video pixel width"), Z("Width of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelHeight::ClassInfos,
                                                        Z("Video pixel height"), Z("Height of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayWidth::ClassInfos,
                                                        Z("Video display width"), Z("Width of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayHeight::ClassInfos,
                                                        Z("Video display height"), Z("Height of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoDisplayUnit::ClassInfos,
                                                        Z("Video display unit"), Z("Type of the unit for DisplayWidth/Height\n(0: pixels, 1: centimeters, 2: inches)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropLeft::ClassInfos,
                                                        Z("Video crop left"), Z("The number of video pixels to remove\non the left of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropTop::ClassInfos,
                                                        Z("Video crop top"), Z("The number of video pixels to remove\non the top of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropRight::ClassInfos,
                                                        Z("Video crop right"), Z("The number of video pixels to remove\non the right of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoPixelCropBottom::ClassInfos,
                                                        Z("Video crop bottom"), Z("The number of video pixels to remove\non the bottom of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoAspectRatio::ClassInfos, Z("Video aspect ratio type"),
                                                        Z("Specify the possible modifications to the aspect ratio\n(0: free resizing, 1: keep aspect ratio, 2: fixed)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxVideoStereoMode::ClassInfos,
                                                        Z("Video stereo mode"), Z("Stereo-3D video mode (0: mono, 1: right eye,\n2: left eye, 3: both eyes)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

    } else if (track_audio == track_type) {
      child_page = new he_float_value_page_c(this, page, k_track_entry, KaxAudioSamplingFreq::ClassInfos, Z("Audio sampling frequency"), Z("Sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_float_value_page_c(this, page, k_track_entry, KaxAudioOutputSamplingFreq::ClassInfos,
                                             Z("Audio output sampling frequency"), Z("Real output sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxAudioChannels::ClassInfos, Z("Audio channels"), Z("Numbers of channels in the track."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(this, page, k_track_entry, KaxAudioBitDepth::ClassInfos, Z("Audio bit depth"), Z("Bits per sample, mostly used for PCM."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();
    }

    // m_tc_tree->ExpandAllChildren(page->m_page_id);
  }
}

void
header_editor_frame_c::on_file_save(wxCommandEvent &evt) {
  wxDateTime curr_mtime;
  m_file_name.GetTimes(NULL, &curr_mtime, NULL);

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

  int i;
  bool tracks_written = false;
  for (i = 0; m_top_level_pages.size() > i; ++i) {
    if (m_top_level_pages[i]->has_been_modified()) {
      if (m_top_level_pages[i]->m_l1_element->Generic().GlobalId == KaxTracks::ClassInfos.GlobalId) {
        if (tracks_written)
          continue;
        tracks_written = true;
      }

      kax_analyzer_c::update_element_result_e result = m_analyzer->update_element(m_top_level_pages[i]->m_l1_element, true);
      if (kax_analyzer_c::uer_success != result)
        display_update_element_result(result);
    }
  }

  open_file(m_file_name);
}

void
header_editor_frame_c::on_file_reload(wxCommandEvent &evt) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to reload without saving the file?"), Z("Headers modified"),
                                wxYES_NO | wxICON_QUESTION, this)))
    return;

  open_file(m_file_name);
}

void
header_editor_frame_c::on_file_close(wxCommandEvent &evt) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to close without saving the file?"), Z("Headers modified"),
                                wxYES_NO | wxICON_QUESTION, this)))
    return;

  clear_pages();

  delete m_analyzer;
  m_analyzer = NULL;

  m_file_name.Clear();

  enable_menu_entries();
}

void
header_editor_frame_c::on_close_window(wxCloseEvent &evt) {
  if (!may_close() && evt.CanVeto())
    evt.Veto();
  else
    Destroy();
}

void
header_editor_frame_c::on_file_quit(wxCommandEvent &evt) {
  Close();
}

void
header_editor_frame_c::on_headers_expand_all(wxCommandEvent &evt) {
  m_tc_tree->Freeze();
  int i;
  for (i = 0; m_pages.size() > i; ++i)
    m_tc_tree->Expand(m_pages[i]->m_page_id);
  m_tc_tree->Thaw();
}

void
header_editor_frame_c::on_headers_collapse_all(wxCommandEvent &evt) {
  m_tc_tree->Freeze();
  int i;
  for (i = 0; m_pages.size() > i; ++i)
    m_tc_tree->Collapse(m_pages[i]->m_page_id);
  m_tc_tree->Thaw();
}

void
header_editor_frame_c::on_headers_validate(wxCommandEvent &evt) {
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
header_editor_frame_c::on_help_help(wxCommandEvent &evt) {
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

  // if ((last_settings.size() > 0) && !file_menu_sep) {
  //   file_menu->AppendSeparator();
  //   file_menu_sep = true;
  // }
  // for (i = 0; i < last_settings.size(); i++) {
  //   s.Printf(wxT("&%u. %s"), i + 1, last_settings[i].c_str());
  //   file_menu->Append(ID_M_FILE_LOADLAST1 + i, s);
  // }
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
                     "Please use your favorite player to check this file.\n"),
                   Z("File structure warning"), wxOK | wxCENTER | wxICON_EXCLAMATION, this);
      break;

    default:
      wxMessageBox(Z("An unknown error occured. The file has been modified."), Z("Internal program error"), wxOK | wxCENTER | wxICON_ERROR, this);
  }
}

void
header_editor_frame_c::append_sub_page(he_page_base_c *page,
                                       const wxString &title,
                                       wxTreeItemId parent_id) {
  wxTreeItemId id = m_tc_tree->AppendItem(parent_id, title);
  page->m_page_id = id;
  m_pages.push_back(page);

  if (parent_id == m_root_id)
    m_top_level_pages.push_back(page);
}

void
header_editor_frame_c::append_page(he_page_base_c *page,
                                   const wxString &title) {
  append_sub_page(page, title, m_root_id);
}

he_page_base_c *
header_editor_frame_c::find_page_for_item(wxTreeItemId id) {
  for (int i = 0; m_pages.size() > i; ++i)
    if (m_pages[i]->m_page_id == id)
      return m_pages[i];

  return NULL;
}

void
header_editor_frame_c::on_tree_sel_changed(wxTreeEvent &evt) {
  m_page_panel->Freeze();

  if (evt.GetOldItem().IsOk()) {
    he_page_base_c *page = find_page_for_item(evt.GetOldItem());
    if (NULL != page)
      page->Hide();
  }

  if (!evt.GetItem().IsOk()) {
    m_page_panel->Thaw();
    return;
  }

  he_page_base_c *page = find_page_for_item(evt.GetItem());

  if (!page) {
    m_page_panel->Thaw();
    return;
  }

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
header_editor_frame_c::on_status_bar_timer(wxTimerEvent &evt) {
  m_status_bar->SetStatusText(wxEmptyString);
}

bool
header_editor_frame_c::on_drop_files(wxCoord x,
                                     wxCoord y,
                                     const wxArrayString &dropped_files) {
  if (   have_been_modified()
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to load a new file without saving the current one?"), Z("Headers modified"),
                                wxYES_NO | wxICON_QUESTION, this)))
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
