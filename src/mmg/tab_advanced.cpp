/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_advanced.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "advanced" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"
#include "wx/config.h"

#include "mmg.h"
#include "common.h"

tab_advanced::tab_advanced(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  uint32_t i;

  new wxStaticBox(this, -1, _("Other options"),
                  wxPoint(10, 5), wxSize(475, 50));
  new wxStaticText(this, -1, _("Command line charset:"), wxPoint(15, 25));
  cob_cl_charset =
    new wxComboBox(this, ID_CB_CLCHARSET, "", wxPoint(155, 25),
                   wxSize(130, -1), 0, NULL, wxCB_DROPDOWN |
                   wxCB_READONLY);
  cob_cl_charset->Append("");
  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_cl_charset->Append(sorted_charsets[i]);
  cob_cl_charset->SetToolTip(_T("Sets the charset that is used to convert "
                                "some of the strings entered here into UTF-8. "
                                "The default is the charset given by the "
                                "system's current locale. The options that "
                                "this setting affects are: segment title, "
                                "track name and attachment description."));

  new wxStaticBox(this, -1, _("Advanced options (DO NOT CHANGE!)"),
                  wxPoint(10, 400), wxSize(475, 62));
  cb_no_cues =
    new wxCheckBox(this, ID_CB_NOCUES, _("No cues"), wxPoint(15, 415),
                   wxDefaultSize, 0);
  cb_no_cues->SetToolTip(_T("Do not write the cues (the index). DO NOT "
                            "ENABLE this option unless you REALLY know "
                            "what you're doing!"));
  cb_no_clusters =
    new wxCheckBox(this, ID_CB_NOCLUSTERSINMETASEEK,
                   _("No clusters in meta seek"), wxPoint(145, 415),
                   wxDefaultSize, 0);
  cb_no_clusters->SetToolTip(_T("Do not put all the clusters into the cues "
                                "(the index). DO NOT ENABLE this option "
                                "unless you REALLY know what you're doing!"));
  cb_disable_lacing =
    new wxCheckBox(this, ID_CB_DISABLELACING, _("Disable lacing"),
                   wxPoint(325, 415), wxDefaultSize, 0);
  cb_disable_lacing->SetToolTip(_T("Disable lacing for audio tracks. DO NOT "
                                   "ENSABLE this option unless you REALLY "
                                   "know what you're doing!"));
  cb_enable_durations =
    new wxCheckBox(this, ID_CB_ENABLEDURATIONS, _("Enable durations"),
                   wxPoint(15, 436), wxDefaultSize, 0);
  cb_enable_durations->SetToolTip(_T("Enable durations for all blocks and not "
                                     "only for blocks that definitely need "
                                     "them (subtitles). DO NOT "
                                     "ENABLE this option unless you REALLY "
                                     "know what you're doing!"));
  cb_enable_timeslices =
    new wxCheckBox(this, ID_CB_ENABLETIMESLICES, _("Enable timeslices"),
                   wxPoint(145, 436), wxDefaultSize, 0);
  cb_enable_durations->SetToolTip(_T("Enable timeslices for laced blocks. "
                                     "DO NOT "
                                     "ENSABLE this option unless you REALLY "
                                     "know what you're doing!"));
}

void tab_advanced::load(wxConfigBase *cfg) {
  wxString s;
  bool b;

  cfg->SetPath("/advanced");

  cfg->Read("command_line_charset", &s);
  cob_cl_charset->SetValue(s);

  b = false;
  cfg->Read("no_cues", &b);
  cb_no_cues->SetValue(b);
  b = false;
  cfg->Read("no_clusters", &b);
  cb_no_clusters->SetValue(b);
  b = false;
  cfg->Read("disable_lacing", &b);
  cb_disable_lacing->SetValue(b);
  b = false;
  cfg->Read("enable_durations", &b);
  cb_enable_durations->SetValue(b);
  b = false;
  cfg->Read("enable_timeslices", &b);
  cb_enable_timeslices->SetValue(b);
}

void tab_advanced::save(wxConfigBase *cfg) {
  cfg->SetPath("/advanced");

  cfg->Write("command_line_charset", cob_cl_charset->GetValue());

  cfg->Write("no_cues", cb_no_cues->IsChecked());
  cfg->Write("no_clusters", cb_no_clusters->IsChecked());
  cfg->Write("disable_lacing", cb_disable_lacing->IsChecked());
  cfg->Write("enable_durations", cb_enable_durations->IsChecked());
  cfg->Write("enable_timeslices", cb_enable_timeslices->IsChecked());
}

bool tab_advanced::validate_settings() {
  return true;
}

IMPLEMENT_CLASS(tab_advanced, wxPanel);
BEGIN_EVENT_TABLE(tab_advanced, wxPanel)
END_EVENT_TABLE();
