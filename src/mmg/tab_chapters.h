/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_chapters.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declarations for the chapter tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TAB_CHAPTERS_H
#define __TAB_CHAPTERS_H

#include "wx/button.h"
#include "wx/combobox.h"
#include "wx/panel.h"
#include "wx/treectrl.h"

#include <matroska/KaxChapters.h>

#include "kax_analyzer.h"

#define ID_TRC_CHAPTERS                   16000
#define ID_B_ADDCHAPTER                   16001
#define ID_B_REMOVECHAPTER                16002
#define ID_T_CHAPTERVALUES                16003
#define ID_TC_CHAPTERNAME                 16004
#define ID_TC_CHAPTERLANGUAGES            16005
#define ID_TC_CHAPTERCOUNTRYCODES         16006
#define ID_TC_CHAPTERSTART                16007
#define ID_TC_CHAPTEREND                  16008
#define ID_CB_CHAPTERSELECTLANGUAGECODE   16009
#define ID_CB_CHAPTERSELECTCOUNTRYCODE    16010
#define ID_B_ADDSUBCHAPTER                16011
#define ID_B_SETVALUES                    16012

using namespace libmatroska;

class tab_chapters: public wxPanel {
  DECLARE_CLASS(tab_chapters);
  DECLARE_EVENT_TABLE();
public:
  wxTreeCtrl *tc_chapters;
  wxTreeItemId tid_root;
  wxButton *b_add_chapter, *b_add_subchapter, *b_remove_chapter;
  wxButton *b_set_values;
  wxMenu *m_chapters;

  wxTextCtrl *tc_chapter_name, *tc_language_codes, *tc_country_codes;
  wxTextCtrl *tc_start_time, *tc_end_time;
  wxComboBox *cob_add_language_code, *cob_add_country_code;
  bool inputs_enabled;

  wxString file_name;
  bool source_is_kax_file, source_is_simple_format;

  KaxChapters *chapters;

  kax_analyzer_c *analyzer;

public:
  tab_chapters(wxWindow *parent, wxMenu *nm_chapters);
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
  void on_set_default_values(wxCommandEvent &evt);
  void on_set_values(wxCommandEvent &evt);
  void set_values_recursively(wxTreeItemId id, string &language,
                              bool set_language);

  bool copy_values(wxTreeItemId id);
  int64_t parse_time(string s);
  bool verify_atom_recursively(EbmlElement *e, int64_t p_start = -1,
                               int64_t p_end = -1);
  bool verify();
  void verify_language_codes(string s, vector<string> &parts);
  void verify_country_codes(string s, vector<string> &parts);
  void add_recursively(wxTreeItemId &parent, EbmlMaster &master);
  wxString create_chapter_label(KaxChapterAtom &chapter);
  void fix_missing_languages(EbmlMaster &master);
  void enable_inputs(bool enable);
  void enable_buttons(bool enable);
  bool select_file_name();
  bool load(wxString name);
  void save();
};

#endif // __TAB_CHAPTERS_H
