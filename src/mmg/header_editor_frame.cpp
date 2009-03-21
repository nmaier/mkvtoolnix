/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/statline.h>

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
#include "mmg.h"
#include "wxcommon.h"

header_editor_frame_c::header_editor_frame_c(wxWindow *parent)
  : wxFrame(parent, wxID_ANY, Z("Header editor"), wxDefaultPosition, wxSize(800, 600))
  , m_file_menu(NULL)
  , m_file_menu_sep(false)
  , m_panel(NULL)
  , m_bs_panel(NULL)
  , m_tb_tree(NULL)
  , m_analyzer(NULL)
  , m_e_segment_info(NULL)
  , m_e_tracks(NULL)
{
  wxPanel *panel = new wxPanel(this);

  m_tb_tree = new wxTreebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);

  m_bs_panel = new wxBoxSizer(wxVERTICAL);
  m_bs_panel->Add(m_tb_tree, 1, wxGROW | wxALL, 5);

  panel->SetSizer(m_bs_panel);

  clear_pages();

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN,   Z("&Open\tCtrl-O"),   Z("Open an existing Matroska file"));
  m_file_menu->Append(ID_M_HE_FILE_SAVE,   Z("&Save\tCtrl-S"),   Z("Save the header values"));
  m_file_menu->Append(ID_M_HE_FILE_RELOAD, Z("&Reload\tCtrl-R"), Z("Reload the current file without saving"));
  m_file_menu->Append(ID_M_HE_FILE_CLOSE,  Z("&Close\tCtrl-W"),  Z("Close the current file without saving"));
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_QUIT,  Z("&Quit\tCtrl-Q"),  Z("Quit the header editor"));

  m_headers_menu = new wxMenu();
  m_headers_menu->Append(ID_M_HE_HEADERS_EXPAND_ALL,   Z("&Expand all entries\tCtrl-E"),   Z("Epand all entries so that their sub-entries will be shown"));
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
}

header_editor_frame_c::~header_editor_frame_c() {
  delete m_e_segment_info;
  delete m_e_tracks;
  delete m_analyzer;
}

bool
header_editor_frame_c::have_been_modified() {
  std::vector<he_page_base_c *>::iterator it = m_pages.begin();
  while (it != m_pages.end()) {
    if ((*it)->has_been_modified())
      return true;
    ++it;
  }

  return false;
}

void
header_editor_frame_c::do_modifications() {
  std::vector<he_page_base_c *>::iterator it = m_pages.begin();
  while (it != m_pages.end()) {
    (*it)->do_modifications();
    ++it;
  }
}

int
header_editor_frame_c::validate_pages() {
  std::vector<he_page_base_c *>::iterator it = m_pages.begin();
  while (it != m_pages.end()) {
    int result = (*it)->validate();
    if (-1 != result)
      return result;

    ++it;
  }

  return -1;
}

void
header_editor_frame_c::clear_pages() {
  m_bs_panel->Hide(m_tb_tree);

  m_tb_tree->DeleteAllPages();
  m_pages.clear();

  he_empty_page_c *page = new he_empty_page_c(m_tb_tree, Z("No file loaded"), Z("No file has been loaded yet. You can open a file by selecting 'Open' from the 'File' menu."));

  m_tb_tree->AddPage(page, Z("No file loaded"));

  m_bs_panel->Show(m_tb_tree);
  m_bs_panel->Layout();
}

void
header_editor_frame_c::on_file_open(wxCommandEvent &evt) {
  wxString home;
  wxGetEnv(wxT("HOME"), &home);
  // open_file(wxFileName(wxString::Format(wxT("%s/prog/video/mkvtoolnix/data/longfile.mkv"), home.c_str())));
  open_file(wxFileName(wxString::Format(wxT("%s/prog/video/mkvtoolnix/data/muh.mkv"), home.c_str())));
  return;

  wxFileDialog dlg(this, Z("Open a Matroska file"), last_open_dir, wxEmptyString, wxString::Format(Z("Matroska files (*.mkv;*.mka;*.mks)|*.mkv;*.mka;*.mks|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (!open_file(wxFileName(dlg.GetPath())))
    return;

  last_open_dir = dlg.GetDirectory();
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

  m_bs_panel->Hide(m_tb_tree);

  m_tb_tree->DeleteAllPages();
  m_pages.clear();

  int i;
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

  m_bs_panel->Show(m_tb_tree);
  m_bs_panel->Layout();

  return true;
}

void
header_editor_frame_c::handle_segment_info(analyzer_data_c *data) {
  EbmlElement *e = m_analyzer->read_element(data, KaxInfo::ClassInfos);
  if (NULL == e)
    return;

  he_top_level_page_c *page = new he_top_level_page_c(m_tb_tree, Z("Segment information"), e);
  m_tb_tree->AddPage(page, Z("Segment information"));
  m_pages.push_back(page);

  m_e_segment_info = e;
  KaxInfo *info    = static_cast<KaxInfo *>(e);
  he_value_page_c *child_page;

  child_page = new he_string_value_page_c(m_tb_tree, page, info, KaxTitle::ClassInfos, Z("Title"), Z("The title for the whole movie."));
  child_page->init();

  child_page = new he_string_value_page_c(m_tb_tree, page, info, KaxSegmentFilename::ClassInfos, Z("Segment filename"), Z("The file name for this segment."));
  child_page->init();

  child_page = new he_string_value_page_c(m_tb_tree, page, info, KaxPrevFilename::ClassInfos, Z("Previous filename"), Z("An escaped filename corresponding to the previous segment."));
  child_page->init();

  child_page = new he_string_value_page_c(m_tb_tree, page, info, KaxNextFilename::ClassInfos, Z("Next filename"), Z("An escaped filename corresponding to the next segment."));
  child_page->init();

  child_page = new he_bit_value_page_c(m_tb_tree, page, info, KaxSegmentUID::ClassInfos, Z("Segment unique ID"),
                                       Z("A randomly generated unique ID to identify the current segment between many others (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(m_tb_tree, page, info, KaxPrevUID::ClassInfos, Z("Previous segment's unique ID"),
                                       Z("A unique ID to identify the previous chained segment (128 bits)."), 128);
  child_page->init();

  child_page = new he_bit_value_page_c(m_tb_tree, page, info, KaxNextUID::ClassInfos, Z("Next segment's unique ID"),
                                       Z("A unique ID to identify the next chained segment (128 bits)."), 128);
  child_page->init();

  m_tb_tree->ExpandNode(page->m_page_id);
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

    he_top_level_page_c *page = new he_top_level_page_c(m_tb_tree, title, tracks);
    m_tb_tree->AddPage(page, title);
    m_pages.push_back(page);

    he_value_page_c *child_page;

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackNumber::ClassInfos, Z("Track number"),
                                                      Z("The track number as used in the Block Header."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackUID::ClassInfos, Z("Track UID"),
                                                      Z("A unique ID to identify the Track. This should be kept the same when making a direct stream copy of the Track to another file."));
    child_page->init();

    child_page = new he_bool_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackFlagDefault::ClassInfos, Z("'Default track' flag"),
                                          Z("Set if that track (audio, video or subs) SHOULD be used if no language found matches the user preference."));
    child_page->init();

    child_page = new he_bool_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackFlagEnabled::ClassInfos, Z("'Track enabled' flag"), Z("Set if the track is used."));
    child_page->init();

    child_page = new he_bool_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackFlagForced::ClassInfos, Z("'Forced display' flag"),
                                          Z("Set if that track MUST be used during playback. "
                                            "There can be many forced track for a kind (audio, video or subs). "
                                            "The player should select the one which language matches the user preference or the default + forced track."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackMinCache::ClassInfos, Z("Mininum cache"),
                                                      Z("The minimum number of frames a player should be able to cache during playback. "
                                                        "If set to 0, the reference pseudo-cache system is not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackMaxCache::ClassInfos, Z("Maxinum cache"),
                                                      Z("The maximum number of frames a player should be able to cache during playback. "
                                                        "If set to 0, the reference pseudo-cache system is not used."));
    child_page->init();

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackDefaultDuration::ClassInfos, Z("Default duration"),
                                                      Z("Number of nanoseconds (not scaled) per frame."));
    child_page->init();

    child_page = new he_float_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackTimecodeScale::ClassInfos, Z("Timecode scaling"),
                                           Z("The scale to apply on this track to work at normal speed in relation with other tracks "
                                             "(mostly used to adjust video speed when the audio length differs)."));
    child_page->init();

    child_page = new he_string_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackName::ClassInfos, Z("Name"), Z("A human-readable track name."));
    child_page->init();

    child_page = new he_language_value_page_c(m_tb_tree, page, k_track_entry, KaxTrackLanguage::ClassInfos, Z("Language"),
                                              Z("Specifies the language of the track in the Matroska languages form."));
    child_page->init();

    child_page = new he_ascii_string_value_page_c(m_tb_tree, page, k_track_entry, KaxCodecID::ClassInfos, Z("Codec ID"), Z("An ID corresponding to the codec."));
    child_page->init();

    child_page = new he_string_value_page_c(m_tb_tree, page, k_track_entry, KaxCodecName::ClassInfos, Z("Codec name"), Z("A human-readable string specifying the codec."));
    child_page->init();

    if (track_video == track_type) {
      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelWidth::ClassInfos,
                                                        Z("Video pixel width"), Z("Width of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelHeight::ClassInfos,
                                                        Z("Video pixel height"), Z("Height of the encoded video frames in pixels."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoDisplayWidth::ClassInfos,
                                                        Z("Video display width"), Z("Width of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoDisplayHeight::ClassInfos,
                                                        Z("Video display height"), Z("Height of the video frames to display."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoDisplayUnit::ClassInfos,
                                                        Z("Video display unit"), Z("Type of the unit for DisplayWidth/Height (0: pixels, 1: centimeters, 2: inches)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelCropLeft::ClassInfos,
                                                        Z("Video crop left"), Z("The number of video pixels to remove on the left of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelCropTop::ClassInfos,
                                                        Z("Video crop top"), Z("The number of video pixels to remove on the top of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelCropRight::ClassInfos,
                                                        Z("Video crop right"), Z("The number of video pixels to remove on the right of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoPixelCropBottom::ClassInfos,
                                                        Z("Video crop bottom"), Z("The number of video pixels to remove on the bottom of the image."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoAspectRatio::ClassInfos, Z("Video aspect ratio type"),
                                                        Z("Specify the possible modifications to the aspect ratio (0: free resizing, 1: keep aspect ratio, 2: fixed)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxVideoStereoMode::ClassInfos,
                                                        Z("Video stereo mode"), Z("Stereo-3D video mode (0: mono, 1: right eye, 2: left eye, 3: both eyes)."));
      child_page->set_sub_master_callbacks(KaxTrackVideo::ClassInfos);
      child_page->init();

    } else if (track_audio == track_type) {
      child_page = new he_float_value_page_c(m_tb_tree, page, k_track_entry, KaxAudioSamplingFreq::ClassInfos, Z("Audio sampling frequency"), Z("Sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_float_value_page_c(m_tb_tree, page, k_track_entry, KaxAudioOutputSamplingFreq::ClassInfos,
                                             Z("Audio output sampling frequency"), Z("Real output sampling frequency in Hz."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxAudioChannels::ClassInfos, Z("Audio channels"), Z("Numbers of channels in the track."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();

      child_page = new he_unsigned_integer_value_page_c(m_tb_tree, page, k_track_entry, KaxAudioBitDepth::ClassInfos, Z("Audio bit depth"), Z("Bits per sample, mostly used for PCM."));
      child_page->set_sub_master_callbacks(KaxTrackAudio::ClassInfos);
      child_page->init();
    }

    m_tb_tree->ExpandNode(page->m_page_id);
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

  int page_id = validate_pages();

  if (-1 != page_id) {
    m_tb_tree->SetSelection(page_id);
    wxMessageBox(Z("There were errors in the header values preventing the headers from being saved. The first error has been selected."), Z("Header validation"), wxOK | wxICON_ERROR, this);
    return;
  }

  do_modifications();

  std::vector<he_page_base_c *>::iterator it = m_pages.begin();
  bool tracks_written = false;
  while (it != m_pages.end()) {
    if ((*it)->has_been_modified()) {
      if ((*it)->m_l1_element->Generic().GlobalId == KaxTracks::ClassInfos.GlobalId) {
        if (tracks_written) {
          ++it;
          continue;
        }
        tracks_written = true;
      }

      kax_analyzer_c::update_element_result_e result = m_analyzer->update_element((*it)->m_l1_element, true);
      if (kax_analyzer_c::uer_success != result)
        display_update_element_result(result);
    }
    ++it;
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
  m_tb_tree->Freeze();
  int i;
  for (i = 0; m_tb_tree->GetTreeCtrl()->GetCount() > i; ++i)
    m_tb_tree->ExpandNode(i);
  m_tb_tree->Thaw();
}

void
header_editor_frame_c::on_headers_collapse_all(wxCommandEvent &evt) {
  m_tb_tree->Freeze();
  int i;
  for (i = 0; m_tb_tree->GetTreeCtrl()->GetCount() > i; ++i)
    m_tb_tree->CollapseNode(i);
  m_tb_tree->Thaw();
}

void
header_editor_frame_c::on_headers_validate(wxCommandEvent &evt) {
  validate();
}

bool
header_editor_frame_c::validate() {
  int page_id = validate_pages();

  if (-1 == page_id) {
    wxMessageBox(Z("All header values are OK."), Z("Header validation"), wxOK | wxICON_INFORMATION, this);
    return true;
  }

  m_tb_tree->SetSelection(page_id);
  wxMessageBox(Z("There were errors in the header values preventing the headers from being saved. The first error has been selected."), Z("Header validation"), wxOK | wxICON_ERROR, this);

  return false;
}

void
header_editor_frame_c::on_help_help(wxCommandEvent &evt) {
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
      wxMessageBox(Z("An unknown error occured. The file has not been modified."), Z("Internal program error"), wxOK | wxCENTER | wxICON_ERROR, this);
  }
}

IMPLEMENT_CLASS(header_editor_frame_c, wxFrame);
BEGIN_EVENT_TABLE(header_editor_frame_c, wxFrame)
  EVT_MENU(ID_M_HE_FILE_OPEN,               header_editor_frame_c::on_file_open)
  EVT_MENU(ID_M_HE_FILE_SAVE,               header_editor_frame_c::on_file_save)
  EVT_MENU(ID_M_HE_FILE_RELOAD,             header_editor_frame_c::on_file_reload)
  EVT_MENU(ID_M_HE_FILE_CLOSE,              header_editor_frame_c::on_file_close)
  EVT_MENU(ID_M_HE_FILE_QUIT,               header_editor_frame_c::on_file_quit)
  EVT_MENU(ID_M_HE_HEADERS_EXPAND_ALL,      header_editor_frame_c::on_headers_expand_all)
  EVT_MENU(ID_M_HE_HEADERS_COLLAPSE_ALL,    header_editor_frame_c::on_headers_collapse_all)
  EVT_MENU(ID_M_HE_HEADERS_VALIDATE,        header_editor_frame_c::on_headers_validate)
  EVT_MENU(ID_M_HE_HELP_HELP,               header_editor_frame_c::on_help_help)
  EVT_CLOSE(header_editor_frame_c::on_close_window)
END_EVENT_TABLE();
