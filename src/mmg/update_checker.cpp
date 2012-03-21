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
# include <wx/richtext/richtextstyles.h>
# include <wx/thread.h>

# include "common/version.h"
# include "common/xml/pugi.h"
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
    m_mdlg->set_releases_info(get_releases_info());
    event.SetId(release.current_version < release.latest_source ? UPDATE_CHECK_DONE_NEW_RELEASE : UPDATE_CHECK_DONE_NO_NEW_RELEASE);
  }

  wxPostEvent(m_mdlg, event);

  return nullptr;
}

// ------------------------------------------------------------

update_check_dlg_c::update_check_dlg_c(wxWindow *parent)
  : wxDialog(parent, -1, Z("Online check for updates"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
{
  wxStaticText *st_status_label            = new wxStaticText(this, wxID_STATIC, Z("Status:"));
  m_st_status                              = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  wxStaticText *st_current_version_label   = new wxStaticText(this, wxID_STATIC, Z("Current version:"));
  wxStaticText *st_current_version         = new wxStaticText(this, wxID_STATIC, wxU(get_current_version().to_string()));

  wxStaticText *st_available_version_label = new wxStaticText(this, wxID_STATIC, Z("Available version:"));
  m_st_available_version                   = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  wxStaticText *st_download_url_label      = new wxStaticText(this, wxID_STATIC, Z("Download URL:"));
  const wxString default_url               = wxU(MTX_DOWNLOAD_URL);
  m_hlc_download_url                       = new wxHyperlinkCtrl(this, ID_HLC_DOWNLOAD_URL, default_url, default_url);

  m_changelog                              = new wxRichTextCtrl(this, ID_RE_CHANGELOG, wxString(), wxDefaultPosition, wxDefaultSize, wxRE_MULTILINE | wxRE_READONLY);

  m_b_close                                = new wxButton(this, ID_B_UPDATE_CHECK_CLOSE, Z("&Close"));
  m_b_close->Enable(false);

  setup_changelog_ctrl();

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(2, 5, 5);
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
  m_siz_all->Add(siz_fg,      0, wxALL                       | wxGROW, 5);
  m_siz_all->Add(m_changelog, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxGROW, 5);
  m_siz_all->Add(siz_button,  0, wxLEFT | wxRIGHT | wxBOTTOM | wxGROW, 5);

  SetMinSize(wxSize(400, 400));
  SetSizer(m_siz_all);
  SetSize(wxSize(700, 500));
}

void
update_check_dlg_c::setup_changelog_ctrl() {
  wxRichTextAttr default_char_style;
  default_char_style.SetFontSize(10);

  wxRichTextAttr url_char_style(wxColour(0, 0, 255));
  url_char_style.SetURL(wxU(MTX_CHANGELOG_URL));
  url_char_style.SetFontUnderlined(true);
  url_char_style.SetFlags(url_char_style.GetFlags() | wxTEXT_ATTR_TEXT_COLOUR);

  wxRichTextAttr heading_char_style;
  heading_char_style.SetFontSize(12);
  heading_char_style.SetFontStyle(wxBOLD);
  heading_char_style.SetAlignment(wxTEXT_ALIGNMENT_LEFT);
  heading_char_style.SetParagraphSpacingBefore(30);
  heading_char_style.SetParagraphSpacingAfter(30);

  wxRichTextAttr title_char_style;
  title_char_style.SetFontSize(14);
  title_char_style.SetFontWeight(wxBOLD);
  title_char_style.SetAlignment(wxTEXT_ALIGNMENT_LEFT);
  title_char_style.SetParagraphSpacingBefore(20);
  title_char_style.SetParagraphSpacingAfter(50);

  auto *style_sheet = new wxRichTextStyleSheet;
  auto add_char_style = [&](std::string const &name,
                            wxRichTextAttr &style) {
    auto style_def = new wxRichTextCharacterStyleDefinition(wxU(name));
    style_def->SetStyle(style);
    style_sheet->AddCharacterStyle(style_def);
  };

  add_char_style("default", default_char_style);
  add_char_style("title",   title_char_style);
  add_char_style("heading", heading_char_style);
  add_char_style("url",     url_char_style);

  m_changelog->SetStyleSheet(style_sheet);
  m_changelog->SetDefaultStyle(default_char_style);

  m_changelog->BeginSuppressUndo();

  write_changelog_title();
  m_changelog->WriteText(wxU("Loading, please be patient..."));
}

void
update_check_dlg_c::write_changelog_title() {
  m_changelog->BeginStyle(m_changelog->GetStyleSheet()->FindCharacterStyle(wxU("title"))->GetStyle());
  m_changelog->WriteText(wxT("MKVToolNix ChangeLog"));
  m_changelog->Newline();
  m_changelog->EndStyle();
}

void
update_check_dlg_c::update_status(const wxString &status) {
  m_st_status->SetLabel(status);
}

void
update_check_dlg_c::update_info(mtx_release_version_t const &version,
                                mtx::xml::document_cptr const &releases_info) {
  if (version.valid) {
    m_st_available_version->SetLabel(wxU(version.latest_source.to_string()));
    m_hlc_download_url->SetLabel(wxU(version.source_download_url));
    m_hlc_download_url->SetURL(wxU(version.source_download_url));

    if (releases_info)
      update_changelog(releases_info);
  }

  m_b_close->Enable(true);
}

void
update_check_dlg_c::update_changelog(mtx::xml::document_cptr const &releases_info) {
  auto current_version = get_current_version();

  m_changelog->Freeze();

  m_changelog->Clear();
  write_changelog_title();

  auto releases = releases_info->select_nodes("/mkvtoolnix-releases/release[not(@version='HEAD')]");
  releases.sort();

  size_t num_releases_output = 0;
  for (auto &release : releases) {
    m_changelog->BeginStyle(m_changelog->GetStyleSheet()->FindCharacterStyle(wxU("heading"))->GetStyle());

    std::string codename    = release.node().attribute("codename").value();
    std::string version_str = release.node().attribute("version").value();
    wxString heading;
    if (!codename.empty())
      heading.Printf(wxU("Version %s \"%s\""), wxU(version_str).c_str(), wxU(codename).c_str());
    else
      heading.Printf(wxU("Version %s"), wxU(version_str).c_str());
    m_changelog->WriteText(heading);
    m_changelog->Newline();
    m_changelog->EndStyle();

    boost::regex re_released("^released\\s+v?[\\d\\.]+", boost::regex::perl | boost::regex::icase);

    for (auto change = release.node().child("changes").first_child() ; change ; change = change.next_sibling()) {
      if (   (std::string(change.name()) != "change")
             || boost::regex_search(change.child_value(), re_released))
        continue;

      m_changelog->BeginStandardBullet(wxU("standard/circle"), 50, 40);
      m_changelog->WriteText(wxU(change.child_value()));
      m_changelog->Newline();
      m_changelog->EndStandardBullet();
    }

    num_releases_output++;
    if ((10 < num_releases_output) && (version_number_t(version_str) < current_version))
      break;
  }

  m_changelog->BeginURL(wxU(MTX_CHANGELOG_URL), wxU("url"));
  m_changelog->WriteText(wxT("Read full ChangeLog online"));
  m_changelog->EndURL();

  m_changelog->Thaw();
}

void
update_check_dlg_c::update_changelog_failed() {
  m_changelog->Freeze();

  m_changelog->Clear();
  write_changelog_title();

  m_changelog->WriteText(wxU(wxT("The ChangeLog could not be downloaded.")));
  m_changelog->Newline();
  m_changelog->Newline();

  m_changelog->BeginURL(wxU(MTX_CHANGELOG_URL), wxU("url"));
  m_changelog->WriteText(wxT("You can read it online."));
  m_changelog->EndURL();

  m_changelog->Thaw();
}

void
update_check_dlg_c::on_close(wxCloseEvent &) {
  close_dialog();
}

void
update_check_dlg_c::on_close_pressed(wxCommandEvent &) {
  close_dialog();
}

void
update_check_dlg_c::on_url_pressed(wxCommandEvent &evt) {
  wxLaunchDefaultBrowser(evt.GetString());
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
  EVT_COMMAND(ID_RE_CHANGELOG, wxEVT_COMMAND_TEXT_URL, update_check_dlg_c::on_url_pressed)
END_EVENT_TABLE();
#endif  // defined(HAVE_CURL_EASY_H)
