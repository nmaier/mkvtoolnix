/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "chapter editor" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <errno.h>

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/statline.h>

#include <ebml/EbmlStream.h>

#include "common/chapters/chapters.h"
#include "common/common_pch.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/mm_write_cache_io.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/unique_numbers.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/tabs/chapters.h"
#include "mmg/wx_kax_analyzer.h"

using namespace libebml;
using namespace libmatroska;

class chapters_drop_target_c: public wxFileDropTarget {
private:
  tab_chapters *owner;
public:
  chapters_drop_target_c(tab_chapters *n_owner):
    owner(n_owner) {};
  virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &dropped_files) {
    owner->load(dropped_files[0]);

    return true;
  }
};

class chapter_node_data_c: public wxTreeItemData {
public:
  bool is_atom;
  KaxChapterAtom *chapter;
  KaxEditionEntry *eentry;

  chapter_node_data_c(KaxChapterAtom *nchapter):
    is_atom(true),
    chapter(nchapter) {
  };

  chapter_node_data_c(KaxEditionEntry *neentry):
    is_atom(false),
    eentry(neentry) {
  };
};

#define ID_CVD_CB_LANGUAGE 12000
#define ID_CVD_CB_COUNTRY 12001

class chapter_values_dlg: public wxDialog {
  DECLARE_CLASS(chapter_values_dlg);
  DECLARE_EVENT_TABLE();
public:
  wxMTX_COMBOBOX_TYPE *cob_language, *cob_country;
  wxCheckBox *cb_language, *cb_country;

public:
  chapter_values_dlg(wxWindow *parent);
  void on_language_clicked(wxCommandEvent &evt);
  void on_country_clicked(wxCommandEvent &evt);
};

chapter_values_dlg::chapter_values_dlg(wxWindow *parent)
  : wxDialog(parent, 0, wxEmptyString, wxDefaultPosition, wxSize(350, 200))
{
  wxBoxSizer *siz_all, *siz_buttons;
  wxFlexGridSizer *siz_input;
  uint32_t i;

  siz_all = new wxBoxSizer(wxVERTICAL);
  SetTitle(Z("Select values to be applied"));
  siz_all->Add(new wxStaticText(this, wxID_STATIC,
                                Z("Please enter the values for the language and the\n"
                                  "country that you want to apply to all the chapters\n"
                                  "below and including the currently selected entry.")),
               0, wxLEFT | wxRIGHT | wxTOP, 10);

  siz_input = new wxFlexGridSizer(2);
  siz_input->AddGrowableCol(1);

  cb_language = new wxCheckBox(this, ID_CVD_CB_LANGUAGE, Z("Set language to:"));
  cb_language->SetValue(false);
  siz_input->Add(cb_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_language = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language->Enable(false);
  siz_input->Add(cob_language, 0, wxGROW, 0);

  cb_country = new wxCheckBox(this, ID_CVD_CB_COUNTRY, Z("Set country to:"));
  cb_country->SetValue(false);
  siz_input->Add(cb_country, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_country = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_country->Enable(false);
  siz_input->Add(cob_country, 0, wxGROW, 0);

  siz_all->AddSpacer(10);
  siz_all->Add(siz_input, 0, wxGROW | wxLEFT | wxRIGHT, 10);
  siz_all->AddSpacer(10);

  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language->Append(sorted_iso_codes[i]);

  cob_country->Append(wxEmptyString);
  for (i = 0; cctlds[i] != NULL; i++)
    cob_country->Append(wxU(cctlds[i]));

  siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons->AddStretchSpacer();
  siz_buttons->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_buttons->AddSpacer(10);

  siz_all->Add(siz_buttons, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizer(siz_all);

  siz_all->SetSizeHints(this);
}

void
chapter_values_dlg::on_language_clicked(wxCommandEvent &evt) {
  cob_language->Enable(cb_language->IsChecked());
}

void
chapter_values_dlg::on_country_clicked(wxCommandEvent &evt) {
  cob_country->Enable(cb_country->IsChecked());
}

void
expand_subtree(wxTreeCtrl &tree,
               wxTreeItemId &root,
               bool expand = true) {
  wxTreeItemId child;
  wxTreeItemIdValue cookie;

  if (!expand)
    tree.Collapse(root);
  child = tree.GetFirstChild(root, cookie);
  while (child > 0) {
    expand_subtree(tree, child, expand);
    child = tree.GetNextChild(root, cookie);
  }
  if (expand)
    tree.Expand(root);
}

#define Y1 30
#define X1 100

tab_chapters::tab_chapters(wxWindow *parent,
                           wxMenu *nm_chapters):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL) {
  wxBoxSizer *siz_all, *siz_line, *siz_column;
  wxStaticBoxSizer *siz_sb;
  wxFlexGridSizer *siz_fg;
  uint32_t i;

  m_chapters = nm_chapters;

  siz_all = new wxBoxSizer(wxVERTICAL);

  st_chapters = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_all->AddSpacer(5);
  siz_all->Add(st_chapters, 0, wxLEFT, 10);
  siz_all->AddSpacer(5);

#ifdef SYS_WINDOWS
  tc_chapters = new wxTreeCtrl(this, ID_TRC_CHAPTERS);
#else
  tc_chapters = new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER | wxTR_HAS_BUTTONS);
#endif

  b_add_chapter      = new wxButton(this, ID_B_ADDCHAPTER);
  b_add_subchapter   = new wxButton(this, ID_B_ADDSUBCHAPTER);
  b_remove_chapter   = new wxButton(this, ID_B_REMOVECHAPTER);
  b_set_values       = new wxButton(this, ID_B_SETVALUES);
  b_adjust_timecodes = new wxButton(this, ID_B_ADJUSTTIMECODES);

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(0);
  siz_fg->AddGrowableRow(0);

  siz_fg->Add(tc_chapters, 1, wxGROW | wxRIGHT | wxBOTTOM, 5);

  siz_column = new wxBoxSizer(wxVERTICAL);
  siz_column->Add(b_add_chapter, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_add_subchapter, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_remove_chapter, 0, wxGROW | wxBOTTOM, 15);
  siz_column->Add(b_set_values, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_adjust_timecodes, 0, wxGROW | wxBOTTOM, 3);
  siz_fg->Add(siz_column, 0, wxLEFT | wxBOTTOM, 5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  st_start = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_line->Add(st_start, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  tc_start_time = new wxTextCtrl(this, ID_TC_CHAPTERSTART, wxEmptyString);
  siz_line->Add(tc_start_time, 1, wxGROW | wxRIGHT, 10);

  st_end = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_line->Add(st_end, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  tc_end_time = new wxTextCtrl(this, ID_TC_CHAPTEREND, wxEmptyString);
  siz_line->Add(tc_end_time, 1, wxGROW, 0);

  siz_fg->Add(siz_line, 0, wxGROW | wxRIGHT, 5);
  siz_fg->Add(1, 0, 0, 0, 0);

  siz_fg->AddSpacer(5);
  siz_fg->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  st_uid = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_line->Add(st_uid, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  tc_uid = new wxTextCtrl(this, ID_TC_UID, wxEmptyString);
  siz_line->Add(tc_uid, 1, wxGROW | wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
  cb_flag_hidden = new wxCheckBox(this, ID_CB_CHAPTERHIDDEN, wxEmptyString);
  siz_line->Add(cb_flag_hidden, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);
  cb_flag_enabled = new wxCheckBox(this, ID_CB_CHAPTERENABLED, wxEmptyString);
  siz_line->Add(cb_flag_enabled, 0, wxALIGN_CENTER_VERTICAL, 0);

  siz_fg->Add(siz_line, 0, wxGROW | wxRIGHT, 5);
  siz_fg->Add(1, 0, 0, 0, 0);

  siz_all->Add(siz_fg, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  sb_names = new wxStaticBox(this, wxID_STATIC, wxEmptyString);
  siz_sb = new wxStaticBoxSizer(sb_names, wxVERTICAL);

  lb_chapter_names = new wxListBox(this, ID_LB_CHAPTERNAMES);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(lb_chapter_names, 1, wxGROW | wxRIGHT, 10);

  b_add_chapter_name    = new wxButton(this, ID_B_ADD_CHAPTERNAME);
  b_remove_chapter_name = new wxButton(this, ID_B_REMOVE_CHAPTERNAME);
  siz_column            = new wxBoxSizer(wxVERTICAL);
  siz_column->Add(b_add_chapter_name, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_remove_chapter_name, 0, wxGROW, 0);
  siz_line->Add(siz_column, 0, 0, 0);
  siz_sb->Add(siz_line, 1, wxGROW | wxALL, 10);

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(1);
  st_name = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_fg->Add(st_name, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 10);
  tc_chapter_name = new wxTextCtrl(this, ID_TC_CHAPTERNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
  siz_fg->Add(tc_chapter_name, 1, wxGROW | wxBOTTOM, 10);

  st_language = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_fg->Add(st_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

  cob_language_code = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERSELECTLANGUAGECODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language_code->SetMinSize(wxSize(0, 0));
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language_code->Append(sorted_iso_codes[i]);
  cob_language_code->SetValue(wxEmptyString);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cob_language_code, 3, wxGROW, 0);
  siz_line->Add(10, 0, 0, 0, 0);

  st_country = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_line->Add(st_country, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  cob_country_code = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERSELECTCOUNTRYCODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_country_code->Append(wxEmptyString);
  for (i = 0; cctlds[i] != NULL; i++)
    cob_country_code->Append(wxU(cctlds[i]));
  cob_country_code->SetValue(wxEmptyString);
  siz_line->Add(cob_country_code, 1, wxGROW, 0);
  siz_fg->Add(siz_line, 0, wxGROW, 0);

  siz_sb->Add(siz_fg,  0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->Add(siz_sb, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  enable_inputs(false);

  m_chapters->Enable(ID_M_CHAPTERS_SAVE,      false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVEAS,    false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVETOKAX, false);
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY,    false);
  enable_buttons(false);

  file_name               = wxEmptyString;
  chapters                = NULL;
  analyzer                = NULL;
  source_is_kax_file      = false;
  source_is_simple_format = false;
  no_update               = false;

  SetDropTarget(new chapters_drop_target_c(this));
  SetSizer(siz_all);
}

tab_chapters::~tab_chapters() {
  delete chapters;
  delete analyzer;
}

void
tab_chapters::translate_ui() {
  st_chapters->SetLabel(Z("Chapters:"));
  b_add_chapter->SetLabel(Z("Add chapter"));
  b_add_subchapter->SetLabel(Z("Add subchapter"));
  b_remove_chapter->SetLabel(Z("Remove chapter"));
  b_set_values->SetLabel(Z("Set values"));
  b_set_values->SetToolTip(TIP("Here you can set the values for the language and the country that you want to apply to all the "
                               "chapters below and including the currently selected entry."));
  b_adjust_timecodes->SetLabel(Z("Adjust timecodes"));
  b_adjust_timecodes->SetToolTip(TIP("Here you can adjust all the timecodes of the selected chapter and of all its sub-chapters by a specific amount either increasing or decreasing them."));
  st_start->SetLabel(Z("Start:"));
  st_end->SetLabel(Z("End:"));
  st_uid->SetLabel(Z("UID:"));
  tc_uid->SetToolTip(TIP("Each chapter and each edition has a unique identifier. This identifier is normally assigned "
                          "automatically by the programs, but it can be changed manually if it is really needed."));
  cb_flag_hidden->SetLabel(Z("hidden"));
  cb_flag_hidden->SetToolTip(TIP("If a chapter is marked 'hidden' then the player should not show this chapter entry "
                                 "to the user. Such entries could still be used by the menu system."));
  cb_flag_enabled->SetLabel(Z("enabled"));
  cb_flag_enabled->SetToolTip(TIP("If a chapter is not marked 'enabled' then the player should skip the part of the file that this chapter occupies."));
  sb_names->SetLabel(Z("Chapter names and languages"));
  b_add_chapter_name->SetLabel(Z("Add name"));
  b_remove_chapter_name->SetLabel(Z("Remove name"));
  st_name->SetLabel(Z("Name:"));
  st_language->SetLabel(Z("Language:"));
  st_country->SetLabel(Z("Country:"));
}

void
tab_chapters::enable_inputs(bool enable) {
  tc_chapter_name->Enable(enable);
  tc_start_time->Enable(enable);
  tc_end_time->Enable(enable);
  tc_uid->Enable(enable);
  cob_language_code->Enable(enable);
  cob_country_code->Enable(enable);
  lb_chapter_names->Enable(enable);
  b_add_chapter_name->Enable(enable);
  b_remove_chapter_name->Enable(enable);
  cb_flag_hidden->Enable(enable);
  cb_flag_enabled->Enable(enable);
  st_start->Enable(enable);
  st_end->Enable(enable);
  st_uid->Enable(enable);
  st_name->Enable(enable);
  st_language->Enable(enable);
  st_country->Enable(enable);
  sb_names->Enable(enable);
  inputs_enabled = enable;
}

void
tab_chapters::enable_buttons(bool enable) {
  b_add_chapter->Enable(enable);
  b_add_subchapter->Enable(enable);
  b_remove_chapter->Enable(enable);
  b_set_values->Enable(enable);
  b_adjust_timecodes->Enable(enable);
}

void
tab_chapters::on_new_chapters(wxCommandEvent &evt) {
  file_name = wxEmptyString;
  tc_chapters->DeleteAllItems();
  tid_root = tc_chapters->AddRoot(Z("(new chapter file)"));
  if (chapters != NULL)
    delete chapters;
  chapters = new KaxChapters;

  m_chapters->Enable(ID_M_CHAPTERS_SAVE, true);
  m_chapters->Enable(ID_M_CHAPTERS_SAVEAS, true);
  m_chapters->Enable(ID_M_CHAPTERS_SAVETOKAX, true);
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, true);
  enable_buttons(true);

  enable_inputs(false);
  source_is_kax_file = false;
  if (analyzer != NULL) {
    delete analyzer;
    analyzer = NULL;
  }

  clear_list_of_unique_uint32(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_uint32(UNIQUE_EDITION_IDS);

  mdlg->set_status_bar(Z("New chapters created."));
}

wxString
tab_chapters::create_chapter_label(KaxChapterAtom &chapter) {
  wxString label, s;
  KaxChapterDisplay *display;
  KaxChapterString *cstring;
  KaxChapterTimeStart *tstart;
  KaxChapterTimeEnd *tend;
  KaxChapterLanguage *language;
  int64_t timestamp;

  label = Z("(unnamed chapter)");
  language = NULL;
  display = FindChild<KaxChapterDisplay>(chapter);
  if (display != NULL) {
    cstring = FindChild<KaxChapterString>(*display);
    if (cstring != NULL)
      label =
        UTFstring_to_wxString(*static_cast<EbmlUnicodeString *>(cstring));
    language = FindChild<KaxChapterLanguage>(*display);
  }
  label += wxT(" [");
  tstart = FindChild<KaxChapterTimeStart>(chapter);
  if (tstart != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tstart));
    s.Printf(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
    label += s;

    tend = FindChild<KaxChapterTimeEnd>(chapter);
    if (tend != NULL) {
      timestamp = uint64(*static_cast<EbmlUInteger *>(tend));
      s.Printf(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
      label += wxT(" - ") + s;
    }

    label += wxT("; ");
  }
  if (language != NULL)
    label += wxU(language);
  else
    label += wxT("eng");

  label += wxT("]");

  return label;
}

void
tab_chapters::add_recursively(wxTreeItemId &parent,
                              EbmlMaster &master) {
  uint32_t i;
  wxString s;
  EbmlElement *e;
  KaxChapterAtom *chapter;
  KaxEditionEntry *eentry;
  wxTreeItemId this_item;

  for (i = 0; i < master.ListSize(); i++) {
    e = master[i];
    if (EbmlId(*e) == KaxEditionEntry::ClassInfos.GlobalId) {
      s.Printf(Z("Edition %d"), (int)tc_chapters->GetChildrenCount(tid_root, false) + 1);
      eentry = static_cast<KaxEditionEntry *>(e);
      this_item =
        tc_chapters->AppendItem(parent, s, -1, -1,
                                new chapter_node_data_c(eentry));
      add_recursively(this_item, *static_cast<EbmlMaster *>(e));

    } else if (EbmlId(*e) == KaxChapterAtom::ClassInfos.GlobalId) {
      chapter = static_cast<KaxChapterAtom *>(e);
      s = create_chapter_label(*chapter);
      this_item = tc_chapters->AppendItem(parent, s, -1, -1,
                                          new chapter_node_data_c(chapter));
      add_recursively(this_item, *static_cast<EbmlMaster *>(e));
    }
  }
}

void
tab_chapters::fix_missing_languages(EbmlMaster &master) {
  uint32_t i;
  EbmlElement *e;
  KaxChapterDisplay *d;
  KaxChapterLanguage *l;
  KaxChapterUID *u;
  EbmlMaster *m;

  for (i = 0; i < master.ListSize(); i++) {
    e = master[i];

    if (dynamic_cast<EbmlMaster *>(e) != NULL)
      fix_missing_languages(*dynamic_cast<EbmlMaster *>(e));

    if (EbmlId(*e) == KaxChapterAtom::ClassInfos.GlobalId) {
      m = static_cast<EbmlMaster *>(e);
      d = FindChild<KaxChapterDisplay>(*m);
      if (d == NULL)
        d = &GetChild<KaxChapterDisplay>(*m);
      l = FindChild<KaxChapterLanguage>(*d);
      if (l == NULL) {
        l = &GetChild<KaxChapterLanguage>(*d);
        *static_cast<EbmlString *>(l) = "eng";
      }
      u = FindChild<KaxChapterUID>(*m);
      if (u != NULL)
        add_unique_uint32(uint32(*static_cast<EbmlUInteger *>(u)),
                          UNIQUE_CHAPTER_IDS);
    }
  }
}

void
tab_chapters::on_load_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, Z("Choose a chapter file"), last_open_dir, wxEmptyString,
                   wxString::Format(Z("Chapter files (*.xml;*.txt;*.mka;*.mkv;*.mk3d;*.cue)|*.xml;*.txt;*.mka;*.mkv;*.mk3d;*.cue|%s"), ALLFILES.c_str()), wxFD_OPEN);

  if (dlg.ShowModal() == wxID_OK)
    if (load(dlg.GetPath()))
      last_open_dir = dlg.GetDirectory();
}

bool
tab_chapters::load(wxString name) {
  KaxChapters *new_chapters;
  EbmlElement *e;
  wxString s;
  int pos;

  clear_list_of_unique_uint32(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_uint32(UNIQUE_EDITION_IDS);

  try {
    if (kax_analyzer_c::probe(wxMB(name))) {
      if (analyzer != NULL)
        delete analyzer;
      analyzer = new wx_kax_analyzer_c(this, wxMB(name));
      file_name = name;
      if (!analyzer->process()) {
        wxMessageBox(Z("This file could not be opened or parsed."), Z("File parsing failed"), wxOK | wxCENTER | wxICON_ERROR);
        delete analyzer;
        analyzer = NULL;

        return false;
      }
      pos = analyzer->find(KaxChapters::ClassInfos.GlobalId);
      if (pos == -1) {
        wxMessageBox(Z("This file does not contain any chapters."), Z("No chapters found"), wxOK | wxCENTER | wxICON_INFORMATION);
        delete analyzer;
        analyzer = NULL;
        return false;
      }

      e = analyzer->read_element(pos);
      if (e == NULL)
        throw mtx::exception();
      new_chapters = static_cast<KaxChapters *>(e);
      source_is_kax_file = true;
      source_is_simple_format = false;

      analyzer->close_file();

    } else {
      new_chapters = parse_chapters(wxMB(name), 0, -1, 0, "", "", true,
                                    &source_is_simple_format);
      source_is_kax_file = false;
    }
  } catch (mtx::exception &) {
    if (analyzer)
      delete analyzer;
    analyzer = NULL;
    s = Z("This file does not contain valid chapters.");
    break_line(s);
    while (s[s.Length() - 1] == wxT('\n'))
      s.Remove(s.Length() - 1);
    wxMessageBox(s, Z("Error parsing the file"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  if (chapters != NULL)
    delete chapters;
  tc_chapters->DeleteAllItems();
  chapters = new_chapters;
  m_chapters->Enable(ID_M_CHAPTERS_SAVE, true);
  m_chapters->Enable(ID_M_CHAPTERS_SAVEAS, true);
  m_chapters->Enable(ID_M_CHAPTERS_SAVETOKAX, true);
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, true);
  enable_buttons(true);

  file_name = name;
  clear_list_of_unique_uint32(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_uint32(UNIQUE_EDITION_IDS);
  fix_missing_languages(*chapters);
  tid_root = tc_chapters->AddRoot(file_name);
  add_recursively(tid_root, *chapters);
  expand_subtree(*tc_chapters, tid_root);

  enable_inputs(false);

  mdlg->set_last_chapters_in_menu(name);
  mdlg->set_status_bar(Z("Chapters loaded."));

  return true;
}

void
tab_chapters::on_save_chapters(wxCommandEvent &evt) {
  if (!verify())
    return;

  if (source_is_kax_file) {
    write_chapters_to_matroska_file();
    return;
  }

  if ((file_name.length() == 0) || source_is_simple_format)
    if (!select_file_name())
      return;
  save();
}

void
tab_chapters::on_save_chapters_to_kax_file(wxCommandEvent &evt) {
  if (!verify())
    return;

  wxFileDialog dlg(this, Z("Choose an output file"), last_open_dir, wxEmptyString,
                   wxString::Format(Z("Matroska files (*.mkv;*.mka;*.mk3d)|*.mkv;*.mka;*.mk3d|%s"), ALLFILES.c_str()), wxFD_SAVE);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (!kax_analyzer_c::probe(wxMB(dlg.GetPath()))) {
    wxMessageBox(Z("The file you tried to save to is NOT a Matroska file."), Z("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  last_open_dir = dlg.GetDirectory();
  file_name = dlg.GetPath();
  tc_chapters->SetItemText(tid_root, file_name);

  source_is_kax_file = true;
  source_is_simple_format = false;

  if (analyzer != NULL)
    delete analyzer;
  analyzer = new wx_kax_analyzer_c(this, wxMB(file_name));
  if (!analyzer->process()) {
    delete analyzer;
    analyzer = NULL;
    return;
  }

  write_chapters_to_matroska_file();
  mdlg->set_last_chapters_in_menu(file_name);

  analyzer->close_file();
}

void
tab_chapters::on_save_chapters_as(wxCommandEvent &evt) {
  if (verify() && select_file_name())
    save();
}

bool
tab_chapters::select_file_name() {
  wxFileDialog dlg(this, Z("Choose an output file"), last_open_dir, wxEmptyString,
                   wxString::Format(Z("Chapter files (*.xml)|*.xml|%s"), ALLFILES.c_str()), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() != wxID_OK)
    return false;

  if (kax_analyzer_c::probe(wxMB(dlg.GetPath()))) {
    wxMessageBox(Z("The file you tried to save to is a Matroska file. For this to work you have to use the 'Save to Matroska file' menu option."),
                 Z("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  last_open_dir = dlg.GetDirectory();

  wxFileName fn(dlg.GetPath());
  if (!fn.HasExt())
    fn.SetExt(wxU("xml"));
  file_name = fn.GetFullPath();

  tc_chapters->SetItemText(tid_root, file_name);

  return true;
}

void
tab_chapters::save() {
  mm_io_cptr out;

  try {
    out = mm_io_cptr(mm_write_cache_io_c::open(wxMB(file_name), 128 * 1024));
  } catch (...) {
    wxMessageBox(wxString::Format(Z("Could not open the destination file '%s' for writing. Error code: %d (%s)."), file_name.c_str(), errno, wxUCS(strerror(errno))), Z("Error opening file"), wxCENTER | wxOK | wxICON_ERROR);
    return;
  }

#if defined(SYS_WINDOWS)
  out->use_dos_style_newlines(true);
#endif
  out->write_bom("UTF-8");
  out->puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "\n"
            "<!-- <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> -->\n"
            "\n"
            "<Chapters>\n");
  write_chapters_xml(chapters, out.get_object());
  out->puts("</Chapters>\n");

  source_is_kax_file = false;
  source_is_simple_format = false;

  mdlg->set_last_chapters_in_menu(file_name);
  mdlg->set_status_bar(Z("Chapters written."));
}

void
tab_chapters::on_verify_chapters(wxCommandEvent &evt) {
  verify(true);
}

bool
tab_chapters::verify_atom_recursively(EbmlElement *e) {
  KaxChapterUID *uid;
  KaxChapterAtom *chapter;
  KaxChapterTimeStart *start;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterString *cs;
  wxString label;
  std::string lang;
  uint32_t i;

  if (dynamic_cast<KaxEditionUID *>(e) != NULL)
    return true;
  chapter = dynamic_cast<KaxChapterAtom *>(e);
  if (chapter == NULL)
    return false;

  if (FindChild<KaxChapterUID>(*chapter) == NULL) {
    uid = &GetChild<KaxChapterUID>(*chapter);
    *static_cast<EbmlUInteger *>(uid) =
      create_unique_uint32(UNIQUE_CHAPTER_IDS);
  }

  cs = NULL;
  display = FindChild<KaxChapterDisplay>(*chapter);
  if (display != NULL)
    cs = FindChild<KaxChapterString>(*display);

  if ((display == NULL) || (cs == NULL)) {
    wxMessageBox(Z("One of the chapters does not have a name."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }
  label =
    UTFstring_to_wxString(UTFstring(*static_cast<EbmlUnicodeString *>(cs)));

  start = FindChild<KaxChapterTimeStart>(*chapter);
  if (start == NULL) {
    wxMessageBox(wxString::Format(Z("The chapter '%s' is missing the start time."), label.c_str()), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  language = FindChild<KaxChapterLanguage>(*display);
  if (language == NULL) {
    wxMessageBox(wxString::Format(Z("The chapter '%s' is missing its language."), label.c_str()), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }
  lang = std::string(*language);
  if ((0 == lang.size()) || !is_valid_iso639_2_code(lang.c_str())) {
    wxMessageBox(wxString::Format(Z("The selected language '%s' for the chapter '%s' is not a valid language code. Please select one of the predefined ones."),
                                  wxUCS(lang), label.c_str()),
                 Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  for (i = 0; i < chapter->ListSize(); i++)
    if (dynamic_cast<KaxChapterAtom *>((*chapter)[i]) != NULL)
      if (!verify_atom_recursively((*chapter)[i]))
        return false;

  return true;
}

bool
tab_chapters::verify(bool called_interactively) {
  uint32_t eidx, cidx;

  if (NULL == chapters){
    wxMessageBox(Z("No chapter entries have been create yet."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  if (0 == chapters->ListSize())
    return true;

  wxTreeItemId id = tc_chapters->GetSelection();
  if (id.IsOk())
    copy_values(id);

  for (eidx = 0; eidx < chapters->ListSize(); eidx++) {
    KaxEditionEntry *eentry = static_cast<KaxEditionEntry *>((*chapters)[eidx]);
    bool contains_atom      = false;

    for (cidx = 0; cidx < eentry->ListSize(); cidx++)
      if (dynamic_cast<KaxChapterAtom *>((*eentry)[cidx]) != NULL) {
        contains_atom = true;
        break;
      }

    if (!contains_atom) {
      wxMessageBox(Z("Each edition must contain at least one chapter."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
      return false;
    }
  }

  fix_mandatory_chapter_elements(chapters);

  for (eidx = 0; eidx < chapters->ListSize(); eidx++) {
    KaxEditionEntry *eentry = static_cast<KaxEditionEntry *>((*chapters)[eidx]);
    for (cidx = 0; cidx < eentry->ListSize(); cidx++)
      if ((dynamic_cast<KaxChapterAtom *>((*eentry)[cidx]) != NULL) && !verify_atom_recursively((*eentry)[cidx]))
        return false;
  }

  if (!chapters->CheckMandatory())
    wxdie(Z("verify failed: chapters->CheckMandatory() is false. This should not have happened. Please file a bug report.\n"));
  chapters->UpdateSize();

  if (called_interactively)
    wxMessageBox(Z("All chapter entries are valid."), Z("Chapter verification succeeded"), wxCENTER | wxOK | wxICON_INFORMATION);

  return true;
}

void
tab_chapters::on_add_chapter(wxCommandEvent &evt) {
  wxTreeItemId id, pid;
  KaxEditionEntry *eentry;
  KaxChapterAtom *chapter;
  EbmlMaster *m;
  chapter_node_data_c *d, *pd;
  wxString s;
  std::vector<EbmlElement *> tmpvec;
  uint32_t start, i;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  copy_values(id);

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (id == tid_root) {
    KaxEditionUID *euid;

    eentry = new KaxEditionEntry;
    chapters->PushElement(*eentry);
    euid = new KaxEditionUID;
    *static_cast<EbmlUInteger *>(euid) =
      create_unique_uint32(UNIQUE_EDITION_IDS);
    eentry->PushElement(*euid);
    d = new chapter_node_data_c(eentry);
    s.Printf(Z("EditionEntry %u"), (unsigned int)chapters->ListSize());
    id = tc_chapters->AppendItem(tid_root, s, -1, -1, d);
  }

  chapter = new KaxChapterAtom;
  while (chapter->ListSize() > 0) {
    delete (*chapter)[0];
    chapter->Remove(0);
  }
  KaxChapterDisplay &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
  GetChildAs<KaxChapterString, EbmlUnicodeString>(display) = cstrutf8_to_UTFstring(Y("(unnamed)"));
  if (!g_default_chapter_language.empty())
    GetChildAs<KaxChapterLanguage, EbmlString>(display) = g_default_chapter_language.c_str();
  if (!g_default_chapter_country.empty())
    GetChildAs<KaxChapterCountry, EbmlString>(display) = g_default_chapter_country.c_str();
  GetChildAs<KaxChapterUID, EbmlUInteger>(*chapter) = create_unique_uint32(UNIQUE_CHAPTER_IDS);
  s = create_chapter_label(*chapter);

  if (d->is_atom) {
    pid = tc_chapters->GetItemParent(id);
    pd = (chapter_node_data_c *)tc_chapters->GetItemData(pid);
    if (pd->is_atom)
      m = pd->chapter;
    else
      m = pd->eentry;
    tc_chapters->InsertItem(pid, id, s, -1, -1,
                            new chapter_node_data_c(chapter));

    start = m->ListSize() + 1;
    for (i = 0; i < m->ListSize(); i++)
      if ((*m)[i] == d->chapter) {
        start = i;
        break;
      }
    if (start >= m->ListSize())
      wxdie(Z("start >= m->ListSize(). This should not have happened. Please file a bug report. Thanks."));
    while ((start + 1) < m->ListSize()) {
      tmpvec.push_back((*m)[start + 1]);
      m->Remove(start + 1);
    }
    m->PushElement(*chapter);
    for (i = 0; i < tmpvec.size(); i++)
      m->PushElement(*tmpvec[i]);
  } else {
    m = d->eentry;
    tc_chapters->AppendItem(id, s, -1, -1,
                            new chapter_node_data_c(chapter));
    m->PushElement(*chapter);
  }
  id = tc_chapters->GetSelection();
  expand_subtree(*tc_chapters, id, true);
}

void
tab_chapters::on_add_subchapter(wxCommandEvent &evt) {
  wxTreeItemId id;
  KaxEditionEntry *eentry;
  KaxChapterAtom *chapter;
  EbmlMaster *m;
  chapter_node_data_c *d;
  wxString s;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  copy_values(id);

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (id == tid_root) {
    eentry = new KaxEditionEntry;
    chapters->PushElement(*eentry);
    d = new chapter_node_data_c(eentry);
    s.Printf(Z("EditionEntry %u"), (unsigned int)chapters->ListSize());
    id = tc_chapters->AppendItem(tid_root, s, -1, -1, d);
  }
  if (d->is_atom)
    m = d->chapter;
  else
    m = d->eentry;
  chapter = new KaxChapterAtom;
  while (chapter->ListSize() > 0) {
    delete (*chapter)[0];
    chapter->Remove(0);
  }
  KaxChapterDisplay &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
  GetChildAs<KaxChapterString, EbmlUnicodeString>(display) = cstrutf8_to_UTFstring(Y("(unnamed)"));
  if (!g_default_chapter_language.empty())
    GetChildAs<KaxChapterLanguage, EbmlString>(display) = g_default_chapter_language.c_str();
  if (!g_default_chapter_country.empty())
    GetChildAs<KaxChapterCountry, EbmlString>(display)  = g_default_chapter_country.c_str();
  m->PushElement(*chapter);
  s = create_chapter_label(*chapter);
  tc_chapters->AppendItem(id, s, -1, -1,
                          new chapter_node_data_c(chapter));
  id = tc_chapters->GetSelection();
  expand_subtree(*tc_chapters, id, true);
}

void
tab_chapters::on_remove_chapter(wxCommandEvent &evt) {
  uint32_t i;
  wxTreeItemId id, p_id;
  chapter_node_data_c *d, *p_d;
  EbmlMaster *m, *del;

  id = tc_chapters->GetSelection();
  if (!id.IsOk() || (id == tid_root))
    return;

  p_id = tc_chapters->GetItemParent(id);
  if (p_id == tid_root)
    m = chapters;
  else {
    p_d = (chapter_node_data_c *)tc_chapters->GetItemData(p_id);
    if (p_d == NULL)
      return;
    if (p_d->is_atom)
      m = p_d->chapter;
    else
      m = p_d->eentry;
  }
  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (d == NULL)
    return;
  if (d->is_atom)
    del = d->chapter;
  else
    del = d->eentry;
  enable_buttons(false);
  enable_inputs(false);
  for (i = 0; i < m->ListSize(); i++) {
    if ((*m)[i] == (EbmlElement *)del) {
      delete del;
      m->Remove(i);
      tc_chapters->DeleteChildren(id);
      tc_chapters->Delete(id);
      return;
    }
  }
}

void
tab_chapters::on_entry_selected(wxTreeEvent &evt) {
  chapter_node_data_c *t;
  KaxChapterUID *cuid;
  KaxChapterDisplay *display;
  KaxChapterString *cstring;
  KaxChapterFlagHidden *fhidden;
  KaxChapterFlagEnabled *fenabled;
  wxString label, language;
  bool first, value;
  wxTreeItemId old_id;

  enable_buttons(true);

  old_id = evt.GetOldItem();
  if (old_id.IsOk() && (old_id != tid_root))
    copy_values(old_id);

  t = (chapter_node_data_c *)tc_chapters->GetItemData(evt.GetItem());
  lb_chapter_names->Clear();

  if ((evt.GetItem() == tid_root) || (t == NULL) || !t->is_atom) {
    enable_inputs(false);
    no_update = true;
    tc_chapter_name->SetValue(wxEmptyString);
    tc_start_time->SetValue(wxEmptyString);
    tc_end_time->SetValue(wxEmptyString);
    if ((evt.GetItem() != tid_root) && (t != NULL) && !t->is_atom) {
      KaxEditionUID *euid;
      wxString s;

      st_uid->Enable(true);
      tc_uid->Enable(true);
      euid = FINDFIRST(t->eentry, KaxEditionUID);
      if (euid != NULL)
        s.Printf(wxT("%u"), uint32(*euid));
      tc_uid->SetValue(s);
    } else {
      tc_uid->Enable(true);
      tc_uid->SetValue(wxEmptyString);
      tc_uid->Enable(false);
    }
    no_update = false;
    return;
  }

  enable_inputs(true);

  display = FINDFIRST(t->chapter, KaxChapterDisplay);
  if (display == NULL)
    wxdie(Z("on_entry_selected: display == NULL. Should not have happened."));
  first = true;
  while (display != NULL) {
    cstring = FindChild<KaxChapterString>(*display);
    if (cstring != NULL)
      label =
        UTFstring_to_wxString(*static_cast<EbmlUnicodeString *>(cstring));
    else
      label = Z("(unnamed)");

    if (first) {
      set_display_values(display);
      first = false;
    }
    lb_chapter_names->Append(label);
    display = FINDNEXT(t->chapter, KaxChapterDisplay, display);
  }

  set_timecode_values(t->chapter);

  fhidden = FINDFIRST(t->chapter, KaxChapterFlagHidden);
  if (fhidden == NULL)
    value = false;
  else
    value = uint8(*static_cast<EbmlUInteger *>(fhidden)) != 0;
  cb_flag_hidden->SetValue(value);

  fenabled = FINDFIRST(t->chapter, KaxChapterFlagEnabled);
  if (fenabled == NULL)
    value = true;
  else
    value = uint8(*static_cast<EbmlUInteger *>(fenabled)) != 0;
  cb_flag_enabled->SetValue(value);

  cuid = FINDFIRST(t->chapter, KaxChapterUID);
  if (cuid == NULL)
    label = wxEmptyString;
  else
    label.Printf(wxT("%u"), uint32(*cuid));
  tc_uid->SetValue(label);

  lb_chapter_names->SetSelection(0);
  b_remove_chapter_name->Enable(lb_chapter_names->GetCount() > 1);
  tc_chapter_name->SetSelection(-1, -1);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_language_code_selected(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  KaxChapterLanguage *clanguage;
  chapter_node_data_c *t;
  size_t i, n;
  wxString label;
  wxTreeItemId id;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  clanguage = &GetChild<KaxChapterLanguage>(*cdisplay);
  *static_cast<EbmlString *>(clanguage) =
    wxMB(extract_language_code(cob_language_code->GetStringSelection()));

  label = create_chapter_label(*t->chapter);
  tc_chapters->SetItemText(id, label);
}

void
tab_chapters::on_country_code_selected(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  KaxChapterCountry *ccountry;
  chapter_node_data_c *t;
  size_t i, n;
  wxTreeItemId id;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  if (cob_country_code->GetStringSelection().Length() == 0) {
    i = 0;
    while (i < cdisplay->ListSize()) {
      if (EbmlId(*(*cdisplay)[i]) == KaxChapterCountry::ClassInfos.GlobalId) {
        delete (*cdisplay)[i];
        cdisplay->Remove(i);
      } else
        i++;
    }
    return;
  }
  ccountry = &GetChild<KaxChapterCountry>(*cdisplay);
  *static_cast<EbmlString *>(ccountry) =
    wxMB(cob_country_code->GetStringSelection());
}

void
tab_chapters::on_chapter_name_changed(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  chapter_node_data_c *t;
  size_t i, n;
  wxString label;
  wxTreeItemId id;

  if (no_update)
    return;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  GetChildAs<KaxChapterString, EbmlUnicodeString>(*cdisplay) = cstrutf8_to_UTFstring(wxMB(tc_chapter_name->GetValue()));

  label = create_chapter_label(*t->chapter);
  tc_chapters->SetItemText(id, label);
  lb_chapter_names->SetString(lb_chapter_names->GetSelection(),
                              tc_chapter_name->GetValue());
}

void
tab_chapters::set_values_recursively(wxTreeItemId id,
                                     const wxString &s,
                                     bool set_language) {
  uint32_t i;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterCountry *country;
  chapter_node_data_c *d;
  wxTreeItemId next_child;
  wxTreeItemIdValue cookie;
  wxString text;

  if (!id.IsOk())
    return;

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((d != NULL) && (d->is_atom)) {
    display = FINDFIRST(d->chapter, KaxChapterDisplay);
    while (display != NULL) {
      i = 0;
      while (i < display->ListSize()) {
        if ((set_language &&
             (dynamic_cast<KaxChapterLanguage *>((*display)[i]) != NULL)) ||
            (!set_language &&
             (dynamic_cast<KaxChapterCountry *>((*display)[i]) != NULL))) {
          delete (*display)[i];
          display->Remove(i);
        } else
          i++;
      }

      if (set_language) {
        language = new KaxChapterLanguage;
        *static_cast<EbmlString *>(language) = wxMB(s);
        display->PushElement(*language);
      } else {
        country = new KaxChapterCountry;
        *static_cast<EbmlString *>(country) = wxMB(s);
        display->PushElement(*country);
      }

      display = FINDNEXT(d->chapter, KaxChapterDisplay, display);
    }
    text = create_chapter_label(*d->chapter);
    tc_chapters->SetItemText(id, text);
  }

  next_child = tc_chapters->GetFirstChild(id, cookie);
  while (next_child.IsOk()) {
    set_values_recursively(next_child, s, set_language);
    next_child = tc_chapters->GetNextChild(id, cookie);
  }
}

void
tab_chapters::on_set_values(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  wxTreeItemId id;
  chapter_node_data_c *t;
  wxString s;
  size_t i, n;
  chapter_values_dlg dlg(this);

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  if (dlg.ShowModal() != wxID_OK)
    return;

  if (dlg.cb_language->IsChecked()) {
    s = extract_language_code(dlg.cob_language->GetValue());
    if (!is_valid_iso639_2_code(wxMB(s))) {
      wxMessageBox(wxString::Format(Z("The language '%s' is not a valid language and cannot be selected."), s.c_str()), Z("Invalid language selected"), wxICON_ERROR | wxOK);
      return;
    }
    set_values_recursively(id, s, true);
  }
  if (dlg.cb_country->IsChecked()) {
    s = dlg.cob_country->GetValue();
    set_values_recursively(id, s, false);
  }

  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((t == NULL) || !t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  set_display_values(cdisplay);
}

void
tab_chapters::on_adjust_timecodes(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  wxTreeItemId id;
  chapter_node_data_c *t;
  wxString sadjustment;
  size_t i, n, offset;
  int64_t adjustment, mult;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  sadjustment =
    wxGetTextFromUser(Z("You can use this function for adjusting the timecodes\n"
                        "of the selected chapter and all its children by a fixed amount.\n"
                        "The amount can be positive or negative. The format used can be\n"
                        "either just a number in which case it is interpreted as the number of seconds,\n"
                        "it can be followed by the unit like 'ms' or 's' for milliseconds and seconds respectively,\n"
                        "or it can have the usual HH:MM:SS.mmm or HH:MM:SS format.\n"
                        "Example: -00:05:23 would let all the chapters begin\n"
                        "5minutes and 23seconds earlier than now."),
                      Z("Adjust chapter timecodes"));
  strip(sadjustment);
  if (sadjustment.Length() == 0)
    return;

  if (sadjustment.c_str()[0] == wxT('-')) {
    mult = -1;
    offset = 1;
  } else if (sadjustment.c_str()[0] == wxT('+')) {
    mult = 1;
    offset = 1;
  } else {
    mult = 1;
    offset = 0;
  }
  sadjustment.Remove(0, offset);
  if (sadjustment.Length() == 0)
    return;
  adjustment = parse_time(sadjustment);
  if (adjustment == -1) {
    wxMessageBox(Z("Invalid format used for the adjustment."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }
  adjustment *= mult;

  adjust_timecodes_recursively(id, adjustment);

  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((t == NULL) || !t->is_atom)
    return;

  set_timecode_values(t->chapter);

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  set_display_values(cdisplay);
}

void
tab_chapters::adjust_timecodes_recursively(wxTreeItemId id,
                                           int64_t adjust_by) {
  KaxChapterTimeStart *tstart;
  KaxChapterTimeEnd *tend;
  chapter_node_data_c *d;
  wxTreeItemId next_child;
  wxTreeItemIdValue cookie;
  wxString text;
  int64_t t;

  if (!id.IsOk())
    return;

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((d != NULL) && (d->is_atom)) {
    tstart = FINDFIRST(d->chapter, KaxChapterTimeStart);
    if (tstart != NULL) {
      t = uint64(*tstart);
      t += adjust_by;
      if (t < 0)
        t = 0;
      *static_cast<EbmlUInteger *>(tstart) = t;
    }
    tend = FINDFIRST(d->chapter, KaxChapterTimeEnd);
    if (tend != NULL) {
      t = uint64(*tend);
      t += adjust_by;
      if (t < 0)
        t = 0;
      *static_cast<EbmlUInteger *>(tend) = t;
    }
    text = create_chapter_label(*d->chapter);
    tc_chapters->SetItemText(id, text);
  }

  next_child = tc_chapters->GetFirstChild(id, cookie);
  while (next_child.IsOk()) {
    adjust_timecodes_recursively(next_child, adjust_by);
    next_child = tc_chapters->GetNextChild(id, cookie);
  }
}

bool
tab_chapters::copy_values(wxTreeItemId id) {
  chapter_node_data_c *data;
  wxString label;
  KaxChapterAtom *chapter;
  EbmlElement *e;
  int64_t ts_start, ts_end;
  std::vector<std::string> l_codes, c_codes;
  wxString s;
  uint32_t i;

  data = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (data == NULL)
    return true;

  label = tc_chapters->GetItemText(id);
  chapter = data->chapter;

  s = tc_uid->GetValue();
  if (s.Length() > 0) {
    if (!parse_int(wxMB(s), ts_start))
      wxMessageBox(Z("Invalid UID. A UID is simply a number."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    else {
      i = ts_start;
      if (data->is_atom) {
        KaxChapterUID *cuid;

        cuid = &GetChild<KaxChapterUID>(*data->chapter);
        if (ts_start != uint32(*cuid)) {
          if (!is_unique_uint32(i, UNIQUE_CHAPTER_IDS)) {
            wxMessageBox(Z("Invalid UID. This chapter UID is already in use. The original UID has not been changed."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
            tc_uid->SetValue(wxString::Format(wxT("%u"), uint32(*cuid)));
          } else {
            remove_unique_uint32(uint32(*cuid), UNIQUE_CHAPTER_IDS);
            add_unique_uint32(ts_start, UNIQUE_CHAPTER_IDS);
            *static_cast<EbmlUInteger *>(cuid) = ts_start;
          }
        }
      } else {
        KaxEditionUID *euid;

        euid = &GetChild<KaxEditionUID>(*data->eentry);
        if (ts_start != uint32(*euid)) {
          if (!is_unique_uint32(i, UNIQUE_EDITION_IDS)) {
            wxMessageBox(Z("Invalid UID. This edition UID is already in use. The original UID has not been changed."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
            tc_uid->SetValue(wxString::Format(wxT("%u"), uint32(*euid)));
          } else {
            remove_unique_uint32(uint32(*euid), UNIQUE_EDITION_IDS);
            add_unique_uint32(ts_start, UNIQUE_EDITION_IDS);
            *static_cast<EbmlUInteger *>(euid) = ts_start;
          }
        }
      }
    }
  }
  if (data->is_atom) {
    s = tc_start_time->GetValue();
    strip(s);
    ts_start = parse_time(s);
    if (ts_start == -1) {
      wxMessageBox(wxString::Format(Z("Invalid format used for the start time for '%s'. Setting value to 0."), label.c_str()), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      ts_start = 0;
    }

    s = tc_end_time->GetValue();
    strip(s);
    ts_end = parse_time(s);
    if (ts_end == -1) {
      wxMessageBox(wxString::Format(Z("Invalid format used for the end time for '%s'. Setting value to 0."), label.c_str()), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      ts_end = 0;
    }

    i = 0;
    while (i < chapter->ListSize()) {
      e = (*chapter)[i];
      if ((EbmlId(*e) == KaxChapterTimeStart::ClassInfos.GlobalId) ||
          (EbmlId(*e) == KaxChapterTimeEnd::ClassInfos.GlobalId)) {
        chapter->Remove(i);
        delete e;
      } else
        i++;
    }

    if (ts_start >= 0)
      GetChildAs<KaxChapterTimeStart, EbmlUInteger>(*chapter) = ts_start;
    if (ts_end >= 0)
      GetChildAs<KaxChapterTimeEnd,   EbmlUInteger>(*chapter) = ts_end;

    label = create_chapter_label(*chapter);
    tc_chapters->SetItemText(id, label);
  }

  return true;
}

void
tab_chapters::on_add_chapter_name(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  chapter_node_data_c *t;
  size_t i, n;
  wxTreeItemId id;
  wxString s;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  cdisplay = new KaxChapterDisplay;
  GetChildAs<KaxChapterString, EbmlUnicodeString>(*cdisplay) = cstrutf8_to_UTFstring(Y("(unnamed)"));
  if (g_default_chapter_language.length() > 0)
    s = wxU(g_default_chapter_language);
  else
    s = wxT("eng");
  GetChildAs<KaxChapterLanguage, EbmlString>(*cdisplay) = wxMB(s);
  if (g_default_chapter_country.length() > 0)
    GetChildAs<KaxChapterCountry, EbmlString>(*cdisplay) = g_default_chapter_country.c_str();
  // Workaround for a bug in libebml
  if (i == (t->chapter->ListSize() - 1))
    t->chapter->PushElement(*cdisplay);
  else
    t->chapter->InsertElement(*cdisplay, i + 1);
  s = Z("(unnamed)");
  lb_chapter_names->InsertItems(1, &s, lb_chapter_names->GetSelection() + 1);
  lb_chapter_names->SetSelection(lb_chapter_names->GetSelection() + 1);
  set_display_values(cdisplay);

  b_remove_chapter_name->Enable(true);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_remove_chapter_name(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  chapter_node_data_c *t;
  size_t i, n;
  wxTreeItemId id;
  wxString s;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t->is_atom)
    return;

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }

  if (cdisplay == NULL)
    return;

  t->chapter->Remove(i);
  delete cdisplay;

  n = lb_chapter_names->GetSelection();
  lb_chapter_names->Delete(n);
  lb_chapter_names->SetSelection(n == 0 ? 0 : n - 1);
  if (n == 0) {
    s = create_chapter_label(*t->chapter);
    tc_chapters->SetItemText(id, s);
  }

  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }

  if (cdisplay != NULL)
    set_display_values(cdisplay);
  tc_chapter_name->SetFocus();
  b_remove_chapter_name->Enable(lb_chapter_names->GetCount() > 1);
}

void
tab_chapters::on_chapter_name_selected(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  chapter_node_data_c *t;
  size_t i, n;
  wxTreeItemId id;
  wxString s;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (!t || !t->is_atom)
    return;
  cdisplay = NULL;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  set_display_values(cdisplay);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_chapter_name_enter(wxCommandEvent &evt) {
  wxTreeItemId id;
  wxTreeItemIdValue cookie;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  id = tc_chapters->GetNextSibling(id);
  if (id.IsOk()) {
    tc_chapters->SelectItem(id);
    return;
  }

  id = tc_chapters->GetItemParent(tc_chapters->GetSelection());
  if (!id.IsOk())
    return;
  id = tc_chapters->GetNextSibling(id);
  if (id.IsOk()) {
    id = tc_chapters->GetFirstChild(id, cookie);
    if (id.IsOk()) {
      tc_chapters->SelectItem(id);
      return;
    }
  }

  id = tc_chapters->GetRootItem();
  if (!id.IsOk())
    return;
  id = tc_chapters->GetFirstChild(id, cookie);
  if (!id.IsOk())
    return;
  id = tc_chapters->GetFirstChild(id, cookie);
  if (id.IsOk())
    tc_chapters->SelectItem(id);
}

void
tab_chapters::on_flag_hidden(wxCommandEvent &evt) {
  wxTreeItemId selected;
  chapter_node_data_c *t;
  size_t i;

  selected = tc_chapters->GetSelection();
  if (!selected.IsOk() || (selected == tid_root))
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(selected);
  if ((t == NULL) || !t->is_atom)
    return;

  if (cb_flag_hidden->IsChecked())
    *static_cast<EbmlUInteger *>
      (&GetChild<KaxChapterFlagHidden>(*t->chapter)) = 1;
  else
    for (i = 0; i < t->chapter->ListSize(); i++)
      if (is_id((*t->chapter)[i], KaxChapterFlagHidden)) {
        t->chapter->Remove(i);
        return;
      }
}

void
tab_chapters::on_flag_enabled(wxCommandEvent &evt) {
  wxTreeItemId selected;
  chapter_node_data_c *t;
  size_t i;

  selected = tc_chapters->GetSelection();
  if (!selected.IsOk() || (selected == tid_root))
    return;
  t = (chapter_node_data_c *)tc_chapters->GetItemData(selected);
  if ((t == NULL) || !t->is_atom)
    return;

  if (!cb_flag_enabled->IsChecked())
    *static_cast<EbmlUInteger *>
      (&GetChild<KaxChapterFlagEnabled>(*t->chapter)) = 0;
  else
    for (i = 0; i < t->chapter->ListSize(); i++)
      if (is_id((*t->chapter)[i], KaxChapterFlagEnabled)) {
        t->chapter->Remove(i);
        return;
      }
}

void
tab_chapters::set_timecode_values(KaxChapterAtom *atom) {
  KaxChapterTimeStart *tstart;
  KaxChapterTimeEnd *tend;
  wxString label;
  int64_t timestamp;

  no_update = true;

  tstart = FINDFIRST(atom, KaxChapterTimeStart);
  if (tstart != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tstart));
    label.Printf(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
    tc_start_time->SetValue(label);
  } else
    tc_start_time->SetValue(wxEmptyString);

  tend = FINDFIRST(atom, KaxChapterTimeEnd);
  if (tend != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tend));
    label.Printf(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
    tc_end_time->SetValue(label);
  } else
    tc_end_time->SetValue(wxEmptyString);

  no_update = false;
}

void
tab_chapters::set_display_values(KaxChapterDisplay *display) {
  KaxChapterString *cstring;
  KaxChapterLanguage *clanguage;
  KaxChapterCountry *ccountry;
  wxString language;
  uint32_t i, count;
  bool found;

  no_update = true;

  cstring = FINDFIRST(display, KaxChapterString);
  if (cstring != NULL) {
    wxString tmp =
      UTFstring_to_wxString(*static_cast<EbmlUnicodeString *>(cstring));
    tc_chapter_name->SetValue(tmp);
  } else
    tc_chapter_name->SetValue(wxT("(unnamed)"));

  clanguage = FINDFIRST(display, KaxChapterLanguage);
  if (clanguage != NULL)
    language = wxU(clanguage);
  else
    language = wxT("eng");
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    if (extract_language_code(sorted_iso_codes[i]) == language) {
      language = sorted_iso_codes[i];
      break;
    }

  count = cob_language_code->GetCount();
  found = false;
  for (i = 0; i < count; i++)
    if (extract_language_code(cob_language_code->GetString(i)) == language) {
      found = true;
      break;
    }
  if (found)
    cob_language_code->SetSelection(i);
  else
    cob_language_code->SetValue(language);

  ccountry = FINDFIRST(display, KaxChapterCountry);
  if (ccountry != NULL)
    cob_country_code->SetValue(wxU(ccountry));
  else
    cob_country_code->SetValue(wxEmptyString);

  no_update = false;
}

int64_t
tab_chapters::parse_time(wxString s) {
  int64_t nsecs;
  std::string utf8s;
  const char *c;

  utf8s = wxMB(s);
  strip(utf8s);
  if (utf8s.length() == 0)
    return -2;

  c = utf8s.c_str();
  while (*c != 0) {
    if (!isdigit(*c)) {
      if (parse_timecode(utf8s, nsecs))
        return nsecs;
      return -1;
    }
    c++;
  }
  if (!parse_int(utf8s, nsecs))
    return -1;
  return nsecs * 1000000000;
}

bool
tab_chapters::is_empty() {
  if (!tid_root.IsOk())
    return true;
  return tc_chapters->GetCount() < 2;
}

void
tab_chapters::write_chapters_to_matroska_file() {
  kax_analyzer_c::update_element_result_e result = (0 == chapters->ListSize() ? analyzer->remove_elements(KaxChapters::ClassInfos.GlobalId) : analyzer->update_element(chapters));

  switch (result) {
    case kax_analyzer_c::uer_success:
      mdlg->set_status_bar(Z("Chapters written."));
      break;

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
                     "Please use your favorite player to check this file.\n\n"
                     "The proper solution is to save these chapters into a XML file and then to remux the file with the chapters included."),
                   Z("File structure warning"), wxOK | wxCENTER | wxICON_EXCLAMATION, this);
      break;

    default:
      wxMessageBox(Z("An unknown error occured. The file has been modified."), Z("Internal program error"), wxOK | wxCENTER | wxICON_ERROR, this);
  }
}

IMPLEMENT_CLASS(chapter_values_dlg, wxDialog);
BEGIN_EVENT_TABLE(chapter_values_dlg, wxDialog)
  EVT_CHECKBOX(ID_CVD_CB_LANGUAGE, chapter_values_dlg::on_language_clicked)
  EVT_CHECKBOX(ID_CVD_CB_COUNTRY,  chapter_values_dlg::on_country_clicked)
END_EVENT_TABLE();

IMPLEMENT_CLASS(tab_chapters, wxPanel);
BEGIN_EVENT_TABLE(tab_chapters, wxPanel)
  EVT_BUTTON(ID_B_ADDCHAPTER,                   tab_chapters::on_add_chapter)
  EVT_BUTTON(ID_B_ADDSUBCHAPTER,                tab_chapters::on_add_subchapter)
  EVT_BUTTON(ID_B_REMOVECHAPTER,                tab_chapters::on_remove_chapter)
  EVT_BUTTON(ID_B_SETVALUES,                    tab_chapters::on_set_values)
  EVT_BUTTON(ID_B_ADJUSTTIMECODES,              tab_chapters::on_adjust_timecodes)
  EVT_BUTTON(ID_B_ADD_CHAPTERNAME,              tab_chapters::on_add_chapter_name)
  EVT_BUTTON(ID_B_REMOVE_CHAPTERNAME,           tab_chapters::on_remove_chapter_name)
  EVT_TREE_SEL_CHANGED(ID_TRC_CHAPTERS,         tab_chapters::on_entry_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTLANGUAGECODE, tab_chapters::on_language_code_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTCOUNTRYCODE,  tab_chapters::on_country_code_selected)
  EVT_LISTBOX(ID_LB_CHAPTERNAMES,               tab_chapters::on_chapter_name_selected)
  EVT_TEXT(ID_TC_CHAPTERNAME,                   tab_chapters::on_chapter_name_changed)
  EVT_CHECKBOX(ID_CB_CHAPTERHIDDEN,             tab_chapters::on_flag_hidden)
  EVT_CHECKBOX(ID_CB_CHAPTERENABLED,            tab_chapters::on_flag_enabled)
  EVT_TEXT_ENTER(ID_TC_CHAPTERNAME,             tab_chapters::on_chapter_name_enter)
END_EVENT_TABLE();
