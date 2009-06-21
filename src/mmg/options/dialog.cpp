/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <algorithm>
#include <string>
#include <vector>

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/common.h"
#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/dialog.h"

options_dialog::options_dialog(wxWindow *parent,
                               mmg_options_t &options)
  : wxDialog(parent, -1, Z("Options"), wxDefaultPosition, wxDefaultSize)
  , m_options(options)
{

  nb_tabs      = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, wxNB_TOP);
  tab_mmg      = new optdlg_mmg_tab(nb_tabs, options);
  tab_mkvmerge = new optdlg_mkvmerge_tab(nb_tabs, options);

  nb_tabs->AddPage(tab_mmg, Z("GUI"));
  nb_tabs->AddPage(tab_mkvmerge, Z("mkvmerge"));

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);

  siz_all->Add(nb_tabs, 1, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxGROW, 5);
  siz_all->AddSpacer(5);

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_line->AddSpacer(5);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);

  siz_all->SetMinSize(wxSize(600, -1));
  siz_all->SetSizeHints(this);
}

void
options_dialog::on_ok(wxCommandEvent &evt) {
  m_options.mkvmerge                      = tab_mkvmerge->tc_mkvmerge->GetValue();
  m_options.priority                      = tab_mkvmerge->get_selected_priority();
  m_options.output_directory              = tab_mmg->tc_output_directory->GetValue();
  m_options.autoset_output_filename       = tab_mmg->cb_autoset_output_filename->IsChecked();
  m_options.ask_before_overwriting        = tab_mmg->cb_ask_before_overwriting->IsChecked();
  m_options.on_top                        = tab_mmg->cb_on_top->IsChecked();
  m_options.filenew_after_add_to_jobqueue = tab_mmg->cb_filenew_after_add_to_jobqueue->IsChecked();
  m_options.filenew_after_successful_mux  = tab_mmg->cb_filenew_after_successful_mux->IsChecked();
  m_options.warn_usage                    = tab_mmg->cb_warn_usage->IsChecked();
  m_options.gui_debugging                 = tab_mmg->cb_gui_debugging->IsChecked();
  m_options.set_delay_from_filename       = tab_mmg->cb_set_delay_from_filename->IsChecked();
  m_options.output_directory_mode         = tab_mmg->rb_odm_input_file->GetValue() ? ODM_FROM_FIRST_INPUT_FILE
                                          : tab_mmg->rb_odm_previous->GetValue()   ? ODM_PREVIOUS
                                          :                                          ODM_FIXED;

#if defined(HAVE_LIBINTL_H)
  std::string new_ui_locale = tab_mmg->get_selected_ui_language();

  if (downcase(new_ui_locale) != downcase(app->m_ui_locale))
    wxMessageBox(Z("Changing the interface language requires a restart to take effect."), Z("Restart required"), wxOK | wxCENTER | wxICON_INFORMATION);

  app->m_ui_locale  = new_ui_locale;

  wxConfigBase *cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("ui_locale"), wxString(new_ui_locale.c_str(), wxConvUTF8));
#endif  // HAVE_LIBINTL_H

  EndModal(wxID_OK);
}

IMPLEMENT_CLASS(options_dialog, wxDialog);
BEGIN_EVENT_TABLE(options_dialog, wxDialog)
  EVT_BUTTON(wxID_OK, options_dialog::on_ok)
END_EVENT_TABLE();
