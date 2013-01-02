/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "chapter editor" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/statline.h>

#include <ebml/EbmlStream.h>

#include "common/bitvalue.h"
#include "common/chapters/chapters.h"
#include "common/common_pch.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/mm_io_x.h"
#include "common/mm_write_buffer_io.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "common/unique_numbers.h"
#include "common/wx.h"
#include "common/xml/ebml_chapters_converter.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/tabs/chapters.h"
#include "mmg/wx_kax_analyzer.h"

using namespace libebml;
using namespace libmatroska;

class chapters_drop_target_c: public wxFileDropTarget {
private:
  tab_chapters *m_owner;
public:
  chapters_drop_target_c(tab_chapters *owner)
    : m_owner{owner}
  {
  }

  virtual bool
  OnDropFiles(wxCoord /* x */,
              wxCoord /* y */,
              const wxArrayString &dropped_files) {
    m_owner->load(dropped_files[0]);
    return true;
  }
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
  auto siz_all = new wxBoxSizer(wxVERTICAL);
  SetTitle(Z("Select values to be applied"));
  siz_all->Add(new wxStaticText(this, wxID_STATIC,
                                Z("Please enter the values for the language and the\n"
                                  "country that you want to apply to all the chapters\n"
                                  "below and including the currently selected entry.")),
               0, wxLEFT | wxRIGHT | wxTOP, 10);

  auto siz_input = new wxFlexGridSizer(2);
  siz_input->AddGrowableCol(1);

  cb_language = new wxCheckBox(this, ID_CVD_CB_LANGUAGE, Z("Set language to:"));
  cb_language->SetValue(false);
  siz_input->Add(cb_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_language = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language->Enable(false);
  siz_input->Add(cob_language, 0, wxGROW, 0);

  cb_country = new wxCheckBox(this, ID_CVD_CB_COUNTRY, Z("Set country to:"));
  cb_country->SetValue(false);
  siz_input->Add(cb_country, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  cob_country = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_country->Enable(false);
  siz_input->Add(cob_country, 0, wxGROW, 0);

  siz_all->AddSpacer(10);
  siz_all->Add(siz_input, 0, wxGROW | wxLEFT | wxRIGHT, 10);
  siz_all->AddSpacer(10);

  size_t i;
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language->Append(sorted_iso_codes[i]);

  cob_country->Append(wxEmptyString);
  for (auto &cctld : cctlds)
    cob_country->Append(wxU(cctld));

  auto siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons->AddStretchSpacer();
  siz_buttons->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_buttons->AddSpacer(10);

  siz_all->Add(siz_buttons, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizer(siz_all);

  siz_all->SetSizeHints(this);
}

void
chapter_values_dlg::on_language_clicked(wxCommandEvent &) {
  cob_language->Enable(cb_language->IsChecked());
}

void
chapter_values_dlg::on_country_clicked(wxCommandEvent &) {
  cob_country->Enable(cb_country->IsChecked());
}

void
expand_subtree(wxTreeCtrl &tree,
               wxTreeItemId const &root,
               bool expand = true) {
  wxTreeItemIdValue cookie;

  if (!expand)
    tree.Collapse(root);
  auto child = tree.GetFirstChild(root, cookie);
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
                           wxMenu *chapters_menu)
  : wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL)
  , m_chapters_menu{chapters_menu}
  , m_chapters{}
{
  st_chapters  = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  auto siz_all = new wxBoxSizer(wxVERTICAL);
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

  auto siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(0);
  siz_fg->AddGrowableRow(0);

  siz_fg->Add(tc_chapters, 1, wxGROW | wxRIGHT | wxBOTTOM, 5);

  auto siz_column = new wxBoxSizer(wxVERTICAL);
  siz_column->Add(b_add_chapter, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_add_subchapter, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_remove_chapter, 0, wxGROW | wxBOTTOM, 15);
  siz_column->Add(b_set_values, 0, wxGROW | wxBOTTOM, 3);
  siz_column->Add(b_adjust_timecodes, 0, wxGROW | wxBOTTOM, 3);
  siz_fg->Add(siz_column, 0, wxLEFT | wxBOTTOM, 5);

  siz_all->Add(siz_fg, 10, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  // Start of flex grid sizer
  auto siz_upper_fg = new wxFlexGridSizer(4, 5, 5);
  siz_upper_fg->AddGrowableCol(1, 1);
  siz_upper_fg->AddGrowableCol(3, 1);

  // Line 1 columns 1 & 2
  st_start = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_start, 0, wxALIGN_CENTER_VERTICAL);
  tc_start_time = new wxTextCtrl(this, ID_TC_CHAPTERSTART, wxEmptyString);
  siz_upper_fg->Add(tc_start_time, 1, wxGROW);

  // Line 1 columns 3 & 4
  st_uid = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_uid, 0, wxALIGN_CENTER_VERTICAL);
  tc_uid = new wxTextCtrl(this, ID_TC_UID, wxEmptyString);
  siz_upper_fg->Add(tc_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL);

  // Line 2 columns 1 & 2
  st_end = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_end, 0, wxALIGN_CENTER_VERTICAL);
  tc_end_time = new wxTextCtrl(this, ID_TC_CHAPTEREND, wxEmptyString);
  siz_upper_fg->Add(tc_end_time, 1, wxGROW);

  // Line 2 columns 3 & 4
  st_segment_uid = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_segment_uid, 0, wxALIGN_CENTER_VERTICAL);
  tc_segment_uid = new wxTextCtrl(this, ID_TC_SEGMENT_UID, wxEmptyString);
  siz_upper_fg->Add(tc_segment_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL);

  // Line 3 columns 1 & 2
  st_flags = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_flags, 0, wxALIGN_CENTER_VERTICAL);

  auto siz_line = new wxBoxSizer(wxHORIZONTAL);
  cb_flag_hidden = new wxCheckBox(this, ID_CB_FLAGHIDDEN, wxEmptyString);
  siz_line->Add(cb_flag_hidden, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);

  cb_flag_enabled_default = new wxCheckBox(this, ID_CB_FLAGENABLEDDEFAULT, wxEmptyString);
  siz_line->Add(cb_flag_enabled_default, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 10);

  cb_flag_ordered = new wxCheckBox(this, ID_CB_FLAGORDERED, wxEmptyString);
  siz_line->Add(cb_flag_ordered, 0, wxALIGN_CENTER_VERTICAL, 0);

  siz_upper_fg->Add(siz_line, 1, wxGROW | wxALIGN_CENTER_VERTICAL);

  // Line 3 columns 3 & 4
  st_segment_edition_uid = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_upper_fg->Add(st_segment_edition_uid, 0, wxALIGN_CENTER_VERTICAL);
  tc_segment_edition_uid = new wxTextCtrl(this, ID_TC_SEGMENT_EDITION_UID, wxEmptyString);
  siz_upper_fg->Add(tc_segment_edition_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL);

  siz_all->Add(siz_upper_fg, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  // End of flex grid sizer


  sb_names = new wxStaticBox(this, wxID_STATIC, wxEmptyString);
  auto siz_sb = new wxStaticBoxSizer(sb_names, wxVERTICAL);

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

  cob_language_code = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERSELECTLANGUAGECODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language_code->SetMinSize(wxSize(0, 0));
  size_t i;
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language_code->Append(sorted_iso_codes[i]);
  cob_language_code->SetValue(wxEmptyString);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cob_language_code, 3, wxGROW, 0);
  siz_line->Add(10, 0, 0, 0, 0);

  st_country = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_line->Add(st_country, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  cob_country_code = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERSELECTCOUNTRYCODE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_country_code->Append(wxEmptyString);
  for (auto &cctld : cctlds)
    cob_country_code->Append(wxU(cctld));
  cob_country_code->SetValue(wxEmptyString);
  siz_line->Add(cob_country_code, 1, wxGROW, 0);
  siz_fg->Add(siz_line, 0, wxGROW, 0);

  siz_sb->Add(siz_fg,  0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->Add(siz_sb, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  // Fix tab order
  tc_uid->MoveAfterInTabOrder(cb_flag_ordered);
  tc_segment_uid->MoveAfterInTabOrder(tc_uid);
  tc_segment_edition_uid->MoveAfterInTabOrder(tc_segment_uid);

  enable_inputs(false);

  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVE,      false);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVEAS,    false);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVETOKAX, false);
  m_chapters_menu->Enable(ID_M_CHAPTERS_VERIFY,    false);
  enable_buttons(false);

  file_name               = wxEmptyString;
  source_is_kax_file      = false;
  source_is_simple_format = false;
  no_update               = false;

  SetDropTarget(new chapters_drop_target_c(this));
  SetSizer(siz_all);
}

tab_chapters::~tab_chapters() {
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
  st_segment_uid->SetLabel(Z("Segment UID:"));
  tc_segment_uid->SetToolTip(TIP("A segment to play in place of this chapter. The edition set via the segment edition UID should be used for this segment, otherwise no edition is used. "
                                 "This is a 128bit segment UID in the usual UID form: hex numbers with or without the \"0x\" prefix, with or without spaces, exactly 32 digits.\n"));
  st_segment_edition_uid->SetLabel(Z("Segment edition UID:"));
  tc_segment_edition_uid->SetToolTip(TIP("The edition UID to play from the segment linked in the chapter's segment UID. This is simply a number."));
  st_flags->SetLabel(Z("Flags:"));
  cb_flag_hidden->SetLabel(Z("hidden"));
  cb_flag_hidden->SetToolTip(TIP("If a chapter is marked 'hidden' then the player should not show this chapter entry "
                                 "to the user. Such entries could still be used by the menu system."));
  cb_flag_ordered->SetLabel(Z("ordered"));
  cb_flag_ordered->SetToolTip(TIP("If an edition is marked 'ordered' then the chapters can be defined multiple times and the order to play them is enforced."));

  set_flag_enabled_default_texts(tc_chapters->GetSelection());

  cb_flag_hidden->SetLabel(Z("hidden"));
  cb_flag_hidden->SetToolTip(TIP("If a chapter or an edition is marked 'hidden' then the player should not show this chapter entry (or all of this edition's entries) "
                                 "to the user. Such entries could still be used by the menu system."));
  sb_names->SetLabel(Z("Chapter names and languages"));
  b_add_chapter_name->SetLabel(Z("Add name"));
  b_remove_chapter_name->SetLabel(Z("Remove name"));
  st_name->SetLabel(Z("Name:"));
  st_language->SetLabel(Z("Language:"));
  st_country->SetLabel(Z("Country:"));
}

void
tab_chapters::set_flag_enabled_default_texts(wxTreeItemId id) {
  bool is_atom = true;
  if (id.IsOk() && (id != tid_root)) {
    auto t  = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
    is_atom = !t || t->is_atom;
  }

  if (is_atom) {
    cb_flag_enabled_default->SetLabel(Z("enabled"));
    cb_flag_enabled_default->SetToolTip(TIP("If a chapter is not marked 'enabled' then the player should skip the part of the file that this chapter occupies."));
  } else {
    cb_flag_enabled_default->SetLabel(Z("default"));
    cb_flag_enabled_default->SetToolTip(TIP("If an edition is marked as 'default' then it should be used as the default edition."));
  }
}

void
tab_chapters::enable_inputs(bool enable,
                            bool is_edition) {
  tc_chapter_name->Enable(enable);
  tc_start_time->Enable(enable);
  tc_end_time->Enable(enable);
  cob_language_code->Enable(enable);
  cob_country_code->Enable(enable);
  lb_chapter_names->Enable(enable);
  b_add_chapter_name->Enable(enable);
  b_remove_chapter_name->Enable(enable);
  cb_flag_hidden->Enable(enable || is_edition);
  cb_flag_enabled_default->Enable(enable || is_edition);
  cb_flag_ordered->Enable(is_edition);
  st_start->Enable(enable);
  st_end->Enable(enable);
  st_uid->Enable(enable || is_edition);
  tc_uid->Enable(enable || is_edition);
  st_segment_uid->Enable(enable);
  tc_segment_uid->Enable(enable);
  st_segment_edition_uid->Enable(enable);
  tc_segment_edition_uid->Enable(enable);
  st_name->Enable(enable);
  st_language->Enable(enable);
  st_country->Enable(enable);
  sb_names->Enable(enable);
  st_flags->Enable(enable || is_edition);
  inputs_enabled = enable;
}

void
tab_chapters::enable_buttons(bool enable) {
  b_add_chapter->Enable(enable);
  b_add_subchapter->Enable(enable);
  b_remove_chapter->Enable(enable);
  enable_adjustment_buttons(enable && contains_atoms());
}

void
tab_chapters::enable_adjustment_buttons(bool enable) {
  b_set_values->Enable(enable);
  b_adjust_timecodes->Enable(enable);
}

bool
tab_chapters::contains_atoms()
  const {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return false;

  auto data   = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  auto master = data ? data->get() : m_chapters;
  if (!master)
    return false;

  std::function<bool(EbmlMaster *)> check_recursively = [&check_recursively](EbmlMaster *m) -> bool {
    auto atom = dynamic_cast<KaxChapterAtom *>(m);
    if (atom)
      return true;

    for (auto child : *m) {
      atom = dynamic_cast<KaxChapterAtom *>(child);
      if (atom)
        return true;
      auto master = dynamic_cast<EbmlMaster *>(child);
      if (master && check_recursively(master))
        return true;
    }

    return false;
  };

  return check_recursively(master);
}

void
tab_chapters::on_new_chapters(wxCommandEvent &) {
  file_name = wxEmptyString;
  tc_chapters->DeleteAllItems();
  tid_root = tc_chapters->AddRoot(Z("(new chapter file)"));
  m_chapters_cp = ebml_element_cptr(new KaxChapters);
  m_chapters    = static_cast<KaxChapters *>(m_chapters_cp.get());

  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVE, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVEAS, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVETOKAX, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_VERIFY, true);
  enable_buttons(true);

  enable_inputs(false);
  source_is_kax_file = false;
  if (analyzer)
    analyzer.reset();

  clear_list_of_unique_numbers(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_numbers(UNIQUE_EDITION_IDS);

  mdlg->set_status_bar(Z("New chapters created."));
}

wxString
tab_chapters::create_chapter_label(KaxChapterAtom &chapter) {
  wxString label       = Z("(unnamed chapter)");
  std::string language = "eng";
  auto display         = FindChild<KaxChapterDisplay>(chapter);

  if (display) {
    label    = FindChildValue<KaxChapterString>(*display, label);
    language = FindChildValue<KaxChapterLanguage>(*display, language);
  }
  label += wxT(" [");
  auto tstart = FindChild<KaxChapterTimeStart>(chapter);
  if (tstart) {
    auto timestamp  = static_cast<EbmlUInteger *>(tstart)->GetValue();
    label          += wxString::Format(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));

    auto tend       = FindChild<KaxChapterTimeEnd>(chapter);
    if (tend) {
      timestamp  = static_cast<EbmlUInteger *>(tend)->GetValue();
      label     += wxT(" - ") + wxString::Format(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp));
    }

    label += wxT("; ");
  }

  return label + wxU(language) + wxT("]");
}

void
tab_chapters::add_recursively(wxTreeItemId &parent,
                              EbmlMaster &master) {
  for (auto child : master) {
    if (EbmlId(*child) == KaxEditionEntry::ClassInfos.GlobalId) {
      auto eentry = static_cast<KaxEditionEntry *>(child);
      auto label  = wxString::Format(Z("Edition %d"), (int)tc_chapters->GetChildrenCount(tid_root, false) + 1);
      auto item   = tc_chapters->AppendItem(parent, label, -1, -1, new chapter_node_data_c(eentry));
      add_recursively(item, *static_cast<EbmlMaster *>(child));

    } else if (EbmlId(*child) == KaxChapterAtom::ClassInfos.GlobalId) {
      auto chapter = static_cast<KaxChapterAtom *>(child);
      auto label   = create_chapter_label(*chapter);
      auto item    = tc_chapters->AppendItem(parent, label, -1, -1, new chapter_node_data_c(chapter));
      add_recursively(item, *static_cast<EbmlMaster *>(child));
    }
  }
}

void
tab_chapters::fix_missing_languages(EbmlMaster &master) {
  for (auto child : master) {
    if (dynamic_cast<EbmlMaster *>(child))
      fix_missing_languages(*static_cast<EbmlMaster *>(child));

    if (EbmlId(*child) != KaxChapterAtom::ClassInfos.GlobalId)
      continue;

    auto &master  = *static_cast<EbmlMaster *>(child);
    auto &display = GetChild<KaxChapterDisplay>(master);
    auto language = FindChild<KaxChapterLanguage>(display);
    auto uid      = FindChild<KaxChapterUID>(master);

    if (!language)
      GetChild<KaxChapterLanguage>(display).SetValue("eng");

    if (uid && is_unique_number(uid->GetValue(), UNIQUE_CHAPTER_IDS))
      add_unique_number(uid->GetValue(), UNIQUE_CHAPTER_IDS);
  }
}

void
tab_chapters::on_load_chapters(wxCommandEvent &) {
  wxFileDialog dlg(nullptr, Z("Choose a chapter file"), last_open_dir, wxEmptyString,
                   wxString::Format(Z("Chapter files (*.xml;*.txt;*.mka;*.mkv;*.mk3d;*.cue)|*.xml;*.txt;*.mka;*.mkv;*.mk3d;*.cue|%s"), ALLFILES.c_str()), wxFD_OPEN);

  if ((dlg.ShowModal() == wxID_OK) && load(dlg.GetPath()))
  last_open_dir = dlg.GetDirectory();
}

bool
tab_chapters::load(wxString name) {
  ebml_element_cptr new_chapters;

  clear_list_of_unique_numbers(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_numbers(UNIQUE_EDITION_IDS);

  try {
    if (kax_analyzer_c::probe(wxMB(name))) {
      analyzer  = wx_kax_analyzer_cptr(new wx_kax_analyzer_c(this, wxMB(name)));
      file_name = name;
      if (!analyzer->process()) {
        wxMessageBox(Z("This file could not be opened or parsed."), Z("File parsing failed"), wxOK | wxCENTER | wxICON_ERROR);
        analyzer.reset();

        return false;
      }
      auto pos = analyzer->find(KaxChapters::ClassInfos.GlobalId);
      if (-1 == pos) {
        wxMessageBox(Z("This file does not contain any chapters."), Z("No chapters found"), wxOK | wxCENTER | wxICON_INFORMATION);
        analyzer.reset();

        return false;
      }

      new_chapters = analyzer->read_element(pos);
      if (!new_chapters)
        throw mtx::exception();
      source_is_kax_file      = true;
      source_is_simple_format = false;

      analyzer->close_file();

    } else {
      new_chapters       = ebml_element_cptr(parse_chapters(wxMB(name), 0, -1, 0, "", "", true, &source_is_simple_format));
      source_is_kax_file = false;
    }
  } catch (mtx::exception &) {
    analyzer.reset();
    wxString s = Z("This file does not contain valid chapters.");
    break_line(s);
    while (s[s.Length() - 1] == wxT('\n'))
      s.Remove(s.Length() - 1);
    wxMessageBox(s, Z("Error parsing the file"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  m_chapters_cp = new_chapters;
  m_chapters    = static_cast<KaxChapters *>(m_chapters_cp.get());

  tc_chapters->DeleteAllItems();
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVE, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVEAS, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_SAVETOKAX, true);
  m_chapters_menu->Enable(ID_M_CHAPTERS_VERIFY, true);
  enable_buttons(true);

  file_name = name;
  clear_list_of_unique_numbers(UNIQUE_CHAPTER_IDS);
  clear_list_of_unique_numbers(UNIQUE_EDITION_IDS);
  fix_missing_languages(*m_chapters);
  tid_root = tc_chapters->AddRoot(file_name);
  add_recursively(tid_root, *m_chapters);
  expand_subtree(*tc_chapters, tid_root);

  enable_inputs(false);

  mdlg->set_last_chapters_in_menu(name);
  mdlg->set_status_bar(Z("Chapters loaded."));

  return true;
}

void
tab_chapters::on_save_chapters(wxCommandEvent &) {
  if (!verify())
    return;

  if (source_is_kax_file) {
    write_chapters_to_matroska_file();
    return;
  }

  if ((!file_name.empty() && !source_is_simple_format) || select_file_name())
    save();
}

void
tab_chapters::on_save_chapters_to_kax_file(wxCommandEvent &) {
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

  last_open_dir           = dlg.GetDirectory();
  file_name               = dlg.GetPath();
  source_is_kax_file      = true;
  source_is_simple_format = false;

  tc_chapters->SetItemText(tid_root, file_name);

  analyzer = wx_kax_analyzer_cptr(new wx_kax_analyzer_c(this, wxMB(file_name)));
  if (!analyzer->process()) {
    analyzer.reset();
    return;
  }

  write_chapters_to_matroska_file();
  mdlg->set_last_chapters_in_menu(file_name);

  analyzer->close_file();
}

void
tab_chapters::on_save_chapters_as(wxCommandEvent &) {
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
    out = mm_io_cptr(mm_write_buffer_io_c::open(wxMB(file_name), 128 * 1024));
  } catch (mtx::mm_io::exception &ex) {
    wxMessageBox(wxString::Format(Z("Could not open the destination file '%s' for writing. Error code: %d (%s)."), file_name.c_str(), errno, wxUCS(ex.error())), Z("Error opening file"), wxCENTER | wxOK | wxICON_ERROR);
    return;
  }

#if defined(SYS_WINDOWS)
  out->use_dos_style_newlines(true);
#endif
  mtx::xml::ebml_chapters_converter_c::write_xml(*m_chapters, *out);

  source_is_kax_file      = false;
  source_is_simple_format = false;

  mdlg->set_last_chapters_in_menu(file_name);
  mdlg->set_status_bar(Z("Chapters written."));
}

void
tab_chapters::on_verify_chapters(wxCommandEvent &) {
  verify(true);
}

bool
tab_chapters::verify_atom_recursively(EbmlElement *e) {
  if (dynamic_cast<KaxEditionUID *>(e))
    return true;

  auto chapter = dynamic_cast<KaxChapterAtom *>(e);
  if (!chapter)
    return false;

  if (!FindChild<KaxChapterUID>(*chapter))
    GetChild<KaxChapterUID>(*chapter).SetValue(create_unique_number(UNIQUE_CHAPTER_IDS));

  auto display = FindChild<KaxChapterDisplay>(*chapter);
  auto *cs     = display ? FindChild<KaxChapterString>(*display) : nullptr;

  if (!display || !cs) {
    wxMessageBox(Z("One of the chapters does not have a name."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  wxString label = cs->GetValue().c_str();
  auto start     = FindChild<KaxChapterTimeStart>(*chapter);
  if (!start) {
    wxMessageBox(wxString::Format(Z("The chapter '%s' is missing the start time."), label.c_str()), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  auto clanguage = FindChild<KaxChapterLanguage>(*display);
  if (!clanguage) {
    wxMessageBox(wxString::Format(Z("The chapter '%s' is missing its language."), label.c_str()), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  auto language = clanguage->GetValue();
  if (language.empty() || !is_valid_iso639_2_code(language)) {
    wxMessageBox(wxString::Format(Z("The selected language '%s' for the chapter '%s' is not a valid language code. Please select one of the predefined ones."),
                                  wxUCS(language), label.c_str()),
                 Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  for (auto child : *chapter)
    if (dynamic_cast<KaxChapterAtom *>(child))
      if (!verify_atom_recursively(child))
        return false;

  return true;
}

bool
tab_chapters::verify(bool called_interactively) {
  if (!m_chapters_cp){
    wxMessageBox(Z("No chapter entries have been create yet."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
    return false;
  }

  if (0 == m_chapters->ListSize())
    return true;

  auto id = tc_chapters->GetSelection();
  if (id.IsOk())
    copy_values(id);

  for (auto chapters_child : *m_chapters) {
    auto eentry = dynamic_cast<KaxEditionEntry *>(chapters_child);
    if (!eentry)
      continue;

    auto atom_itr = brng::find_if(eentry->GetElementList(), [](EbmlElement const *eentry_child) { return !!dynamic_cast<KaxChapterAtom const *>(eentry_child); });
    if (eentry->GetElementList().end() == atom_itr) {
      wxMessageBox(Z("Each edition must contain at least one chapter."), Z("Chapter verification error"), wxCENTER | wxOK | wxICON_ERROR);
      return false;
    }
  }

  fix_mandatory_chapter_elements(m_chapters);

  for (auto chapters_child : *m_chapters) {
    auto eentry = dynamic_cast<KaxEditionEntry *>(chapters_child);
    if (!eentry)
      continue;

    for (auto eentry_child : *eentry)
      if (dynamic_cast<KaxChapterAtom *>(eentry_child) && !verify_atom_recursively(eentry_child))
        return false;
  }

  if (!m_chapters->CheckMandatory())
    wxdie(Z("verify failed: chapters->CheckMandatory() is false. This should not have happened. Please file a bug report.\n"));
  m_chapters->UpdateSize();

  if (called_interactively)
    wxMessageBox(Z("All chapter entries are valid."), Z("Chapter verification succeeded"), wxCENTER | wxOK | wxICON_INFORMATION);

  return true;
}

void
tab_chapters::on_add_chapter(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  copy_values(id);

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (id == tid_root) {
    auto eentry = new KaxEditionEntry;
    auto euid   = new KaxEditionUID;

    euid->SetValue(create_unique_number(UNIQUE_EDITION_IDS));

    m_chapters->PushElement(*eentry);
    eentry->PushElement(*euid);

    data = new chapter_node_data_c(eentry);
    id   = tc_chapters->AppendItem(tid_root, wxString::Format(Z("EditionEntry %u"), static_cast<unsigned int>(m_chapters->ListSize())), -1, -1, data);
  }

  auto chapter = static_cast<KaxChapterAtom *>(empty_ebml_master(new KaxChapterAtom));
  auto &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
  GetChild<KaxChapterString>(display).SetValueUTF8(Y("(unnamed)"));
  if (!g_default_chapter_language.empty())
    GetChild<KaxChapterLanguage>(display).SetValue(g_default_chapter_language);
  if (!g_default_chapter_country.empty())
    GetChild<KaxChapterCountry>(display).SetValue(g_default_chapter_country);
  GetChild<KaxChapterUID>(*chapter).SetValue(create_unique_number(UNIQUE_CHAPTER_IDS));

  auto label = create_chapter_label(*chapter);

  if (data->is_atom) {
    auto parent_id   = tc_chapters->GetItemParent(id);
    auto parent_data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(parent_id));
    auto &elements   = parent_data->get()->GetElementList();

    auto chapter_itr = brng::find(elements, data->chapter);
    if (elements.end() == chapter_itr)
      wxdie(Z("start >= m->ListSize(). This should not have happened. Please file a bug report. Thanks."));
    elements.insert(chapter_itr + 1, chapter);

    tc_chapters->InsertItem(parent_id, id, label, -1, -1, new chapter_node_data_c(chapter));

  } else {
    data->eentry->PushElement(*chapter);
    tc_chapters->AppendItem(id, label, -1, -1, new chapter_node_data_c(chapter));
  }

  expand_subtree(*tc_chapters, tc_chapters->GetSelection(), true);

  enable_buttons(true);
}

void
tab_chapters::on_add_subchapter(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;
  copy_values(id);

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (id == tid_root) {
    auto eentry = new KaxEditionEntry;
    data        = new chapter_node_data_c(eentry);
    id          = tc_chapters->AppendItem(tid_root, wxString::Format(Z("EditionEntry %u"), static_cast<unsigned int>(m_chapters->ListSize())), -1, -1, data);

    m_chapters->PushElement(*eentry);
  }

  auto chapter  = static_cast<KaxChapterAtom *>(empty_ebml_master(new KaxChapterAtom));
  auto &display = GetEmptyChild<KaxChapterDisplay>(*chapter);
  GetChild<KaxChapterString>(display).SetValueUTF8(Y("(unnamed)"));
  if (!g_default_chapter_language.empty())
    GetChild<KaxChapterLanguage>(display).SetValue(g_default_chapter_language);
  if (!g_default_chapter_country.empty())
    GetChild<KaxChapterCountry>(display).SetValue(g_default_chapter_country);
  GetChild<KaxChapterUID>(*chapter).SetValue(create_unique_number(UNIQUE_CHAPTER_IDS));

  data->get()->PushElement(*chapter);
  tc_chapters->AppendItem(id, create_chapter_label(*chapter), -1, -1, new chapter_node_data_c(chapter));

  expand_subtree(*tc_chapters, tc_chapters->GetSelection(), true);

  enable_buttons(true);
}

void
tab_chapters::on_remove_chapter(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk() || (id == tid_root))
    return;

  auto parent_id     = tc_chapters->GetItemParent(id);
  EbmlMaster *master = nullptr;
  if (parent_id == tid_root)
    master = m_chapters;
  else {
    auto parent_data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(parent_id));
    if (!parent_data)
      return;
    master = parent_data->get();
  }

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data)
    return;

  auto &elements         = master->GetElementList();
  auto element_to_delete = data->get();
  auto itr               = brng::find(elements, element_to_delete);

  if (itr == elements.end())
    return;

  enable_buttons(false);
  enable_inputs(false);

  delete element_to_delete;
  elements.erase(itr);
  tc_chapters->DeleteChildren(id);
  tc_chapters->Delete(id);
}

void
tab_chapters::root_or_edition_selected(wxTreeEvent &evt) {
  auto t = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(evt.GetItem()));
  if (!t)
    return;

  bool is_edition = (evt.GetItem() != tid_root) && !t->is_atom;

  enable_inputs(false, is_edition);
  no_update = true;

  tc_chapter_name->SetValue(wxEmptyString);
  tc_start_time->SetValue(wxEmptyString);
  tc_end_time->SetValue(wxEmptyString);
  tc_segment_uid->SetValue(wxEmptyString);
  tc_segment_edition_uid->SetValue(wxEmptyString);

  if (is_edition) {
    auto euid = FindChild<KaxEditionUID>(t->eentry);
    tc_uid                 ->SetValue(euid ? wxU(to_string(euid->GetValue())) : wxU(L""));
    cb_flag_hidden         ->SetValue(FindChildValue<KaxEditionFlagHidden>(t->eentry));
    cb_flag_enabled_default->SetValue(FindChildValue<KaxEditionFlagDefault>(t->eentry));
    cb_flag_ordered        ->SetValue(FindChildValue<KaxEditionFlagOrdered>(t->eentry));

    set_flag_enabled_default_texts(evt.GetItem());

  } else {
    tc_uid->Enable(true);
    tc_uid->SetValue(wxEmptyString);
    tc_uid->Enable(false);
  }

  no_update = false;
}

void
tab_chapters::on_entry_selected(wxTreeEvent &evt) {
  enable_buttons(true);

  auto old_id = evt.GetOldItem();
  if (old_id.IsOk() && (old_id != tid_root))
    copy_values(old_id);

  auto t = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(evt.GetItem()));
  lb_chapter_names->Clear();

  if ((evt.GetItem() == tid_root) || !t || !t->is_atom) {
    root_or_edition_selected(evt);
    return;
  }

  enable_inputs(true);

  set_flag_enabled_default_texts(evt.GetItem());

  auto display = &GetChild<KaxChapterDisplay>(t->chapter);
  bool first   = true;
  while (display) {
    if (first) {
      set_display_values(display);
      first = false;
    }

    lb_chapter_names->Append(FindChildValue<KaxChapterString, wxString>(display, Z("(unnamed)")));
    display = FindNextChild<KaxChapterDisplay>(t->chapter, display);
  }

  set_timecode_values(t->chapter);

  cb_flag_hidden         ->SetValue(FindChildValue<KaxChapterFlagHidden>(t->chapter));
  cb_flag_enabled_default->SetValue(FindChildValue<KaxChapterFlagEnabled>(t->chapter, true));

  auto cuid = FindChildValue<KaxChapterUID>(t->chapter);
  tc_uid->SetValue(cuid ? wxU(to_string(cuid)) : L"");

  auto segment_uid = FindChild<KaxChapterSegmentUID>(t->chapter);
  tc_segment_uid->SetValue(segment_uid ? wxU(to_hex(segment_uid->GetBuffer(), segment_uid->GetSize(), true)) : wxU(L""));

  auto segment_edition_uid = FindChild<KaxChapterSegmentEditionUID>(t->chapter);
  tc_segment_edition_uid->SetValue(segment_edition_uid ? wxU(to_string(segment_edition_uid->GetValue())) : wxU(L""));

  lb_chapter_names->SetSelection(0);
  b_remove_chapter_name->Enable(lb_chapter_names->GetCount() > 1);
  tc_chapter_name->SetSelection(-1, -1);
  tc_chapter_name->SetFocus();
}

KaxChapterDisplay *
tab_chapters::get_selected_chapter_display() {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return nullptr;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data || !data->is_atom)
    return nullptr;

  size_t nth_chapter_display = 0;
  auto &elements             = data->get()->GetElementList();
  auto cdisplay_itr          = brng::find_if(elements, [&](EbmlElement *child) -> bool {
      if (EbmlId(*child) != KaxChapterDisplay::ClassInfos.GlobalId)
        return false;
      ++nth_chapter_display;
      return (nth_chapter_display == static_cast<size_t>(lb_chapter_names->GetSelection() + 1));
    });

  return elements.end() == cdisplay_itr ? nullptr : static_cast<KaxChapterDisplay *>(*cdisplay_itr);
}

void
tab_chapters::on_language_code_selected(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data->is_atom)
    return;

  auto cdisplay = get_selected_chapter_display();
  if (!cdisplay)
    return;

  GetChild<KaxChapterLanguage>(*cdisplay).SetValue(to_utf8(extract_language_code(cob_language_code->GetStringSelection())));
  tc_chapters->SetItemText(id, create_chapter_label(*data->chapter));
}

void
tab_chapters::on_country_code_selected(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data->is_atom)
    return;

  auto cdisplay = get_selected_chapter_display();
  if (!cdisplay)
    return;

  if (cob_country_code->GetStringSelection().Length() == 0) {
    size_t i = 0;
    while (i < cdisplay->ListSize()) {
      if (EbmlId(*(*cdisplay)[i]) == KaxChapterCountry::ClassInfos.GlobalId) {
        delete (*cdisplay)[i];
        cdisplay->Remove(i);
      } else
        i++;
    }
    return;
  }

  GetChild<KaxChapterCountry>(*cdisplay).SetValue(to_utf8(cob_country_code->GetStringSelection()));
}

void
tab_chapters::on_chapter_name_changed(wxCommandEvent &) {
  if (no_update)
    return;

  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data || !data->is_atom)
    return;

  auto cdisplay = get_selected_chapter_display();
  GetChild<KaxChapterString>(*cdisplay).SetValue(to_wide(tc_chapter_name->GetValue()));

  tc_chapters->SetItemText(id, create_chapter_label(*data->chapter));
  lb_chapter_names->SetString(lb_chapter_names->GetSelection(), tc_chapter_name->GetValue());
}

void
tab_chapters::set_values_recursively(wxTreeItemId id,
                                     wxString const &value,
                                     bool set_language) {
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (data && data->is_atom) {
    auto display = FindChild<KaxChapterDisplay>(data->chapter);
    while (display) {
      if (set_language) {
        DeleteChildren<KaxChapterLanguage>(display);
        display->PushElement((new KaxChapterLanguage)->SetValue(wxMB(value)));
      } else {
        DeleteChildren<KaxChapterCountry>(display);
        display->PushElement((new KaxChapterCountry)->SetValue(wxMB(value)));
      }

      display = FindNextChild<KaxChapterDisplay>(data->chapter, display);
    }

    tc_chapters->SetItemText(id, create_chapter_label(*data->chapter));
  }

  wxTreeItemIdValue cookie;
  auto next_child = tc_chapters->GetFirstChild(id, cookie);
  while (next_child.IsOk()) {
    set_values_recursively(next_child, value, set_language);
    next_child = tc_chapters->GetNextChild(id, cookie);
  }
}

void
tab_chapters::on_set_values(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  chapter_values_dlg dlg{this};
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (dlg.cb_language->IsChecked()) {
    auto s = extract_language_code(dlg.cob_language->GetValue());
    if (!is_valid_iso639_2_code(wxMB(s))) {
      wxMessageBox(wxString::Format(Z("The language '%s' is not a valid language and cannot be selected."), s.c_str()), Z("Invalid language selected"), wxICON_ERROR | wxOK);
      return;
    }
    set_values_recursively(id, s, true);
  }

  if (dlg.cb_country->IsChecked())
    set_values_recursively(id, dlg.cob_country->GetValue(), false);

  auto cdisplay = get_selected_chapter_display();
  if (cdisplay)
    set_display_values(cdisplay);
}

void
tab_chapters::on_adjust_timecodes(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto sadjustment =
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
  if (sadjustment.IsEmpty())
    return;

  int64_t mult;
  size_t offset;
  if (sadjustment.c_str()[0] == wxT('-')) {
    mult   = -1;
    offset =  1;
  } else if (sadjustment.c_str()[0] == wxT('+')) {
    mult   = 1;
    offset = 1;
  } else {
    mult   = 1;
    offset = 0;
  }
  sadjustment.Remove(0, offset);

  if (sadjustment.IsEmpty())
    return;

  auto adjustment = parse_time(sadjustment);
  if (-1 == adjustment) {
    wxMessageBox(Z("Invalid format used for the adjustment."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }
  adjustment *= mult;

  adjust_timecodes_recursively(id, adjustment);


  auto cdisplay = get_selected_chapter_display();
  if (cdisplay)
    set_display_values(cdisplay);
}

void
tab_chapters::adjust_timecodes_recursively(wxTreeItemId id,
                                           int64_t adjust_by) {
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (data && data->is_atom) {
    auto tstart = FindChild<KaxChapterTimeStart>(data->chapter);
    if (tstart)
      tstart->SetValue(std::max<int64_t>(static_cast<int64_t>(tstart->GetValue()) + adjust_by, 0));

    auto tend = FindChild<KaxChapterTimeEnd>(data->chapter);
    if (tend)
      tend->SetValue(std::max<int64_t>(static_cast<int64_t>(tend->GetValue()) + adjust_by, 0));

    tc_chapters->SetItemText(id, create_chapter_label(*data->chapter));
  }

  wxTreeItemIdValue cookie;
  auto next_child = tc_chapters->GetFirstChild(id, cookie);
  while (next_child.IsOk()) {
    adjust_timecodes_recursively(next_child, adjust_by);
    next_child = tc_chapters->GetNextChild(id, cookie);
  }
}

bool
tab_chapters::copy_values(wxTreeItemId id) {
  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data)
    return true;

  auto label   = tc_chapters->GetItemText(id);
  auto chapter = data->chapter;

  auto uid = tc_uid->GetValue();
  if (!uid.IsEmpty()) {
    uint64_t entered_uid = 0;
    if (!parse_number(wxMB(uid), entered_uid))
      wxMessageBox(Z("Invalid UID. A UID is simply a number."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    else {
      if (data->is_atom) {
        auto &cuid = GetChild<KaxChapterUID>(*data->chapter);
        if (entered_uid != cuid.GetValue()) {
          if (!is_unique_number(entered_uid, UNIQUE_CHAPTER_IDS)) {
            wxMessageBox(Z("Invalid UID. This chapter UID is already in use. The original UID has not been changed."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
            tc_uid->SetValue(uid);
          } else {
            remove_unique_number(cuid.GetValue(), UNIQUE_CHAPTER_IDS);
            add_unique_number(entered_uid, UNIQUE_CHAPTER_IDS);
            cuid.SetValue(entered_uid);
          }
        }
      } else {
        auto &euid = GetChild<KaxEditionUID>(*data->eentry);
        if (entered_uid != euid.GetValue()) {
          if (!is_unique_number(entered_uid, UNIQUE_EDITION_IDS)) {
            wxMessageBox(Z("Invalid UID. This edition UID is already in use. The original UID has not been changed."), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
            tc_uid->SetValue(uid);
          } else {
            remove_unique_number(euid.GetValue(), UNIQUE_EDITION_IDS);
            add_unique_number(entered_uid, UNIQUE_EDITION_IDS);
            euid.SetValue(entered_uid);
          }
        }
      }
    }
  }

  if (!data->is_atom)
    return true;

  auto time_str = tc_start_time->GetValue();
  strip(time_str);
  auto ts_start = parse_time(time_str);
  if (-1 == ts_start) {
    wxMessageBox(wxString::Format(Z("Invalid format used for the start time for '%s'. Setting value to 0."), label.c_str()), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    ts_start = 0;
  }

  time_str = tc_end_time->GetValue();
  strip(time_str);
  auto ts_end = parse_time(time_str);
  if (-1 == ts_end) {
    wxMessageBox(wxString::Format(Z("Invalid format used for the end time for '%s'. Setting value to 0."), label.c_str()), Z("Input data error"), wxOK | wxCENTER | wxICON_ERROR);
    ts_end = 0;
  }

  copy_segment_uid(data);
  copy_segment_edition_uid(data);

  DeleteChildren<KaxChapterTimeStart>(chapter);
  DeleteChildren<KaxChapterTimeEnd>(chapter);

  if (ts_start >= 0)
    GetChild<KaxChapterTimeStart>(*chapter).SetValue(ts_start);
  if (ts_end >= 0)
    GetChild<KaxChapterTimeEnd>(*chapter).SetValue(ts_end);

  tc_chapters->SetItemText(id, create_chapter_label(*chapter));

  return true;
}

bool
tab_chapters::copy_segment_uid(chapter_node_data_c *data) {
  if (tc_segment_uid->GetValue().IsEmpty()) {
    DeleteChildren<KaxChapterSegmentUID>(data->chapter);
    return true;
  }

  try {
    bitvalue_c bit_value(to_utf8(tc_segment_uid->GetValue()), 128);
    GetChild<KaxChapterSegmentUID>(data->chapter).CopyBuffer(bit_value.data(), 16);
    return true;

  } catch (mtx::bitvalue_parser_x &) {
    wxMessageBox(Z("Invalid format used for the segment UID. Not using the value."));
    return false;
  }
}

bool
tab_chapters::copy_segment_edition_uid(chapter_node_data_c *data) {
  if (tc_segment_edition_uid->GetValue().IsEmpty()) {
    DeleteChildren<KaxChapterSegmentEditionUID>(data->chapter);
    return true;
  }

  uint64_t segment_edition_uid;
  if (parse_number(to_utf8(tc_segment_edition_uid->GetValue()), segment_edition_uid)) {
    GetChild<KaxChapterSegmentEditionUID>(data->chapter).SetValue(segment_edition_uid);
    return true;
  }

  wxMessageBox(Z("Invalid format used for the segment edition UID. Not using the value."));
  return false;
}

void
tab_chapters::on_add_chapter_name(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data->is_atom)
    return;

  auto &elements             = data->chapter->GetElementList();
  auto selected_cdisplay     = get_selected_chapter_display();
  auto selected_cdisplay_itr = brng::find(elements, selected_cdisplay);
  if (elements.end() != selected_cdisplay_itr)
    ++selected_cdisplay_itr;

  auto cdisplay = new KaxChapterDisplay;
  GetChild<KaxChapterString>(*cdisplay).SetValueUTF8(Y("(unnamed)"));
  GetChild<KaxChapterLanguage>(*cdisplay).SetValue(!g_default_chapter_language.empty() ? g_default_chapter_language : "eng");
  if (!g_default_chapter_country.empty())
    GetChild<KaxChapterCountry>(*cdisplay).SetValue(g_default_chapter_country);

  elements.insert(selected_cdisplay_itr, cdisplay);

  wxString s = Z("(unnamed)");
  lb_chapter_names->InsertItems(1, &s, lb_chapter_names->GetSelection() + 1);
  lb_chapter_names->SetSelection(lb_chapter_names->GetSelection() + 1);
  set_display_values(cdisplay);

  b_remove_chapter_name->Enable(true);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_remove_chapter_name(wxCommandEvent &) {
  auto id = tc_chapters->GetSelection();
  if (!id.IsOk())
    return;

  auto data = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(id));
  if (!data || !data->is_atom)
    return;

  auto cdisplay = get_selected_chapter_display();

  if (cdisplay) {
    data->chapter->GetElementList().erase(brng::find(data->chapter->GetElementList(), cdisplay));
    delete cdisplay;
  }

  int selection = lb_chapter_names->GetSelection();
  lb_chapter_names->Delete(selection);
  lb_chapter_names->SetSelection(std::max(0, selection - 1));
  if (selection == 0)
    tc_chapters->SetItemText(id, create_chapter_label(*data->chapter));

  cdisplay = get_selected_chapter_display();

  if (cdisplay)
    set_display_values(cdisplay);
  tc_chapter_name->SetFocus();
  b_remove_chapter_name->Enable(lb_chapter_names->GetCount() > 1);
}

void
tab_chapters::on_chapter_name_selected(wxCommandEvent &) {
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
  cdisplay = nullptr;
  n = 0;
  for (i = 0; i < t->chapter->ListSize(); i++)
    if (EbmlId(*(*t->chapter)[i]) == KaxChapterDisplay::ClassInfos.GlobalId) {
      n++;
      if (n == static_cast<size_t>(lb_chapter_names->GetSelection() + 1)) {
        cdisplay = (KaxChapterDisplay *)(*t->chapter)[i];
        break;
      }
    }
  if (!cdisplay)
    return;

  set_display_values(cdisplay);
  tc_chapter_name->SetFocus();
}

void
tab_chapters::on_chapter_name_enter(wxCommandEvent &) {
  wxTreeItemIdValue cookie;

  auto id = tc_chapters->GetSelection();
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
tab_chapters::on_flag_hidden(wxCommandEvent &) {
  auto selected = tc_chapters->GetSelection();
  if (!selected.IsOk() || (selected == tid_root))
    return;
  auto t = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(selected));
  if (!t)
    return;

  if (!t->is_atom) {
    if (cb_flag_hidden->IsChecked())
      GetChild<KaxEditionFlagHidden>(*t->eentry).SetValue(1);
    else
      DeleteChildren<KaxEditionFlagHidden>(t->eentry);
    return;
  }

  if (cb_flag_hidden->IsChecked())
    GetChild<KaxChapterFlagHidden>(*t->chapter).SetValue(1);
  else
    DeleteChildren<KaxChapterFlagHidden>(t->chapter);
}

void
tab_chapters::on_flag_enabled_default(wxCommandEvent &) {
  auto selected = tc_chapters->GetSelection();
  if (!selected.IsOk() || (selected == tid_root))
    return;

  auto t = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(selected));
  if (!t)
    return;

  if (!t->is_atom) {
    if (cb_flag_enabled_default->IsChecked())
      GetChild<KaxEditionFlagDefault>(*t->eentry).SetValue(1);
    else
      DeleteChildren<KaxEditionFlagDefault>(t->eentry);
    return;
  }

  if (!cb_flag_enabled_default->IsChecked())
    GetChild<KaxChapterFlagEnabled>(*t->chapter).SetValue(0);
  else
    DeleteChildren<KaxChapterFlagEnabled>(t->chapter);
}

void
tab_chapters::on_flag_ordered(wxCommandEvent &) {
  auto selected = tc_chapters->GetSelection();
  if (!selected.IsOk() || (selected == tid_root))
    return;
  auto t = static_cast<chapter_node_data_c *>(tc_chapters->GetItemData(selected));
  if (!t || t->is_atom)
    return;

  if (cb_flag_ordered->IsChecked())
    GetChild<KaxEditionFlagOrdered>(*t->eentry).SetValue(1);
  else
    DeleteChildren<KaxEditionFlagOrdered>(t->eentry);
}

void
tab_chapters::set_timecode_values(KaxChapterAtom *atom) {
  no_update = true;

  auto tstart = FindChild<KaxChapterTimeStart>(atom);
  if (tstart) {
    auto timestamp = tstart->GetValue();
    tc_start_time->SetValue(wxString::Format(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp)));
  } else
    tc_start_time->SetValue(wxEmptyString);

  auto tend = FindChild<KaxChapterTimeEnd>(atom);
  if (tend) {
    auto timestamp = tend->GetValue();
    tc_end_time->SetValue(wxString::Format(wxT(FMT_TIMECODEN), ARG_TIMECODEN(timestamp)));
  } else
    tc_end_time->SetValue(wxEmptyString);

  no_update = false;
}

void
tab_chapters::set_display_values(KaxChapterDisplay *display) {
  no_update = true;

  tc_chapter_name->SetValue(FindChildValue<KaxChapterString, wxString>(display, Z("(unnamed)")));

  auto language = wxU(FindChildValue<KaxChapterLanguage>(display, std::string{"eng"}));
  for (size_t i = 0; i < sorted_iso_codes.Count(); i++)
    if (extract_language_code(sorted_iso_codes[i]) == language) {
      language = sorted_iso_codes[i];
      break;
    }

  auto count = cob_language_code->GetCount();
  bool found = false;
  size_t i   = 0;
  while (!found && (i < count))
    if (extract_language_code(cob_language_code->GetString(i)) == language)
      found = true;
    else
      ++i;

  if (found)
    cob_language_code->SetSelection(i);
  else
    cob_language_code->SetValue(language);

  cob_country_code->SetValue(wxU(FindChildValue<KaxChapterCountry>(display)));

  no_update = false;
}

int64_t
tab_chapters::parse_time(wxString s) {
  std::string utf8s = wxMB(s);
  strip(utf8s);
  if (utf8s.empty())
    return -2;

  int64_t nsecs = 0;
  auto c        = utf8s.c_str();
  while (*c != 0) {
    if (!isdigit(*c)) {
      if (parse_timecode(utf8s, nsecs))
        return nsecs;
      return -1;
    }
    c++;
  }
  if (!parse_number(utf8s, nsecs))
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
  kax_analyzer_c::update_element_result_e result = (0 == m_chapters->ListSize() ? analyzer->remove_elements(KaxChapters::ClassInfos.GlobalId) : analyzer->update_element(m_chapters));

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
  EVT_CHECKBOX(ID_CB_FLAGHIDDEN,                tab_chapters::on_flag_hidden)
  EVT_CHECKBOX(ID_CB_FLAGENABLEDDEFAULT,        tab_chapters::on_flag_enabled_default)
  EVT_CHECKBOX(ID_CB_FLAGORDERED,               tab_chapters::on_flag_ordered)
  EVT_TEXT_ENTER(ID_TC_CHAPTERNAME,             tab_chapters::on_chapter_name_enter)
END_EVENT_TABLE();
