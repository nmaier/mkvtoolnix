/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_chapters.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

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

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include "mmg.h"
#include "chapters.h"
#include "common.h"
#include "error.h"
#include "extern_data.h"
#include "iso639.h"

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
  tc_chapters =
    new wxTreeCtrl(this, ID_TRC_CHAPTERS, wxPoint(10, 24), wxSize(350, 250),
                   wxSIMPLE_BORDER | wxTR_HAS_BUTTONS);

  b_add_chapter =
    new wxButton(this, ID_B_ADDCHAPTER, _("Add chapter"), wxPoint(370, 24),
                 wxSize(120, -1));
  b_remove_chapter =
    new wxButton(this, ID_B_REMOVECHAPTER, _("Remove chapter"),
                 wxPoint(370, 50), wxSize(120, -1));

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
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, false);
  b_add_chapter->Enable(false);
  b_remove_chapter->Enable(false);

  file_name = "";
  chapters = NULL;

  value_copy_timer.SetOwner(this, ID_T_CHAPTERVALUES);
  value_copy_timer.Start(333);
}

tab_chapters::~tab_chapters() {
  if (chapters != NULL)
    delete chapters;
}

void tab_chapters::enable_inputs(bool enable) {
  tc_chapter_name->Enable(enable);
  tc_start_time->Enable(enable);
  tc_end_time->Enable(enable);
  tc_language_codes->Enable(enable);
  tc_country_codes->Enable(enable);
  cob_add_language_code->Enable(enable);
  cob_add_country_code->Enable(enable);
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
  m_chapters->Enable(ID_M_CHAPTERS_VERIFY, true);
  b_add_chapter->Enable(true);
  b_remove_chapter->Enable(true);

  enable_inputs(false);

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

void tab_chapters::on_load_chapters(wxCommandEvent &evt) {
  KaxChapters *new_chapters;
  wxString s;
  wxFileDialog dlg(NULL, "Choose a chapter file", last_open_dir, "",
                   _T("Chapter files (*.xml;*.txt)|*.xml;*.txt|" ALLFILES),
                   wxOPEN);

  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    try {
      new_chapters = parse_chapters(dlg.GetPath().c_str(), 0, -1, 0, NULL,
                                    NULL, true);
    } catch (error_c e) {
      s = (const char *)e;
      break_line(s);
      while (s[s.Length() - 1] == '\n')
        s.Remove(s.Length() - 1);
      wxMessageBox(s, _T("Error parsing the chapter file"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return;
    }

    if (chapters != NULL)
      delete chapters;
    tc_chapters->DeleteAllItems();
    chapters = new_chapters;
    m_chapters->Enable(ID_M_CHAPTERS_SAVE, true);
    m_chapters->Enable(ID_M_CHAPTERS_SAVEAS, true);
    m_chapters->Enable(ID_M_CHAPTERS_VERIFY, true);
    b_add_chapter->Enable(true);
    b_remove_chapter->Enable(true);

    file_name = dlg.GetPath();
    tid_root = tc_chapters->AddRoot(file_name);
    add_recursively(tid_root, *chapters);
    expand_subtree(*tc_chapters, tid_root);

    enable_inputs(false);
    
    mdlg->set_status_bar("Chapters loaded.");
  }
}

void tab_chapters::on_save_chapters(wxCommandEvent &evt) {
}

void tab_chapters::on_save_chapters_as(wxCommandEvent &evt) {
}

void tab_chapters::on_verify_chapters(wxCommandEvent &evt) {
}

void tab_chapters::on_add_chapter(wxCommandEvent &evt) {
  wxTreeItemId id;
  KaxEditionEntry *eentry;
  KaxChapterAtom *chapter;
  EbmlMaster *m;
  chapter_node_data_c *d;
  wxString s;

  id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

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

  old_id = evt.GetOldItem();
  if (old_id.IsOk())
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
  vector<string>::iterator eit;
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

  s = tc_language_codes->GetValue();
  strip(s);
  l_codes = split(s.c_str(), " ");
  i = 0;
  while (i < l_codes.size())
    if (!is_valid_iso639_2_code(l_codes[i].c_str())) {
      wxMessageBox(_(wxString("'") + l_codes[i].c_str() +
                     wxString("' is not a valid ISO639-2 language code. "
                              "Removing this entry.")),
                   _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      eit = l_codes.begin();
      eit += i;
      l_codes.erase(eit);
    } else
      i++;

  s = tc_country_codes->GetValue();
  strip(s);
  c_codes = split(s.c_str(), " ");
  i = 0;
  while (i < c_codes.size())
    if ((c_codes[i].length() != 2) ||
        (c_codes[i][0] < 'a') || (c_codes[i][0] > 'z') ||
        (c_codes[i][1] < 'a') || (c_codes[i][1] > 'z')) {
      wxMessageBox(_(wxString("'") + c_codes[i].c_str() +
                     wxString("' is not a valid two-letter country "
                              "code. Removing this entry.")),
                   _("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
      eit = c_codes.begin();
      eit += i;
      c_codes.erase(eit);
    } else
      i++;

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

IMPLEMENT_CLASS(tab_chapters, wxPanel);
BEGIN_EVENT_TABLE(tab_chapters, wxPanel)
  EVT_BUTTON(ID_B_ADDCHAPTER, tab_chapters::on_add_chapter)
  EVT_BUTTON(ID_B_REMOVECHAPTER, tab_chapters::on_remove_chapter)
  EVT_TREE_SEL_CHANGED(ID_TRC_CHAPTERS, tab_chapters::on_entry_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTLANGUAGECODE,
               tab_chapters::on_language_code_selected)
  EVT_COMBOBOX(ID_CB_CHAPTERSELECTCOUNTRYCODE,
               tab_chapters::on_country_code_selected)
END_EVENT_TABLE();
