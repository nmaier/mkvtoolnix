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
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include <ebml/EbmlStream.h>

#include "mmg.h"
#include "chapters.h"
#include "common.h"
#include "error.h"
#include "extern_data.h"
#include "iso639.h"
#include "kax_analyzer.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

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
  wxTextCtrl *tc_language, *tc_country;
  wxCheckBox *cb_language, *cb_country;

public:
  chapter_values_dlg(wxWindow *parent, bool set_defaults,
                     wxString old_def_language = "",
                     wxString old_def_country = "");
  void on_language_clicked(wxCommandEvent &evt);
  void on_country_clicked(wxCommandEvent &evt);
};

chapter_values_dlg::chapter_values_dlg(wxWindow *parent, bool set_defaults,
                                       wxString old_def_language,
                                       wxString old_def_country):
  wxDialog(parent, 0, "", wxDefaultPosition) {
  wxButton *b_ok, *b_cancel;
#if defined(SYS_WINDOWS)
#define CVD_YOFF1 (-23)
  wxDialog *panel = this;
#else
#define CVD_YOFF1 0
  wxPanel *panel = new wxPanel(this, -1);
#endif

  SetSize(350, 200);
  if (set_defaults) {
    SetTitle("Change the default values");
    new wxStaticText(panel, wxID_STATIC,
                     "Here you can set the default values that mmg will use\n"
                     "for each chapter that you create. These values can\n"
                     "then be changed if needed. The default values will be\n"
                     "saved when you exit mmg.",
                     wxPoint(10, 10), wxSize(380, 100), 0);

    new wxStaticText(panel, wxID_STATIC, "Language:",
                     wxPoint(10, 92 + CVD_YOFF1));
    tc_language =
      new wxTextCtrl(panel, wxID_STATIC, old_def_language,
                     wxPoint(90, 90 + CVD_YOFF1), wxSize(220, -1));
    new wxStaticText(panel, wxID_STATIC, "Country:",
                     wxPoint(10, 127 + CVD_YOFF1));
    tc_country =
      new wxTextCtrl(panel, wxID_STATIC, old_def_country,
                     wxPoint(90, 125 + CVD_YOFF1), wxSize(220, -1));

  } else {
    SetTitle("Select values to be applied");
    new wxStaticText(panel, wxID_STATIC,
                     "Please enter the values for the language and the\n"
                     "country that you want to apply to all the chapters\n"
                     "below and including the currently selected entry.",
                     wxPoint(10, 10), wxSize(380, 100), 0);

    cb_language =
      new wxCheckBox(panel, ID_CVD_CB_LANGUAGE, "Set language to:",
                     wxPoint(10, 92 + CVD_YOFF1));
    cb_language->SetValue(false);
    tc_language =
      new wxTextCtrl(panel, wxID_STATIC, old_def_language,
                     wxPoint(135, 90 + CVD_YOFF1), wxSize(175, -1));
    tc_language->Enable(false);
    cb_country =
      new wxCheckBox(panel, ID_CVD_CB_COUNTRY, "Set country to:",
                     wxPoint(10, 127 + CVD_YOFF1));
    cb_country->SetValue(false);
    tc_country =
      new wxTextCtrl(panel, wxID_STATIC, old_def_country,
                     wxPoint(135, 125 + CVD_YOFF1), wxSize(175, -1));
    tc_country->Enable(false);

  }

  b_ok = new wxButton(panel, wxID_OK, "Ok", wxPoint(0, 0));
  b_ok->SetDefault();
  b_cancel = new wxButton(panel, wxID_CANCEL, "Cancel", wxPoint(0, 0));
  b_ok->SetSize(b_cancel->GetSize());
  b_ok->Move(wxPoint(GetSize().GetWidth() / 2 - b_ok->GetSize().GetWidth() -
                     b_cancel->GetSize().GetWidth() / 2,
                     GetSize().GetHeight() -
                     b_ok->GetSize().GetHeight() * 3 / 2 + CVD_YOFF1));
  b_cancel->Move(wxPoint(GetSize().GetWidth() / 2 +
                         b_cancel->GetSize().GetWidth() / 2,
                         GetSize().GetHeight() -
                         b_ok->GetSize().GetHeight() * 3 / 2 + CVD_YOFF1));
}

void chapter_values_dlg::on_language_clicked(wxCommandEvent &evt) {
  tc_language->Enable(cb_language->IsChecked());
}

void chapter_values_dlg::on_country_clicked(wxCommandEvent &evt) {
  tc_country->Enable(cb_country->IsChecked());
}

void expand_subtree(wxTreeCtrl &tree, wxTreeItemId &root, bool expand = true) {
  wxTreeItemId child;
  long cookie;

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
#define X1 140
#define X2 370
#define XS 360

tab_chapters::tab_chapters(wxWindow *parent, wxMenu *nm_chapters):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL) {
  uint32_t i;

  m_chapters = nm_chapters;

  new wxStaticText(this, wxID_STATIC, _("Chapters:"), wxPoint(10, 5),
                   wxDefaultSize, 0);
#ifdef SYS_WINDOWS
  tc_chapters =
    new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxPoint(10, 24), wxSize(350, 250));
#else
  tc_chapters =
    new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxPoint(10, 24), wxSize(350, 250),
                   wxSUNKEN_BORDER | wxTR_HAS_BUTTONS);
#endif

  b_add_chapter =
    new wxButton(this, ID_B_ADDCHAPTER, _("Add chapter"), wxPoint(370, 24),
                 wxSize(120, -1));

  b_add_subchapter =
    new wxButton(this, ID_B_ADDSUBCHAPTER, _("Add subchapter"),
                 wxPoint(370, 50), wxSize(120, -1));

  b_remove_chapter =
    new wxButton(this, ID_B_REMOVECHAPTER, _("Remove chapter"),
                 wxPoint(370, 76), wxSize(120, -1));

  b_set_values =
    new wxButton(this, ID_B_SETVALUES, _("Set values"), wxPoint(370, 128),
                 wxSize(120, -1));
  b_set_values->SetToolTip("Here you can set the values for the language and "
                           "the country that you want to apply to all the "
                           "chapters below and including the currently "
                           "selected entry.");

  new wxStaticText(this, wxID_STATIC, _("Chapter name:"), wxPoint(10, 285));
  tc_chapter_name =
    new wxTextCtrl(this, ID_TC_CHAPTERNAME, _(""), wxPoint(X1, 285 + YOFF),
                   wxSize(XS - X1, -1));

  new wxStaticText(this, wxID_STATIC, _("Start time:"),
                   wxPoint(10, 285 + Y1));
  tc_start_time =
    new wxTextCtrl(this, ID_TC_CHAPTERSTART, _(""),
                   wxPoint(X1, 285 + Y1 + YOFF), wxSize(XS - X1, -1));

  new wxStaticText(this, wxID_STATIC, _("End time:"),
                   wxPoint(10, 285 + 2 * Y1));
  tc_end_time =
    new wxTextCtrl(this, ID_TC_CHAPTEREND, _(""),
                   wxPoint(X1, 285 + 2 * Y1 + YOFF), wxSize(XS - X1, -1));

  new wxStaticText(this, wxID_STATIC, _("Language codes:"),
                   wxPoint(10, 285 + 3 * Y1));
  tc_language_codes =
    new wxTextCtrl(this, ID_TC_CHAPTERLANGUAGES, _(""),
                   wxPoint(X1, 285 + 3 * Y1 + YOFF), wxSize(XS - X1, -1));
  cob_add_language_code =
    new wxComboBox(this, ID_CB_CHAPTERSELECTLANGUAGECODE, _(""),
                   wxPoint(X2, 285 + 3 * Y1 + YOFF), wxSize(120, -1),
                   0, NULL);
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_add_language_code->Append(sorted_iso_codes[i]);
  cob_add_language_code->SetValue(_(""));

  new wxStaticText(this, wxID_STATIC, _("Country codes:"),
                   wxPoint(10, 285  + 4 * Y1));
  tc_country_codes =
    new wxTextCtrl(this, ID_TC_CHAPTERCOUNTRYCODES, _(""),
                   wxPoint(X1, 285 + 4 * Y1 + YOFF), wxSize(XS - X1, -1));
  cob_add_country_code =
    new wxComboBox(this, ID_CB_CHAPTERSELECTCOUNTRYCODE, _(""),
                   wxPoint(X2, 285 + 4 * Y1 + YOFF), wxSize(120, -1),
                   0, NULL);
  for (i = 0; cctlds[i] != NULL; i++)
    cob_add_country_code->Append(_(cctlds[i]));
  cob_add_country_code->SetValue(_(""));

  enable_inputs(false);

  m_chapters->Enable(ID_M_CHAPTERS_SAVE, false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVEAS, false);
  m_chapters->Enable(ID_M_CHAPTERS_SAVETOKAX, false);
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, false);
  enable_buttons(false);

  file_name = "";
  chapters = NULL;
  analyzer = NULL;
  source_is_kax_file = false;
  source_is_simple_format = false;
}

tab_chapters::~tab_chapters() {
  if (chapters != NULL)
    delete chapters;
  if (analyzer != NULL)
    delete analyzer;
}

void tab_chapters::enable_inputs(bool enable) {
  tc_chapter_name->Enable(enable);
  tc_start_time->Enable(enable);
  tc_end_time->Enable(enable);
  tc_language_codes->Enable(enable);
  tc_country_codes->Enable(enable);
  cob_add_language_code->Enable(enable);
  cob_add_country_code->Enable(enable);
  inputs_enabled = enable;
}

void tab_chapters::enable_buttons(bool enable) {
  b_add_chapter->Enable(enable);
  b_add_subchapter->Enable(enable);
  b_remove_chapter->Enable(enable);
  b_set_values->Enable(enable);
}

void tab_chapters::on_new_chapters(wxCommandEvent &evt) {
  file_name = "";
  if (chapters != NULL)
    delete chapters;
  tc_chapters->DeleteAllItems();
  tid_root = tc_chapters->AddRoot(_T("(new chapter file)"));
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

  mdlg->set_status_bar("New chapters created.");
}

wxString tab_chapters::create_chapter_label(KaxChapterAtom &chapter) {
  wxString label, s;
  char *tmpstr;
  KaxChapterDisplay *display;
  KaxChapterString *cstring;
  KaxChapterTimeStart *tstart;
  KaxChapterTimeEnd *tend;
  KaxChapterLanguage *language;
  int64_t timestamp;

  label = "(unnamed chapter)";
  language = NULL;
  display = FindChild<KaxChapterDisplay>(chapter);
  if (display != NULL) {
    cstring = FindChild<KaxChapterString>(*display);
    if (cstring != NULL) {
      tmpstr = UTFstring_to_cstr(*static_cast<EbmlUnicodeString *>(cstring));
      label = tmpstr;
      safefree(tmpstr);
    }
    language = FindChild<KaxChapterLanguage>(*display);
  }
  label += " [";
  tstart = FindChild<KaxChapterTimeStart>(chapter);
  if (tstart != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tstart)) / 1000000;
    s.Printf("%02d:%02d:%02d.%03d",
             (int)(timestamp / 1000 / 60 / 60),
             (int)((timestamp / 1000 / 60) % 60),
             (int)((timestamp / 1000) % 60),
             (int)(timestamp % 1000));
    label += s;

    tend = FindChild<KaxChapterTimeEnd>(chapter);
    if (tend != NULL) {
      timestamp = uint64(*static_cast<EbmlUInteger *>(tend)) / 1000000;
      s.Printf("%02d:%02d:%02d.%03d",
               (int)(timestamp / 1000 / 60 / 60),
               (int)((timestamp / 1000 / 60) % 60),
               (int)((timestamp / 1000) % 60),
               (int)(timestamp % 1000));
      label += " - " + s;
    }

    label += "; ";
  }
  if (language != NULL)
    label += string(*static_cast<EbmlString *>(language)).c_str();
  else
    label += "eng";

  label += "]";

  return label;
}

void tab_chapters::add_recursively(wxTreeItemId &parent, EbmlMaster &master) {
  uint32_t i;
  wxString s;
  EbmlElement *e;
  KaxChapterAtom *chapter;
  KaxEditionEntry *eentry;
  wxTreeItemId this_item;

  for (i = 0; i < master.ListSize(); i++) {
    e = master[i];
    if (EbmlId(*e) == KaxEditionEntry::ClassInfos.GlobalId) {
      s.Printf("Edition %d", tc_chapters->GetChildrenCount(tid_root, false) +
               1);
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

void tab_chapters::fix_missing_languages(EbmlMaster &master) {
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

void tab_chapters::on_load_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose a chapter file", last_open_dir, "",
                   _T("Chapter files (*.xml;*.txt;*.mka;*.mkv;*.cue)|*.xml;"
                      "*.txt;*.mka;*.mkv;*.cue|" ALLFILES), wxOPEN);

  if (dlg.ShowModal() == wxID_OK)
    if (load(dlg.GetPath()))
      last_open_dir = dlg.GetDirectory();
}

bool tab_chapters::load(wxString name) {
  KaxChapters *new_chapters;
  EbmlElement *e;
  wxString s;
  int pos;

  try {
    if (kax_analyzer_c::probe(name.c_str())) {
      if (analyzer != NULL)
        delete analyzer;
      analyzer = new kax_analyzer_c(this, name.c_str());
      file_name = name;
      if (!analyzer->process()) {
        delete analyzer;
        analyzer = NULL;
        return false;
      }
      pos = analyzer->find(KaxChapters::ClassInfos.GlobalId);
      if (pos == -1) {
        wxMessageBox(_("This file does not contain any chapters."),
                     _("No chapters found"), wxOK | wxCENTER |
                     wxICON_INFORMATION);
        delete analyzer;
        analyzer = NULL;
        return false;
      }

      e = analyzer->read_element(pos, KaxChapters::ClassInfos);
      if (e == NULL)
        throw error_c(_("This file does not contain valid chapters."));
      new_chapters = static_cast<KaxChapters *>(e);
      source_is_kax_file = true;
      source_is_simple_format = false;

    } else {
      new_chapters = parse_chapters(name.c_str(), 0, -1, 0, NULL, NULL, true,
                                    &source_is_simple_format);
      source_is_kax_file = false;
    }
  } catch (error_c ex) {
    analyzer = NULL;
    s = (const char *)ex;
    break_line(s);
    while (s[s.Length() - 1] == '\n')
      s.Remove(s.Length() - 1);
    wxMessageBox(s, _T("Error parsing the file"),
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
  mdlg->set_status_bar("Chapters loaded.");

  return true;
}

void tab_chapters::on_save_chapters(wxCommandEvent &evt) {
  if (!verify())
    return;

  if (source_is_kax_file) {
    if (analyzer->update_element(chapters))
      mdlg->set_status_bar(_("Chapters written."));
    return;
  }

  if ((file_name.length() == 0) || source_is_simple_format)
    if (!select_file_name())
      return;
  save();
}

void tab_chapters::on_save_chapters_to_kax_file(wxCommandEvent &evt) {
  if (!verify())
    return;

  wxFileDialog dlg(this, _("Choose an output file"), last_open_dir, "",
                   _T("Matroska files (*.mkv;*.mka)|*.mkv;*.mka|"
                      ALLFILES), wxSAVE);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (!kax_analyzer_c::probe(dlg.GetPath().c_str())) {
    wxMessageBox(_("The file you tried to save to is NOT a Matroska file."),
                 _("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  last_open_dir = dlg.GetDirectory();
  file_name = dlg.GetPath();
  tc_chapters->SetItemText(tid_root, file_name);

  source_is_kax_file = true;
  source_is_simple_format = false;

  if (analyzer != NULL)
    delete analyzer;
  analyzer = new kax_analyzer_c(this, file_name.c_str());
  if (!analyzer->process()) {
    delete analyzer;
    analyzer = NULL;
    return;
  }
  if (analyzer->update_element(chapters))
    mdlg->set_status_bar(_("Chapters written."));
  mdlg->set_last_chapters_in_menu(file_name);
}

void tab_chapters::on_save_chapters_as(wxCommandEvent &evt) {
  if (!verify())
    return;

  if (!select_file_name())
    return;
  save();
}

bool tab_chapters::select_file_name() {
  wxFileDialog dlg(this, _("Choose an output file"), last_open_dir, "",
                   _T("Chapter files (*.xml)|*.xml|"
                      ALLFILES), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    if (kax_analyzer_c::probe(dlg.GetPath().c_str())) {
      wxMessageBox(_("The file you tried to save to is a Matroska file. For "
                     "this to work you have to use the 'Save to Matroska file'"
                     " menu option."), _("Wrong file selected"),
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

void tab_chapters::save() {
  FILE *fout;
  wxString err;

  fout = fopen(file_name.c_str(), "w");
  if (fout == NULL) {
    err.Printf(_("Could not open the destination file '%s' for writing. "
                 "Error code: %d (%s)."), file_name.c_str(), errno,
               strerror(errno));
    wxMessageBox(err, _("Error opening file"), wxCENTER | wxOK | wxICON_ERROR);
    return;
  }

  fprintf(fout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<Chapters>\n");
  write_chapters_xml(chapters, fout);
  fprintf(fout, "</Chapters>\n");
  fclose(fout);

  source_is_kax_file = false;
  source_is_simple_format = false;

  mdlg->set_last_chapters_in_menu(file_name);
  mdlg->set_status_bar(_("Chapters written."));
}

void tab_chapters::on_verify_chapters(wxCommandEvent &evt) {
  verify();
}

bool tab_chapters::verify_atom_recursively(EbmlElement *e, int64_t p_start,
                                           int64_t p_end) {
  KaxChapterUID *uid;
  KaxChapterAtom *chapter;
  KaxChapterTimeStart *start;
  KaxChapterTimeEnd *end;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterString *cs;
  wxString label;
  char *utf;
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
    wxMessageBox(_("One of the chapters does not have a name."),
                 _("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }
  utf = UTFstring_to_cstr(UTFstring(*static_cast<EbmlUnicodeString *>(cs)));
  label = utf;
  safefree(utf);

  start = FindChild<KaxChapterTimeStart>(*chapter);
  if (start == NULL) {
    wxMessageBox(_("The chapter '" + label + "' is missing the start time."),
                 _("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }
  t_start = uint64(*static_cast<EbmlUInteger *>(start));
  if ((p_start != -1) && (t_start < p_start)) {
    wxMessageBox(_("The chapter '" + label + "' starts before its parent."),
                 _("Chapter verification error"), wxCENTER | wxOK |
                 wxICON_ERROR);
    return false;
  }

  end = FindChild<KaxChapterTimeEnd>(*chapter);
  if (end != NULL) {
    t_end = uint64(*static_cast<EbmlUInteger *>(end));
    if ((p_end != -1) && (t_end > p_end)) {
      wxMessageBox(_("The chapter '" + label + "' ends after its parent."),
                   _("Chapter verification error"), wxCENTER | wxOK |
                   wxICON_ERROR);
      return false;
    }
    if (t_end < t_start) {
      wxMessageBox(_("The chapter '" + label + "' ends before it starts."),
                   _("Chapter verification error"), wxCENTER | wxOK |
                   wxICON_ERROR);
      return false;
    }
  } else
    t_end = -1;

  language = FindChild<KaxChapterLanguage>(*display);
  if (language == NULL) {
    wxMessageBox(_("The chapter '" + label + "' is missing its language."),
                 _("Chapter verification error"), wxCENTER | wxOK |
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

void tab_chapters::on_add_chapter(wxCommandEvent &evt) {
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
    s.Printf("EditionEntry %u", chapters->ListSize());
    id = tc_chapters->AppendItem(tid_root, s, -1, -1, d);
  }

  chapter = new KaxChapterAtom;
  while (chapter->ListSize() > 0) {
    delete (*chapter)[0];
    chapter->Remove(0);
  }
  if ((default_chapter_language.length() > 0) ||
      (default_chapter_country.length() > 0)) {
    KaxChapterDisplay &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
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
      mxerror("start >= m->ListSize(). This should not have happened. Please "
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

void tab_chapters::on_add_subchapter(wxCommandEvent &evt) {
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
    s.Printf("EditionEntry %u", chapters->ListSize());
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
  if ((default_chapter_language.length() > 0) ||
      (default_chapter_country.length() > 0)) {
    KaxChapterDisplay &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
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

void tab_chapters::on_remove_chapter(wxCommandEvent &evt) {
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

void tab_chapters::on_entry_selected(wxTreeEvent &evt) {
  chapter_node_data_c *t;
  KaxChapterDisplay *display;
  KaxChapterString *cstring;
  KaxChapterTimeStart *tstart;
  KaxChapterTimeEnd *tend;
  EbmlElement *e;
  wxString label, languages, countries;
  int64_t timestamp;
  uint32_t i;
  char *tmpstr;
  wxTreeItemId old_id;

  enable_buttons(true);

  old_id = evt.GetOldItem();
  if (old_id.IsOk() && inputs_enabled)
    copy_values(old_id);

  t = (chapter_node_data_c *)tc_chapters->GetItemData(evt.GetItem());

  if ((evt.GetItem() == tid_root) || (t == NULL) || !t->is_atom) {
    tc_chapter_name->SetValue("");
    tc_start_time->SetValue("");
    tc_end_time->SetValue("");
    tc_language_codes->SetValue("");
    tc_country_codes->SetValue("");
    enable_inputs(false);
    return;
  }

  enable_inputs(true);

  label = "(unnamed chapter)";
  languages = "";
  countries = "";
  display = FindChild<KaxChapterDisplay>(*t->chapter);
  if (display != NULL) {
    cstring = FindChild<KaxChapterString>(*display);
    if (cstring != NULL) {
      tmpstr = UTFstring_to_cstr(*static_cast<EbmlUnicodeString *>(cstring));
      label = tmpstr;
      safefree(tmpstr);
    }
    for (i = 0; i < display->ListSize(); i++) {
      e = (*display)[i];
      if (EbmlId(*e) == KaxChapterLanguage::ClassInfos.GlobalId) {
        if (languages.length() > 0)
          languages += " ";
        languages += string(*static_cast<EbmlString *>(e)).c_str();
      } else if (EbmlId(*e) == KaxChapterCountry::ClassInfos.GlobalId) {
        if (countries.length() > 0)
          countries += " ";
        countries += string(*static_cast<EbmlString *>(e)).c_str();
      }
    }
  }
  tc_chapter_name->SetValue(label);
  tc_language_codes->SetValue(languages);
  tc_country_codes->SetValue(countries);

  tstart = FindChild<KaxChapterTimeStart>(*t->chapter);
  if (tstart != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tstart)) / 1000000;
    label.Printf("%02d:%02d:%02d.%03d",
                 (int)(timestamp / 1000 / 60 / 60),
                 (int)((timestamp / 1000 / 60) % 60),
                 (int)((timestamp / 1000) % 60),
                 (int)(timestamp % 1000));
    tc_start_time->SetValue(label);
  } else
    tc_start_time->SetValue("");

  tend = FindChild<KaxChapterTimeEnd>(*t->chapter);
  if (tend != NULL) {
    timestamp = uint64(*static_cast<EbmlUInteger *>(tend)) / 1000000;
    label.Printf("%02d:%02d:%02d.%03d",
                 (int)(timestamp / 1000 / 60 / 60),
                 (int)((timestamp / 1000 / 60) % 60),
                 (int)((timestamp / 1000) % 60),
                 (int)(timestamp % 1000));
    tc_end_time->SetValue(label);
  } else
    tc_end_time->SetValue("");

  tc_chapter_name->SetFocus();
}

void tab_chapters::on_language_code_selected(wxCommandEvent &evt) {
  wxString code;
  if (tc_language_codes->GetValue().length() > 0)
    tc_language_codes->AppendText(" ");
  code = extract_language_code(cob_add_language_code->GetStringSelection());
  tc_language_codes->AppendText(code);
}

void tab_chapters::on_country_code_selected(wxCommandEvent &evt) {
  if (tc_country_codes->GetValue().length() > 0)
    tc_country_codes->AppendText(" ");
  tc_country_codes->AppendText(cob_add_country_code->GetStringSelection());
}

void tab_chapters::verify_language_codes(string s, vector<string> &parts) {
  uint32_t i;
  vector<string>::iterator eit;

  strip(s);
  parts = split(s.c_str(), " ");
  i = 0;
  while (i < parts.size())
    if (!is_valid_iso639_2_code(parts[i].c_str())) {
      wxMessageBox(wxString("'") + parts[i].c_str() +
                   wxString("' is not a valid ISO639-2 language code. "
                            "Removing this entry."),
                   _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      eit = parts.begin();
      eit += i;
      parts.erase(eit);
    } else
      i++;
}

void tab_chapters::verify_country_codes(string s, vector<string> &parts) {
  uint32_t i;
  vector<string>::iterator eit;

  strip(s);
  parts = split(s.c_str(), " ");
  i = 0;
  while (i < parts.size())
    if ((parts[i].length() != 2) ||
        (parts[i][0] < 'a') || (parts[i][0] > 'z') ||
        (parts[i][1] < 'a') || (parts[i][1] > 'z')) {
      wxMessageBox(wxString("'") + parts[i].c_str() +
                   wxString("' is not a valid two-letter country "
                            "code. Removing this entry."),
                   _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      eit = parts.begin();
      eit += i;
      parts.erase(eit);
    } else
      i++;
}

void tab_chapters::on_set_default_values(wxCommandEvent &evt) {
  vector<string> parts;
  chapter_values_dlg dlg(this, true, default_chapter_language.c_str(),
                         default_chapter_country.c_str());

  if (dlg.ShowModal() != wxID_OK)
    return;

  verify_language_codes(dlg.tc_language->GetValue().c_str(), parts);
  default_chapter_language = join(" ", parts);

  verify_country_codes(dlg.tc_country->GetValue().c_str(), parts);
  default_chapter_country = join(" ", parts);
}

void tab_chapters::set_values_recursively(wxTreeItemId id, string &s,
                                          bool set_language) {
  uint32_t i;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterCountry *country;
  chapter_node_data_c *d;
  wxTreeItemId next_child;
  wxString text;
  vector<string> codes;
  long cookie;

  if (!id.IsOk())
    return;

  d = (chapter_node_data_c *)tc_chapters->GetItemData(id);
  if ((d != NULL) && (d->is_atom)) {
    codes = split(s.c_str(), " ");
    if ((display = FindChild<KaxChapterDisplay>(*d->chapter)) == NULL)
      display = &GetEmptyChild<KaxChapterDisplay>(*d->chapter);
    i = 0;
    while (i < display->ListSize()) {
      if (set_language &&
          (dynamic_cast<KaxChapterLanguage *>((*display)[i]) != NULL)) {
        delete (*display)[i];
        display->Remove(i);
      } else if (!set_language &&
                 (dynamic_cast<KaxChapterCountry *>((*display)[i]) != NULL)) {
        delete (*display)[i];
        display->Remove(i);
      } else
        i++;
    }
    for (i = 0; i < codes.size(); i++) {
      if (set_language) {
        language = new KaxChapterLanguage;
        *static_cast<EbmlString *>(language) = codes[i].c_str();
        display->PushElement(*language);
      } else {
        country = new KaxChapterCountry;
        *static_cast<EbmlString *>(country) = codes[i].c_str();
        display->PushElement(*country);
      }
    }
    text = create_chapter_label(*d->chapter);
    tc_chapters->SetItemText(id, text);
    if (id == tc_chapters->GetSelection()) {
      if (set_language)
        tc_language_codes->SetValue(s.c_str());
      else
        tc_country_codes->SetValue(s.c_str());
    }
  }

  next_child = tc_chapters->GetFirstChild(id, cookie);
  while (next_child.IsOk()) {
    set_values_recursively(next_child, s, set_language);
    next_child = tc_chapters->GetNextChild(id, cookie);
  }
}

void tab_chapters::on_set_values(wxCommandEvent &evt) {
  wxTreeItemId id;
  vector<string> parts;
  string s;
  chapter_values_dlg dlg(this, false);

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  if (dlg.ShowModal() != wxID_OK)
    return;

  if (dlg.cb_language->IsChecked()) {
    s = dlg.tc_language->GetValue().c_str();
    verify_language_codes(s, parts);
    s = join(" ", parts);
    set_values_recursively(id, s, true);
  }
  if (dlg.cb_country->IsChecked()) {
    s = dlg.tc_country->GetValue().c_str();
    verify_country_codes(s, parts);
    s = join(" ", parts);
    set_values_recursively(id, s, false);
  }
}

bool tab_chapters::copy_values(wxTreeItemId id) {
  chapter_node_data_c *data;
  wxString label;
  KaxChapterAtom *chapter;
  KaxChapterDisplay *display;
  KaxChapterLanguage *language;
  KaxChapterCountry *country;
  EbmlElement *e;
  int64_t ts_start, ts_end;
  vector<string> l_codes, c_codes;
  string s;
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
    wxMessageBox(_("Invalid format used for the start time for '" + label +
                   "'. Setting value to 0."),
                 _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    ts_start = 0;
  }

  s = tc_end_time->GetValue();
  strip(s);
  ts_end = parse_time(s);
  if (ts_end == -1) {
    wxMessageBox(_("Invalid format used for the end time for '" + label +
                   "'. Setting value to 0."),
                 _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    ts_end = 0;
  }

  verify_language_codes(tc_language_codes->GetValue().c_str(), l_codes);
  verify_country_codes(tc_country_codes->GetValue().c_str(), c_codes);

  display = &GetChild<KaxChapterDisplay>(*chapter);
  i = 0;
  while (i < display->ListSize()) {
    e = (*display)[i];
    if ((EbmlId(*e) == KaxChapterLanguage::ClassInfos.GlobalId) ||
        (EbmlId(*e) == KaxChapterCountry::ClassInfos.GlobalId)) {
      display->Remove(i);
      delete e;
    } else
      i++;
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

  *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(*display)) =
    cstr_to_UTFstring(tc_chapter_name->GetValue().c_str());
  if (ts_start >= 0)
    *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*chapter)) =
      ts_start;
  if (ts_end >= 0)
    *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeEnd>(*chapter)) =
      ts_end;

  for (i = 0; i < l_codes.size(); i++) {
    language = new KaxChapterLanguage;
    *static_cast<EbmlString *>(language) = l_codes[i];
    display->PushElement(*language);
  }

  for (i = 0; i < c_codes.size(); i++) {
    country = new KaxChapterCountry;
    *static_cast<EbmlString *>(country) = c_codes[i];
    display->PushElement(*country);
  }

  label = create_chapter_label(*chapter);
  tc_chapters->SetItemText(id, label);

  return true;
}

#define iscolon(s) (*(s) == ':')
#define isdot(s) (*(s) == '.')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (istwodigits(s) && isdigit(*(s + 2)))
#define istimestamp(s) ((strlen(s) == 12) && \
                        istwodigits(s) && iscolon(s + 2) && \
                        istwodigits(s + 3) && iscolon(s + 5) && \
                        istwodigits(s + 6) && isdot(s + 8) && \
                        isthreedigits(s + 9))
#define isshorttimestamp(s) ((strlen(s) == 8) && \
                             istwodigits(s) && iscolon(s + 2) && \
                             istwodigits(s + 3) && iscolon(s + 5) && \
                             istwodigits(s + 6))

int64_t tab_chapters::parse_time(string s) {
  const char *c;
  int hour, minutes, seconds, msecs;

  strip(s);
  if (s.length() == 0)
    return -2;

  c = s.c_str();

  if (istimestamp(c)) {
    sscanf(c, "%02d:%02d:%02d.%03d", &hour, &minutes, &seconds, &msecs);
    return ((int64_t)hour * 60 * 60 * 1000 +
            (int64_t)minutes * 60 * 1000 +
            (int64_t)seconds * 1000 + msecs) * 1000000;

  } else if (isshorttimestamp(c)) {
    sscanf(c, "%02d:%02d:%02d", &hour, &minutes, &seconds);
    return ((int64_t)hour * 60 * 60 * 1000 +
            (int64_t)minutes * 60 * 1000 +
            (int64_t)seconds * 1000) * 1000000;

  }

  return -1;
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
  EVT_TREE_SEL_CHANGED(ID_TRC_CHAPTERS, tab_chapters::on_entry_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTLANGUAGECODE,
               tab_chapters::on_language_code_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTCOUNTRYCODE,
               tab_chapters::on_country_code_selected)
END_EVENT_TABLE();
