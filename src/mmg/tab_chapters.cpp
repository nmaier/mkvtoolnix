/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_chapters.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id: tab_chapters.cpp 1045 2003-09-17 11:56:16Z mosu $
    \brief "chapter editor" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <stdio.h>

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statline.h"

#include <ebml/EbmlStream.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "error.h"
#include "extern_data.h"
#include "iso639.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "kax_analyzer.h"
#include "tab_chapters.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

#if ! wxCHECK_VERSION(2,4,2)
# define wxTreeItemIdValue long
#endif

class chapters_drop_target_c: public wxFileDropTarget {
private:
  tab_chapters *owner;
public:
  chapters_drop_target_c(tab_chapters *n_owner):
    owner(n_owner) {};
  virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &files) {
    owner->load(files[0]);

    return true;
  }
};

#define FINDFIRST(p, c) (static_cast<c *> \
  (((EbmlMaster *)p)->FindFirstElt(c::ClassInfos, false)))
#define FINDNEXT(p, c, e) (static_cast<c *> \
  (((EbmlMaster *)p)->FindNextElt(*e, false)))

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
  wxComboBox *cob_language, *cob_country;
  wxCheckBox *cb_language, *cb_country;

public:
  chapter_values_dlg(wxWindow *parent, bool set_defaults,
                     wxString old_def_language = wxT(""),
                     wxString old_def_country = wxT(""));
  void on_language_clicked(wxCommandEvent &evt);
  void on_country_clicked(wxCommandEvent &evt);
};

chapter_values_dlg::chapter_values_dlg(wxWindow *parent,
                                       bool set_defaults,
                                       wxString old_def_language,
                                       wxString old_def_country):
  wxDialog(parent, 0, wxT(""), wxDefaultPosition, wxSize(350, 200)) {
  wxBoxSizer *siz_all, *siz_buttons, *siz_line;
  wxFlexGridSizer *siz_input;
  wxButton *b_ok, *b_cancel;
  uint32_t i;

  siz_all = new wxBoxSizer(wxVERTICAL);
  if (set_defaults) {
    SetTitle(wxT("Change the default values"));
    siz_all->Add(new wxStaticText(this, wxID_STATIC,
                                  wxT("Here you can set the default values "
                                      "that mmg will use\nfor each chapter "
                                      "that you create. These values can\n"
                                      "then be changed if needed. The default "
                                      "values will be\nsaved when you exit "
                                      "mmg.")),
                 0, wxLEFT | wxRIGHT | wxTOP, 10);

    siz_input = new wxFlexGridSizer(2);
    siz_input->AddGrowableCol(1);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->AddGrowableRow(0);

    siz_input->Add(new wxStaticText(this, wxID_STATIC, wxT("Language:")),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    cob_language = new wxComboBox(this, wxID_STATIC, wxT(""));
    siz_input->Add(cob_language, 0, wxGROW, 0);

    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->AddGrowableRow(2);

    siz_input->Add(new wxStaticText(this, wxID_STATIC, wxT("Country:")),
                   0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    cob_country = new wxComboBox(this, wxID_STATIC, wxT(""));
    siz_input->Add(cob_country, 0, wxGROW, 0);

  } else {
    SetTitle(wxT("Select values to be applied"));
    siz_all->Add(new wxStaticText(this, wxID_STATIC,
                                  wxT("Please enter the values for the "
                                      "language and the\ncountry that you "
                                      "want to apply to all the chapters\n"
                                      "below and including the currently "
                                      "selected entry.")),
                 0, wxLEFT | wxRIGHT | wxTOP, 10);

    siz_input = new wxFlexGridSizer(2);
    siz_input->AddGrowableCol(1);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->AddGrowableRow(0);

    cb_language =
      new wxCheckBox(this, ID_CVD_CB_LANGUAGE, wxT("Set language to:"));
    cb_language->SetValue(false);
    siz_input->Add(cb_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    cob_language = new wxComboBox(this, wxID_STATIC, wxT(""));
    cob_language->Enable(false);
    siz_input->Add(cob_language, 0, wxGROW, 0);

    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->Add(0, 1, 1, wxGROW, 0);
    siz_input->AddGrowableRow(2);

    cb_country =
      new wxCheckBox(this, ID_CVD_CB_COUNTRY, wxT("Set country to:"));
    cb_country->SetValue(false);
    siz_input->Add(cb_country, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    cob_country = new wxComboBox(this, wxID_STATIC, wxT(""));
    cob_country->Enable(false);
    siz_input->Add(cob_country, 0, wxGROW, 0);

  }

  siz_all->Add(siz_input, 3, wxGROW | wxLEFT | wxRIGHT, 25);

  cob_language->Append(wxT(""));
  for (i = 0; i < sorted_iso_codes.Count(); i++) {
    cob_language->Append(sorted_iso_codes[i]);
    if (extract_language_code(sorted_iso_codes[i]) == old_def_language)
      cob_language->SetValue(sorted_iso_codes[i]);
  }

  cob_country->Append(wxT(""));
  for (i = 0; cctlds[i] != NULL; i++)
    cob_country->Append(wxU(cctlds[i]));
  cob_country->SetValue(old_def_country);

  siz_buttons = new wxBoxSizer(wxVERTICAL);
  siz_buttons->Add(0, 1, 1, wxGROW, 0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  b_ok = new wxButton(this, wxID_OK, wxT("Ok"));
  b_ok->SetDefault();
  b_cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
  b_ok->SetSize(b_cancel->GetSize());
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(b_ok, 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(b_cancel, 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_buttons->Add(siz_line, 0, wxGROW, 0);
  siz_all->Add(siz_buttons, 2, wxGROW | wxBOTTOM, 10);
  SetSizer(siz_all);
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
  uint32_t i;

  m_chapters = nm_chapters;

  new wxStaticText(this, wxID_STATIC, wxT("Chapters:"), wxPoint(10, 5),
                   wxDefaultSize, 0);
#ifdef SYS_WINDOWS
  tc_chapters =
    new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxPoint(10, 24), wxSize(350, 200));
#else
  tc_chapters =
    new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxPoint(10, 24), wxSize(350, 200),
                   wxSUNKEN_BORDER | wxTR_HAS_BUTTONS);
#endif

  b_add_chapter =
    new wxButton(this, ID_B_ADDCHAPTER, wxT("Add chapter"), wxPoint(370, 24),
                 wxSize(120, -1));

  b_add_subchapter =
    new wxButton(this, ID_B_ADDSUBCHAPTER, wxT("Add subchapter"),
                 wxPoint(370, 50), wxSize(120, -1));

  b_remove_chapter =
    new wxButton(this, ID_B_REMOVECHAPTER, wxT("Remove chapter"),
                 wxPoint(370, 76), wxSize(120, -1));

  b_set_values =
    new wxButton(this, ID_B_SETVALUES, wxT("Set values"), wxPoint(370, 128),
                 wxSize(120, -1));
  b_set_values->SetToolTip(TIP("Here you can set the values for the language "
                               "and "
                               "the country that you want to apply to all the "
                               "chapters below and including the currently "
                               "selected entry."));

  b_adjust_timecodes =
    new wxButton(this, ID_B_ADJUSTTIMECODES, wxT("Adjust timecodes"),
                 wxPoint(370, 154), wxSize(120, -1));
  b_adjust_timecodes->SetToolTip(TIP("Here you can adjust all the timcdoes "
                                     "of the "
                                     "selected chapter and all its childrend "
                                     "by "
                                     "a specific amount either increasing or "
                                     "decreasing it."));

  new wxStaticText(this, wxID_STATIC, wxT("Start:"), wxPoint(10, 235));
  tc_start_time =
    new wxTextCtrl(this, ID_TC_CHAPTERSTART, wxT(""),
                   wxPoint(50, 235 + YOFF), wxSize(125, -1));

  new wxStaticText(this, wxID_STATIC, wxT("End:"), wxPoint(190, 235));
  tc_end_time =
    new wxTextCtrl(this, ID_TC_CHAPTEREND, wxT(""),
                   wxPoint(235, 235 + YOFF), wxSize(125, -1));

  new wxStaticBox(this, wxID_STATIC, wxT("Chapter names and languages"),
                  wxPoint(10, 270), wxSize(480, 180));

  lb_chapter_names =
    new wxListBox(this, ID_LB_CHAPTERNAMES, wxPoint(25, 300), wxSize(335, 64),
                  0);
  b_add_chapter_name =
    new wxButton(this, ID_B_ADD_CHAPTERNAME, wxT("Add name"),
                 wxPoint(370, 300), wxSize(100, -1));
  b_remove_chapter_name =
    new wxButton(this, ID_B_REMOVE_CHAPTERNAME, wxT("Remove name"),
                 wxPoint(370, 326), wxSize(100, -1));

  new wxStaticText(this, wxID_STATIC, wxT("Name:"), wxPoint(25, 375));
  tc_chapter_name =
    new wxTextCtrl(this, ID_TC_CHAPTERNAME, wxT(""), wxPoint(X1, 375 + YOFF),
                   wxSize(470 - X1, -1));

  new wxStaticText(this, wxID_STATIC, wxT("Language:"),
                   wxPoint(25, 380 + Y1));
  cob_language_code =
    new wxComboBox(this, ID_CB_CHAPTERSELECTLANGUAGECODE, wxT(""),
                   wxPoint(X1, 380 + Y1 + YOFF), wxSize(120, -1), 0, NULL);
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language_code->Append(sorted_iso_codes[i]);
  cob_language_code->SetValue(wxT(""));

  new wxStaticText(this, wxID_STATIC, wxT("Country:"),
                   wxPoint(470 - 120 - (X1 -25), 380  + Y1));
  cob_country_code =
    new wxComboBox(this, ID_CB_CHAPTERSELECTCOUNTRYCODE, wxT(""),
                   wxPoint(470 - 120, 380 + Y1 + YOFF),
                   wxSize(120, -1), 0, NULL);
  cob_country_code->Append(wxT(""));
  for (i = 0; cctlds[i] != NULL; i++)
    cob_country_code->Append(wxU(cctlds[i]));
  cob_country_code->SetValue(wxT(""));

  enable_inputs(false);

  m_chapters->Enable(ID_M_CHAPTERS_SAVE, false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVEAS, false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVETOKAX, false);
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, false);
  enable_buttons(false);

  file_name = wxT("");
  chapters = NULL;
  analyzer = NULL;
  source_is_kax_file = false;
  source_is_simple_format = false;

  no_update = false;

  SetDropTarget(new chapters_drop_target_c(this));
}

tab_chapters::~tab_chapters() {
  if (chapters != NULL)
    delete chapters;
  if (analyzer != NULL)
    delete analyzer;
}

void
tab_chapters::enable_inputs(bool enable) {
  tc_chapter_name->Enable(enable);
  tc_start_time->Enable(enable);
  tc_end_time->Enable(enable);
  cob_language_code->Enable(enable);
  cob_country_code->Enable(enable);
  lb_chapter_names->Enable(enable);
  b_add_chapter_name->Enable(enable);
  b_remove_chapter_name->Enable(enable);
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
  file_name = wxT("");
  if (chapters != NULL)
    delete chapters;
  tc_chapters->DeleteAllItems();
  tid_root = tc_chapters->AddRoot(wxT("(new chapter file)"));
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

  mdlg->set_status_bar(wxT("New chapters created."));
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

  label = wxT("(unnamed chapter)");
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
    label += wxU(string(*static_cast<EbmlString *>(language)).c_str());
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
      s.Printf(wxT("Edition %d"),
               tc_chapters->GetChildrenCount(tid_root, false) + 1);
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
        add_unique_uint32(uint32(*static_cast<EbmlUInteger *>(u)));
    }
  }
}

void
tab_chapters::on_load_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose a chapter file"), last_open_dir, wxT(""),
                   wxT("Chapter files (*.xml;*.txt;*.mka;*.mkv;*.cue)|*.xml;"
                       "*.txt;*.mka;*.mkv;*.cue|" ALLFILES), wxOPEN);

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

  try {
    if (kax_analyzer_c::probe(wxMBL(name))) {
      if (analyzer != NULL)
        delete analyzer;
      analyzer = new kax_analyzer_c(this, wxMBL(name));
      file_name = name;
      if (!analyzer->process()) {
        delete analyzer;
        analyzer = NULL;
        return false;
      }
      pos = analyzer->find(KaxChapters::ClassInfos.GlobalId);
      if (pos == -1) {
        wxMessageBox(wxT("This file does not contain any chapters."),
                     wxT("No chapters found"), wxOK | wxCENTER |
                     wxICON_INFORMATION);
        delete analyzer;
        analyzer = NULL;
        return false;
      }

      e = analyzer->read_element(pos, KaxChapters::ClassInfos);
      if (e == NULL)
        throw error_c("This file does not contain valid chapters.");
      new_chapters = static_cast<KaxChapters *>(e);
      source_is_kax_file = true;
      source_is_simple_format = false;

    } else {
      new_chapters = parse_chapters(wxMBL(name), 0, -1, 0, NULL, NULL, true,
                                    &source_is_simple_format);
      source_is_kax_file = false;
    }
  } catch (error_c ex) {
    analyzer = NULL;
    s = wxU((const char *)ex);
    break_line(s);
    while (s[s.Length() - 1] == wxT('\n'))
      s.Remove(s.Length() - 1);
    wxMessageBox(s, wxT("Error parsing the file"),
                 wxOK | wxCENTER | wxICON_ERROR);
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
  clear_list_of_unique_uint32();
  fix_missing_languages(*chapters);
  tid_root = tc_chapters->AddRoot(file_name);
  add_recursively(tid_root, *chapters);
  expand_subtree(*tc_chapters, tid_root);

  enable_inputs(false);
    
  mdlg->set_last_chapters_in_menu(name);
  mdlg->set_status_bar(wxT("Chapters loaded."));

  return true;
}

void
tab_chapters::on_save_chapters(wxCommandEvent &evt) {
  if (!verify())
    return;

  if (source_is_kax_file) {
    if (analyzer->update_element(chapters))
      mdlg->set_status_bar(wxT("Chapters written."));
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

  wxFileDialog dlg(this, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT("Matroska files (*.mkv;*.mka)|*.mkv;*.mka|"
                      ALLFILES), wxSAVE);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (!kax_analyzer_c::probe(wxMBL(dlg.GetPath()))) {
    wxMessageBox(wxT("The file you tried to save to is NOT a Matroska file."),
                 wxT("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  last_open_dir = dlg.GetDirectory();
  file_name = dlg.GetPath();
  tc_chapters->SetItemText(tid_root, file_name);

  source_is_kax_file = true;
  source_is_simple_format = false;

  if (analyzer != NULL)
    delete analyzer;
  analyzer = new kax_analyzer_c(this, wxMBL(file_name));
  if (!analyzer->process()) {
    delete analyzer;
    analyzer = NULL;
    return;
  }
  if (analyzer->update_element(chapters))
    mdlg->set_status_bar(wxT("Chapters written."));
  mdlg->set_last_chapters_in_menu(file_name);
}

void
tab_chapters::on_save_chapters_as(wxCommandEvent &evt) {
  if (!verify())
    return;

  if (!select_file_name())
    return;
  save();
}

bool
tab_chapters::select_file_name() {
  wxFileDialog dlg(this, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT("Chapter files (*.xml)|*.xml|"
                       ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    if (kax_analyzer_c::probe(wxMBL(dlg.GetPath()))) {
      wxMessageBox(wxT("The file you tried to save to is a Matroska file. For "
                       "this to work you have to use the 'Save to Matroska "
                       "file' menu option."), wxT("Wrong file selected"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return false;
    }
    last_open_dir = dlg.GetDirectory();
    file_name = dlg.GetPath();
    tc_chapters->SetItemText(tid_root, file_name);
    return true;
  }
  return false;
}

void
tab_chapters::save() {
  FILE *fout;
  wxString err;

  fout = fopen(wxMBL(file_name), "w");
  if (fout == NULL) {
    err.Printf(wxT("Could not open the destination file '%s' for writing."
                   " Error code: %d (%s)."), file_name.c_str(), errno,
               wxUCS(strerror(errno)));
    wxMessageBox(err, wxT("Error opening file"),
                 wxCENTER | wxOK | wxICON_ERROR);
    return;
  }

  fprintf(fout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<Chapters>\n");
  write_chapters_xml(chapters, fout);
  fprintf(fout, "</Chapters>\n");
  fclose(fout);

  source_is_kax_file = false;
  source_is_simple_format = false;

  mdlg->set_last_chapters_in_menu(file_name);
  mdlg->set_status_bar(wxT("Chapters written."));
}

void
tab_chapters::on_verify_chapters(wxCommandEvent &evt) {
  verify();
}

bool
tab_chapters::verify_atom_recursively(EbmlElement *e,
                                      int64_t p_start,
                                      int64_t p_end) {
  KaxChapterUID *uid;
  KaxChapterAtom *chapter;
  KaxChapterTimeStart *start;
  KaxChapterTimeEnd *end;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterString *cs;
  wxString label;
  uint32_t i;
  int64_t t_start, t_end;

  chapter = static_cast<KaxChapterAtom *>(e);

  if (FindChild<KaxChapterUID>(*chapter) == NULL) {
    uid = &GetChild<KaxChapterUID>(*chapter);
    *static_cast<EbmlUInteger *>(uid) = create_unique_uint32();
  }

  cs = NULL;
  display = FindChild<KaxChapterDisplay>(*chapter);
  if (display != NULL)
    cs = FindChild<KaxChapterString>(*display);

  if ((display == NULL) || (cs == NULL)) {
    wxMessageBox(wxT("One of the chapters does not have a name."),
                 wxT("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }
  label =
    UTFstring_to_wxString(UTFstring(*static_cast<EbmlUnicodeString *>(cs)));

  start = FindChild<KaxChapterTimeStart>(*chapter);
  if (start == NULL) {
    wxMessageBox(wxT("The chapter '") + label +
                 wxT("' is missing the start time."),
                 wxT("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }
  t_start = uint64(*static_cast<EbmlUInteger *>(start));
  if ((p_start != -1) && (t_start < p_start)) {
    wxMessageBox(wxT("The chapter '") + label +
                 wxT("' starts before its parent."),
                 wxT("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }

  end = FindChild<KaxChapterTimeEnd>(*chapter);
  if (end != NULL) {
    t_end = uint64(*static_cast<EbmlUInteger *>(end));
    if ((p_end != -1) && (t_end > p_end)) {
      wxMessageBox(wxT("The chapter '") + label +
                   wxT("' ends after its parent."),
                   wxT("Chapter verification error"), wxCENTER | wxOK |
                   wxICON_ERROR);
      return false;
    }
    if (t_end < t_start) {
      wxMessageBox(wxT("The chapter '") + label +
                   wxT("' ends before it starts."),
                   wxT("Chapter verification error"), wxCENTER | wxOK |
                   wxICON_ERROR);
      return false;
    }
  } else
    t_end = -1;

  language = FindChild<KaxChapterLanguage>(*display);
  if (language == NULL) {
    wxMessageBox(wxT("The chapter '") + label +
                 wxT("' is missing its language."),
                 wxT("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }

  for (i = 0; i < chapter->ListSize(); i++)
    if (dynamic_cast<KaxChapterAtom *>((*chapter)[i]) != NULL)
      if (!verify_atom_recursively((*chapter)[i], t_start, t_end))
        return false;

  return true;
}

bool tab_chapters::verify() {
  KaxEditionEntry *eentry;
  wxTreeItemId id;
  uint32_t eidx, cidx;

  if (chapters == NULL)
    return false;
  if (chapters->ListSize() == 0)
    return false;

  id = tc_chapters->GetSelection();
  if (id.IsOk())
    copy_values(id);

  for (eidx = 0; eidx < chapters->ListSize(); eidx++) {
    eentry = static_cast<KaxEditionEntry *>((*chapters)[eidx]);
    for (cidx = 0; cidx < eentry->ListSize(); cidx++) {
      if (!verify_atom_recursively((*eentry)[cidx]))
        return false;
    }
  }

  if (!chapters->CheckMandatory())
    die("verify failed: chapters->CheckMandatory() is false. This should not "
        "have happened. Please file a bug report.\n");
  chapters->UpdateSize();

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
  vector<EbmlElement *> tmpvec;
  uint32_t start, i;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  copy_values(id);

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if (id == tid_root) {
    eentry = new KaxEditionEntry;
    chapters->PushElement(*eentry);
    d = new chapter_node_data_c(eentry);
    s.Printf(wxT("EditionEntry %u"), chapters->ListSize());
    id = tc_chapters->AppendItem(tid_root, s, -1, -1, d);
  }

  chapter = new KaxChapterAtom;
  while (chapter->ListSize() > 0) {
    delete (*chapter)[0];
    chapter->Remove(0);
  }
  KaxChapterDisplay &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(display)) =
    cstr_to_UTFstring("(unnamed)");
  if ((default_chapter_language.length() > 0) ||
      (default_chapter_country.length() > 0)) {
    if (default_chapter_language.length() > 0)
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(display)) =
        default_chapter_language.c_str();
    if (default_chapter_country.length() > 0)
      *static_cast<EbmlString *>(&GetChild<KaxChapterCountry>(display)) =
        default_chapter_country.c_str();
  }
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
      die("start >= m->ListSize(). This should not have happened. Please "
          "file a bug report. Thanks.\n");
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
    s.Printf(wxT("EditionEntry %u"), chapters->ListSize());
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
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(display)) =
    cstr_to_UTFstring("(unnamed)");
  if ((default_chapter_language.length() > 0) ||
      (default_chapter_country.length() > 0)) {
    if (default_chapter_language.length() > 0)
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(display)) =
        default_chapter_language.c_str();
    if (default_chapter_country.length() > 0)
      *static_cast<EbmlString *>(&GetChild<KaxChapterCountry>(display)) =
        default_chapter_country.c_str();
  }
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
  KaxChapterDisplay *display;
  KaxChapterString *cstring;
  wxString label, language;
  bool first;
  wxTreeItemId old_id;

  enable_buttons(true);

  old_id = evt.GetOldItem();
  if (old_id.IsOk() && inputs_enabled)
    copy_values(old_id);

  t = (chapter_node_data_c *)tc_chapters->GetItemData(evt.GetItem());
  lb_chapter_names->Clear();

  if ((evt.GetItem() == tid_root) || (t == NULL) || !t->is_atom) {
    enable_inputs(false);
    no_update = true;
    tc_chapter_name->SetValue(wxT(""));
    tc_start_time->SetValue(wxT(""));
    tc_end_time->SetValue(wxT(""));
    no_update = false;
    return;
  }

  enable_inputs(true);

  display = FINDFIRST(t->chapter, KaxChapterDisplay);
  if (display == NULL)
    die("on_entry_selected: display == NULL. Should not have happened.\n");
  first = true;
  while (display != NULL) {
    cstring = FindChild<KaxChapterString>(*display);
    if (cstring != NULL)
      label =
        UTFstring_to_wxString(*static_cast<EbmlUnicodeString *>(cstring));
    else
      label = wxT("(unnamed)");

    if (first) {
      set_display_values(display);
      first = false;
    }
    lb_chapter_names->Append(label);
    display = FINDNEXT(t->chapter, KaxChapterDisplay, display);
  }

  set_timecode_values(t->chapter);

  lb_chapter_names->SetSelection(0);
  b_remove_chapter_name->Enable(lb_chapter_names->GetCount() > 1);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_language_code_selected(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  KaxChapterLanguage *clanguage;
  chapter_node_data_c *t;
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
  KaxChapterString *cstring;
  chapter_node_data_c *t;
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  cstring = &GetChild<KaxChapterString>(*cdisplay);
#if WXUNICODE
  *static_cast<EbmlUnicodeString *>(cstring) =
    cstrutf8_to_UTFstring(wxMB(tc_chapter_name->GetValue()));
#else
  *static_cast<EbmlUnicodeString *>(cstring) =
    cstr_to_UTFstring(tc_chapter_name->GetValue().c_str());
#endif

  label = create_chapter_label(*t->chapter);
  tc_chapters->SetItemText(id, label);
  lb_chapter_names->SetString(lb_chapter_names->GetSelection(),
                              tc_chapter_name->GetValue());
}

void
tab_chapters::on_set_default_values(wxCommandEvent &evt) {
  vector<string> parts;
  chapter_values_dlg dlg(this, true, wxU(default_chapter_language.c_str()),
                         wxU(default_chapter_country.c_str()));

  if (dlg.ShowModal() != wxID_OK)
    return;

  default_chapter_language =
    wxMB(extract_language_code(dlg.cob_language->GetValue()));
  default_chapter_country = wxMB(dlg.cob_country->GetValue());
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
        *static_cast<EbmlString *>(language) = wxMBL(s);
        display->PushElement(*language);
      } else {
        country = new KaxChapterCountry;
        *static_cast<EbmlString *>(country) = wxMBL(s);
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
  uint32_t i, n;
  chapter_values_dlg dlg(this, false);

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  if (dlg.ShowModal() != wxID_OK)
    return;

  if (dlg.cb_language->IsChecked()) {
    s = extract_language_code(dlg.cob_language->GetValue());
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
  uint32_t i, n, offset;
  int64_t adjustment, mult;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  sadjustment =
    wxGetTextFromUser(wxT("You can use this function for adjusting the "
                          "timecodes\nof the selected chapter and all its "
                          "children by a fixed amount.\nThe amount can be "
                          "positive or negative. The format used can be\n"
                          "either just a number in which case it is inter"
                          "preted as the number of seconds,\nor it can have "
                          "the usual HH:MM:SS.mmm or HH:MM:SS format.\n"
                          "Example: -00:05:23 would let all the chpaters "
                          "begin\n5minutes and 23seconds earlier than now."),
                      wxT("Adjust chapter timecodes"));
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
    wxMessageBox(wxT("Invalid format used for the adjustment."),
                 wxT("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
  vector<string> l_codes, c_codes;
  wxString s;
  uint32_t i;

  data = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((data == NULL) || !data->is_atom)
    return true;

  label = tc_chapters->GetItemText(id);
  chapter = data->chapter;

  s = tc_start_time->GetValue();
  strip(s);
  ts_start = parse_time(s);
  if (ts_start == -1) {
    wxMessageBox(wxT("Invalid format used for the start time for '") + label +
                 wxT("'. Setting value to 0."), wxT("Input data error"),
                 wxOK | wxCENTER | wxICON_ERROR);
    ts_start = 0;
  }

  s = tc_end_time->GetValue();
  strip(s);
  ts_end = parse_time(s);
  if (ts_end == -1) {
    wxMessageBox(wxT("Invalid format used for the end time for '") + label +
                 wxT("'. Setting value to 0."), wxT("Input data error"),
                 wxOK | wxCENTER | wxICON_ERROR);
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
    *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*chapter)) =
      ts_start;
  if (ts_end >= 0)
    *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeEnd>(*chapter)) =
      ts_end;

  label = create_chapter_label(*chapter);
  tc_chapters->SetItemText(id, label);

  return true;
}

void
tab_chapters::on_add_chapter_name(wxCommandEvent &evt) {
  KaxChapterDisplay *cdisplay;
  chapter_node_data_c *t;
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (cdisplay == NULL)
    return;

  cdisplay = new KaxChapterDisplay;
  *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(*cdisplay)) =
    cstr_to_UTFstring("(unnamed)");
  if (default_chapter_language.length() > 0)
    s = wxU(default_chapter_language.c_str());
  else
    s = wxT("eng");
  *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*cdisplay)) =
    wxMBL(s);
  if (default_chapter_country.length() > 0)
    *static_cast<EbmlString *>(&GetChild<KaxChapterCountry>(*cdisplay)) =
      default_chapter_country.c_str();
  // Workaround for a bug in libebml
  if (i == (t->chapter->ListSize() - 1))
    t->chapter->PushElement(*cdisplay);
  else
    t->chapter->InsertElement(*cdisplay, i + 1);
  s = wxT("(unnamed)");
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
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
  uint32_t i, n;
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
      if (n == (lb_chapter_names->GetSelection() + 1)) {
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
    tc_start_time->SetValue(wxT(""));

  tend = FINDFIRST(atom, KaxChapterTimeEnd);
  if (tend != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tend));
    label.Printf(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
    tc_end_time->SetValue(label);
  } else
    tc_end_time->SetValue(wxT(""));

  no_update = false;
}

void
tab_chapters::set_display_values(KaxChapterDisplay *display) {
  KaxChapterString *cstring;
  KaxChapterLanguage *clanguage;
  KaxChapterCountry *ccountry;
  wxString language;
  uint32_t i;

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
    language = wxU(string(*static_cast<EbmlString *>(clanguage)).c_str());
  else
    language = wxT("eng");
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    if (extract_language_code(sorted_iso_codes[i]) == language) {
      language = sorted_iso_codes[i];
      break;
    }
  cob_language_code->SetValue(language);

  ccountry = FINDFIRST(display, KaxChapterCountry);
  if (ccountry != NULL)
    cob_country_code->SetValue(wxU(string(*static_cast<EbmlString *>
                                          (ccountry)).c_str()));
  else
    cob_country_code->SetValue(wxT(""));

  no_update = false;
}

int64_t
tab_chapters::parse_time(wxString s) {
  int64_t nsecs;
  string utf8s;
  const char *c;

  utf8s = wxMB(s);
  strip(utf8s);
  if (utf8s.length() == 0)
    return -2;

  c = utf8s.c_str();
  while (*c != 0) {
    if (!isdigit(*c)) {
      if (parse_timecode(utf8s.c_str(), &nsecs))
        return nsecs;
      return -1;
    }
    c++;
  }
  if (!parse_int(utf8s.c_str(), nsecs))
    return -1;
  return nsecs * 1000000000;
}

IMPLEMENT_CLASS(chapter_values_dlg, wxDialog);
BEGIN_EVENT_TABLE(chapter_values_dlg, wxDialog)
  EVT_CHECKBOX(ID_CVD_CB_LANGUAGE, chapter_values_dlg::on_language_clicked)
  EVT_CHECKBOX(ID_CVD_CB_COUNTRY, chapter_values_dlg::on_country_clicked)
END_EVENT_TABLE();

IMPLEMENT_CLASS(tab_chapters, wxPanel);
BEGIN_EVENT_TABLE(tab_chapters, wxPanel)
  EVT_BUTTON(ID_B_ADDCHAPTER, tab_chapters::on_add_chapter)
  EVT_BUTTON(ID_B_ADDSUBCHAPTER, tab_chapters::on_add_subchapter)
  EVT_BUTTON(ID_B_REMOVECHAPTER, tab_chapters::on_remove_chapter)
  EVT_BUTTON(ID_B_SETVALUES, tab_chapters::on_set_values)
  EVT_BUTTON(ID_B_ADJUSTTIMECODES, tab_chapters::on_adjust_timecodes)
  EVT_BUTTON(ID_B_ADD_CHAPTERNAME, tab_chapters::on_add_chapter_name)
  EVT_BUTTON(ID_B_REMOVE_CHAPTERNAME, tab_chapters::on_remove_chapter_name)
  EVT_TREE_SEL_CHANGED(ID_TRC_CHAPTERS, tab_chapters::on_entry_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTLANGUAGECODE,
               tab_chapters::on_language_code_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTCOUNTRYCODE,
               tab_chapters::on_country_code_selected)
  EVT_LISTBOX(ID_LB_CHAPTERNAMES, tab_chapters::on_chapter_name_selected)
  EVT_TEXT(ID_TC_CHAPTERNAME, tab_chapters::on_chapter_name_changed)
END_EVENT_TABLE();
