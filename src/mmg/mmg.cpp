/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   app class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wx.h>
#include <wx/config.h>

#include "common/chapters/chapters.h"
#include "common/command_line.h"
#include "common/common.h"
#include "common/extern_data.h"
#include "common/extern_data.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/translation.h"
#include "common/wx.h"
#include "common/xml/element_mapping.h"
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
    return wxString::Format(wxU(format.c_str()), id - TRACK_ID_TAGS_BASE, num_entries, file_name.c_str());
  }

  std::string format;
  fix_format(Y("%s%s (ID %lld, type: %s) from %s"), format);
  wxString type_str = type == 'a' ? Z("audio")
                    : type == 'v' ? Z("video")
                    : type == 's' ? Z("subtitles")
                    :               Z("unknown");

  return wxString::Format(wxU(format.c_str()), appending ? wxT("++> ") : wxEmptyString, ctype.c_str(), id, type_str.c_str(), file_name.c_str());
}

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
  atexit(mtx_common_cleanup);

  wxConfigBase *cfg;
  uint32_t i;
  wxString k, v;
  int index;

  cfg = new wxConfig(wxT("mkvmergeGUI"));
  wxConfigBase::Set(cfg);

  init_stdio();
  init_ui_locale();
  mm_file_io_c::setup();
  g_cc_local_utf8 = charset_converter_c::init("");
  init_cc_stdio();
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
  int i;
  for (i = 1; app->argc > i; ++i)
    args.push_back(wxMB(wxString(app->argv[i])));

  handle_common_cli_args(args, "");

  if (args.empty())
    return;

  std::vector<wxString> wargs;
  for (i = 0; args.size() > i; ++i)
    wargs.push_back(wxU(args[i].c_str()));

  if (wargs[0] == wxT("--edit-headers")) {
    if (wargs.size() == 1)
      wxMessageBox(Z("Missing file name after for the option '--edit-headers'."), Z("Missing file name"), wxOK | wxCENTER | wxICON_ERROR);
    else {
      header_editor_frame_c *window  = new header_editor_frame_c(mdlg);
      window->Show();
      window->open_file(wxFileName(wargs[1]));
    }

    return;
  }

  wxString file = wargs[0];
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
