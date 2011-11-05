/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   update checker thread & dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#if defined(HAVE_CURL_EASY_H)

# include <wx/wx.h>
# include <wx/thread.h>

# include "common/version.h"
# include "mmg/mmg_dialog.h"
# include "mmg/update_checker.h"

DEFINE_EVENT_TYPE(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED);
update_check_thread_c::update_check_thread_c(mmg_dialog *mdlg)
  : wxThread(wxTHREAD_DETACHED)
  , m_mdlg(mdlg)
{
}

void *
update_check_thread_c::Entry() {
  wxCommandEvent event(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED, UPDATE_CHECK_START);
  wxPostEvent(m_mdlg, event);

  mtx_release_version_t release = get_latest_release_version();

  if (!release.valid)
    event.SetId(UPDATE_CHECK_DONE_ERROR);

  else {
    m_mdlg->set_release_version(release);
    event.SetId(release.current_version < release.latest_source ? UPDATE_CHECK_DONE_NEW_RELEASE : UPDATE_CHECK_DONE_NO_NEW_RELEASE);
  }

  wxPostEvent(m_mdlg, event);

  return NULL;
}

// ------------------------------------------------------------

update_check_dlg_c::update_check_dlg_c(wxWindow *parent)
  : wxDialog(parent, -1, Z("Online check for updates"))
{
  wxStaticText *st_status_label            = new wxStaticText(this, wxID_STATIC, Z("Status:"));
  m_st_status                              = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  wxStaticText *st_current_version_label   = new wxStaticText(this, wxID_STATIC, Z("Current version:"));
  wxStaticText *st_current_version         = new wxStaticText(this, wxID_STATIC, wxU(get_current_version().to_string()));

  wxStaticText *st_available_version_label = new wxStaticText(this, wxID_STATIC, Z("Available version:"));
  m_st_available_version                   = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  wxStaticText *st_download_url_label      = new wxStaticText(this, wxID_STATIC, Z("Download URL:"));
  const wxString default_url               = wxU("http://www.bunkus.org/videotools/mkvtoolnix/downloads.html");
  m_hlc_download_url                       = new wxHyperlinkCtrl(this, ID_HLC_DOWNLOAD_URL, default_url, default_url);

  m_b_close                                = new wxButton(this, ID_B_UPDATE_CHECK_CLOSE, Z("&Close"));
  m_b_close->Enable(false);

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(2, 5, 5);
  siz_fg->AddGrowableCol(0);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_status_label);
  siz_fg->Add(m_st_status);

  siz_fg->Add(st_current_version_label);
  siz_fg->Add(st_current_version);

  siz_fg->Add(st_available_version_label);
  siz_fg->Add(m_st_available_version);

  siz_fg->Add(st_download_url_label);
  siz_fg->Add(m_hlc_download_url);

  wxBoxSizer *siz_button = new wxBoxSizer(wxHORIZONTAL);
  siz_button->AddStretchSpacer();
  siz_button->Add(m_b_close);

  m_siz_all = new wxBoxSizer(wxVERTICAL);
  m_siz_all->Add(siz_fg,     0, wxALL                       | wxGROW, 5);
  m_siz_all->Add(siz_button, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxGROW, 5);

  SetSizer(m_siz_all);
}

void
update_check_dlg_c::update_status(const wxString &status) {
  m_st_status->SetLabel(status);
  readjust();
}

void
update_check_dlg_c::update_info(mtx_release_version_t &release) {
  if (release.valid) {
    m_st_available_version->SetLabel(wxU(release.latest_source.to_string()));
    m_hlc_download_url->SetLabel(wxU(release.source_download_url));
    m_hlc_download_url->SetURL(wxU(release.source_download_url));
  }
  m_b_close->Enable(true);
  readjust();
}

void
update_check_dlg_c::readjust() {
  m_siz_all->RecalcSizes();
  m_siz_all->SetSizeHints(this);
  CenterOnParent();
}

void
update_check_dlg_c::on_close(wxCloseEvent &evt) {
  close_dialog();
}

void
update_check_dlg_c::on_close_pressed(wxCommandEvent &evt) {
  close_dialog();
}

void
update_check_dlg_c::close_dialog() {
  wxCommandEvent event(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED, UPDATE_CHECK_DONE_DIALOG_DISMISSED);
  wxPostEvent(GetParent(), event);
  Destroy();
}

IMPLEMENT_CLASS(update_check_dlg_c, wxDialog);
BEGIN_EVENT_TABLE(update_check_dlg_c, wxDialog)
  EVT_BUTTON(ID_B_UPDATE_CHECK_CLOSE, update_check_dlg_c::on_close_pressed)
  EVT_CLOSE(update_check_dlg_c::on_close)
END_EVENT_TABLE();
#endif  // defined(HAVE_CURL_EASY_H)
