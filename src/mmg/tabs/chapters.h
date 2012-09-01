/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the chapter tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TAB_CHAPTERS_H
#define __TAB_CHAPTERS_H

#include "common/common_pch.h"

#include <wx/button.h>
#include <wx/panel.h>
#include <wx/treectrl.h>

#include <matroska/KaxChapters.h>

#include "mmg/wx_kax_analyzer.h"
#include "common/wx.h"

#define ID_TRC_CHAPTERS                   16000
#define ID_B_ADDCHAPTER                   16001
#define ID_B_REMOVECHAPTER                16002
#define ID_T_CHAPTERVALUES                16003
#define ID_TC_CHAPTERNAME                 16004
#define ID_TC_CHAPTERSTART                16007
#define ID_TC_CHAPTEREND                  16008
#define ID_CB_CHAPTERSELECTLANGUAGECODE   16009
#define ID_CB_CHAPTERSELECTCOUNTRYCODE    16010
#define ID_B_ADDSUBCHAPTER                16011
#define ID_B_SETVALUES                    16012
#define ID_LB_CHAPTERNAMES                16013
#define ID_B_ADD_CHAPTERNAME              16014
#define ID_B_REMOVE_CHAPTERNAME           16015
#define ID_B_ADJUSTTIMECODES              16016
#define ID_CB_FLAGHIDDEN                  16017
#define ID_CB_FLAGENABLEDDEFAULT          16018
#define ID_TC_UID                         16019
#define ID_CB_FLAGORDERED                 16020
#define ID_TC_SEGMENT_UID                 16021
#define ID_TC_SEGMENT_EDITION_UID         16022

using namespace libmatroska;

class chapter_node_data_c: public wxTreeItemData {
public:
  bool is_atom;
  KaxChapterAtom *chapter;
  KaxEditionEntry *eentry;

public:
  chapter_node_data_c(KaxChapterAtom *p_chapter)
    : is_atom{true}
    , chapter{p_chapter}
    , eentry{}
  {
  };

  chapter_node_data_c(KaxEditionEntry *p_eentry)
    : is_atom{false}
    , chapter{}
    , eentry{p_eentry}
  {
  };
};

class tab_chapters: public wxPanel {
  DECLARE_CLASS(tab_chapters);
  DECLARE_EVENT_TABLE();
public:
  wxTreeCtrl *tc_chapters;
  wxTreeItemId tid_root;
  wxButton *b_add_chapter, *b_add_subchapter, *b_remove_chapter;
  wxButton *b_set_values, *b_adjust_timecodes;
  wxMenu *m_chapters_menu;

  wxTextCtrl *tc_chapter_name, *tc_start_time, *tc_end_time;
  wxTextCtrl *tc_uid, *tc_segment_uid, *tc_segment_edition_uid;
  wxMTX_COMBOBOX_TYPE *cob_language_code, *cob_country_code;
  wxListBox *lb_chapter_names;
  wxButton *b_add_chapter_name, *b_remove_chapter_name;
  wxCheckBox *cb_flag_hidden, *cb_flag_enabled_default, *cb_flag_ordered;
  bool inputs_enabled, no_update;

  wxStaticText *st_start, *st_end, *st_uid, *st_name, *st_language, *st_flags, *st_segment_uid, *st_segment_edition_uid;
  wxStaticText *st_country, *st_chapters;
  wxStaticBox *sb_names;

  wxString file_name;
  bool source_is_kax_file, source_is_simple_format;

  wx_kax_analyzer_cptr analyzer;

  ebml_element_cptr m_chapters_cp;
  KaxChapters *m_chapters;

public:
  tab_chapters(wxWindow *parent, wxMenu *chapters_menu);
  ~tab_chapters();

  void on_new_chapters(wxCommandEvent &evt);
  void on_load_chapters(wxCommandEvent &evt);
  void on_save_chapters(wxCommandEvent &evt);
  void on_save_chapters_as(wxCommandEvent &evt);
  void on_save_chapters_to_kax_file(wxCommandEvent &evt);
  void on_verify_chapters(wxCommandEvent &evt);
  void on_add_chapter(wxCommandEvent &evt);
  void on_add_subchapter(wxCommandEvent &evt);
  void on_remove_chapter(wxCommandEvent &evt);
  void on_entry_selected(wxTreeEvent &evt);
  void on_language_code_selected(wxCommandEvent &evt);
  void on_country_code_selected(wxCommandEvent &evt);
  void on_chapter_name_selected(wxCommandEvent &evt);
  void on_chapter_name_changed(wxCommandEvent &evt);
  void on_chapter_name_enter(wxCommandEvent &evt);
  void on_add_chapter_name(wxCommandEvent &evt);
  void on_remove_chapter_name(wxCommandEvent &evt);
  void on_set_values(wxCommandEvent &evt);
  void on_adjust_timecodes(wxCommandEvent &evt);
  void on_flag_hidden(wxCommandEvent &evt);
  void on_flag_enabled_default(wxCommandEvent &evt);
  void on_flag_ordered(wxCommandEvent &evt);
  void set_values_recursively(wxTreeItemId id, const wxString &language,
                              bool set_language);
  void set_display_values(KaxChapterDisplay *display);
  void set_timecode_values(KaxChapterAtom *atom);
  void adjust_timecodes_recursively(wxTreeItemId id, int64_t adjust_by);

  bool copy_values(wxTreeItemId id);
  int64_t parse_time(wxString s);
  bool verify_atom_recursively(EbmlElement *e);
  bool verify(bool called_interactively = false);
  void add_recursively(wxTreeItemId &parent, EbmlMaster &master);
  wxString create_chapter_label(KaxChapterAtom &chapter);
  void fix_missing_languages(EbmlMaster &master);
  void enable_inputs(bool enable, bool is_edition = false);
  void enable_buttons(bool enable);
  bool select_file_name();
  bool load(wxString name);
  void save();
  bool is_empty();
  void translate_ui();

protected:
  void write_chapters_to_matroska_file();

  void root_or_edition_selected(wxTreeEvent &evt);
  void set_flag_enabled_default_texts(wxTreeItemId id);
  bool copy_segment_uid(chapter_node_data_c *data);
  bool copy_segment_edition_uid(chapter_node_data_c *data);
};

#endif // __TAB_CHAPTERS_H
