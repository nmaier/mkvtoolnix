/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   app class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/file.h>
#include <wx/fileconf.h>
#include <wx/regex.h>
#ifdef SYS_WINDOWS
# include <wx/msw/registry.h>
#endif

#ifdef __WXMAC__
# include <ApplicationServices/ApplicationServices.h>
#endif

#include "common/chapters/chapters.h"
#include "common/command_line.h"
#include "common/common_pch.h"
#include "common/extern_data.h"
#include "common/extern_data.h"
#include "common/fs_sys_helpers.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/header_editor/frame.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/tabs/input.h"

mmg_app *app;

wxString
mmg_track_t::create_label() {
  wxString file_name = files[source]->file_name;
  file_name          = wxString::Format(wxT("%s (%s)"), file_name.AfterLast(wxT(PSEP)).c_str(), file_name.BeforeLast(wxT(PSEP)).c_str());

  if ('c' == type)
    return wxString::Format(Z("Chapters (%d entries) from %s"), num_entries, file_name.c_str());

  if (('t' == type) && (TRACK_ID_GLOBAL_TAGS == id))
    return wxString::Format(Z("Global tags (%d entries) from %s"), num_entries, file_name.c_str());

  if ('t' == type) {
    std::string format;
    fix_format(Y("Tags for track ID %lld (%d entries) from %s"), format);
    return wxString::Format(wxU(format), id - TRACK_ID_TAGS_BASE, num_entries, file_name.c_str());
  }

  std::string format;
  fix_format(Y("%s%s (ID %lld, type: %s) from %s"), format);
  wxString type_str = type == 'a' ? Z("audio")
                    : type == 'v' ? Z("video")
                    : type == 's' ? Z("subtitles")
                    :               Z("unknown");

  return wxString::Format(wxU(format), appending ? wxT("++> ") : wxEmptyString, ctype.c_str(), id, type_str.c_str(), file_name.c_str());
}

bool
mmg_track_t::is_webm_compatible() {
  static wxRegEx re_valid_webm_codecs(wxT("VP8|Vorbis|Opus"), wxRE_ICASE);

  return (is_audio() || is_video()) && re_valid_webm_codecs.Matches(ctype);
}

mmg_app::mmg_app() {
#if defined(SYS_WINDOWS)
  wxString dummy;
  wxRegKey key(wxU("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\mmg.exe"));
  m_is_installed = key.Exists() && key.QueryValue(wxU(""), dummy) && !dummy.IsEmpty();
#endif
}

void
mmg_app::init_ui_locale() {
  std::string locale;

#if defined(HAVE_LIBINTL_H)
  static bool s_first_init = true;
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

  if (s_first_init) {
    auto installation_path = mtx::get_installation_path();
    if (!installation_path.empty())
      wxLocale::AddCatalogLookupPathPrefix(wxU((installation_path / "locale").string()));

    auto lang_info = !m_ui_locale.empty() ? wxLocale::FindLanguageInfo(wxU(m_ui_locale)) : nullptr;
    auto language  = lang_info            ? lang_info->Language                          : wxLANGUAGE_ENGLISH;

    if (lang_info && m_locale.Init(language)) {
      m_locale.AddCatalog(wxU("wxstd"));
#ifdef SYS_WINDOWS
      m_locale.AddCatalog(wxU("wxmsw"));
#endif // SYS_WINDOWS
    }

    delete wxLog::SetActiveTarget(nullptr);
    s_first_init = false;
  }

#endif  // HAVE_LIBINTL_H

  init_locales(locale);
}

wxString
mmg_app::get_config_file_name()
  const {
#ifdef SYS_WINDOWS
  return wxU((mtx::get_installation_path() / "mkvtoolnix.ini").string());
#else
  return wxU((mtx::get_application_data_folder() / "config").string());
#endif
}

wxString
mmg_app::get_jobs_folder()
  const {
#if defined(SYS_WINDOWS)
  return m_is_installed ? wxU(mtx::get_application_data_folder().string()) : wxU((mtx::get_installation_path() / "jobs").string());
#else
  return wxU((mtx::get_application_data_folder() / "jobs").string());
#endif
}

void
mmg_app::prepare_mmg_data_folder() {
  // The 'jobs' folder is part of the application data
  // folder. Therefore both directories will be creatd.
  // 'prepare_path' treats the last part of the path as a file name;
  // therefore append a dummy file name so that all directory parts
  // will be created.
  mm_file_io_c::prepare_path(wxMB(get_jobs_folder() + wxT("/dummy")));

#if !defined(SYS_WINDOWS)
  // Migrate the config file from its old location ~/.mkvmergeGUI to
  // the new location ~/.mkvtoolnix/config
  wxString old_config_name;
  wxGetEnv(wxT("HOME"), &old_config_name);
  old_config_name += wxT("/.mkvmergeGUI");

  if (wxFileExists(old_config_name))
    wxRenameFile(old_config_name, get_config_file_name());
#endif
}

void
mmg_app::init_config_base()
  const {
  auto cfg = static_cast<wxConfigBase *>(nullptr);

#if defined(SYS_WINDOWS)
  if (m_is_installed)
    cfg = new wxConfig{wxT("mkvmergeGUI")};
#endif

  if (!cfg)
    cfg = new wxFileConfig{wxT("mkvmergeGUI"), wxEmptyString, get_config_file_name()};

  cfg->SetExpandEnvVars(false);
  wxConfigBase::Set(cfg);
}

bool
mmg_app::OnInit() {
#ifdef __WXMAC__
  ProcessSerialNumber PSN;
  GetCurrentProcess(&PSN);
  TransformProcessType(&PSN, kProcessTransformToForegroundApplication);
#endif

  wxImage::AddHandler(new wxPNGHandler);

  mtx_common_init("mmg", to_utf8(this->argv[0]).c_str());

  debugging_c::send_to_logger(true);

  uint32_t i;
  wxString k, v;
  int index;

  prepare_mmg_data_folder();
  init_config_base();
  init_ui_locale();

  auto cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Read(wxT("last_directory"), &last_open_dir, wxEmptyString);
  for (i = 0; i < 4; i++) {
    k.Printf(wxT("last_settings %u"), i);
    if (cfg->Read(k, &v) && wxFile::Exists(v))
      last_settings.push_back(v);
    k.Printf(wxT("last_chapters %u"), i);
    if (cfg->Read(k, &v) && wxFile::Exists(v))
      last_chapters.push_back(v);
  }
  cfg->SetPath(wxT("/chapter_editor"));
  cfg->Read(wxT("default_language"), &k, wxT("und"));
  g_default_chapter_language = wxMB(k);
  index = map_to_iso639_2_code(g_default_chapter_language.c_str());
  if (-1 == index)
    g_default_chapter_language = "und";
  else
    g_default_chapter_language = iso639_languages[index].iso639_2_code;
  if (cfg->Read(wxT("default_country"), &k) && (0 < k.length()))
    g_default_chapter_country = wxMB(k);
  if (!is_valid_cctld(g_default_chapter_country.c_str()))
    g_default_chapter_country = "";

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

  std::vector<std::string> args;
  size_t i;
  for (i = 1; static_cast<size_t>(app->argc) > i; ++i)
    args.push_back(wxMB(wxString(app->argv[i])));

  handle_common_cli_args(args, "");

  if (args.empty())
    return;

  std::vector<wxString> wargs;
  for (i = 0; args.size() > i; ++i)
    wargs.push_back(wxU(args[i]));

  if (wargs[0] == wxT("--edit-headers")) {
    if (wargs.size() == 1)
      wxMessageBox(Z("Missing file name after for the option '--edit-headers'."), Z("Missing file name"), wxOK | wxCENTER | wxICON_ERROR);
    else
      mdlg->create_header_editor_window(wargs[1]);

    return;
  }

  for (auto &file : wargs)
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
  cfg->Write(wxT("default_language"), wxString(g_default_chapter_language.c_str(), wxConvUTF8));
  cfg->Write(wxT("default_country"), wxString(g_default_chapter_country.c_str(), wxConvUTF8));
  cfg->Flush();

  delete cfg;

  return 0;
}

IMPLEMENT_APP(mmg_app)
