/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   update checker thread & dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_UPDATE_CHECKER_H
# define __MMG_UPDATE_CHECKER_H

# include "common/os.h"

# if defined(HAVE_CURL_EASY_H)

#  include <wx/wx.h>
#  include <wx/button.h>
#  include <wx/hyperlink.h>
#  include <wx/richtext/richtextctrl.h>
#  include <wx/sizer.h>
#  include <wx/thread.h>
#  include "common/version.h"

class mmg_dialog;

class update_check_thread_c: public wxThread {
private:
  mmg_dialog *m_mdlg;

public:
  update_check_thread_c(mmg_dialog *mdlg);
  virtual void *Entry();
};

class update_check_dlg_c: public wxDialog {
  DECLARE_CLASS(update_check_dlg_c);
  DECLARE_EVENT_TABLE();
private:
  wxStaticText *m_st_status, *m_st_current_version, *m_st_available_version, *m_st_download_url_label;
  wxHyperlinkCtrl *m_hlc_download_url;
  wxButton *m_b_close;
  wxBoxSizer *m_siz_all;
  wxRichTextCtrl *m_changelog;

public:
  update_check_dlg_c(wxWindow *parent);
  void update_status(const wxString &status);
  void update_info(mtx_release_version_t const &version, mtx::xml::document_cptr const &releases_info);
  void update_changelog(mtx::xml::document_cptr const &releases_info);
  void update_changelog_failed();
  void on_close_pressed(wxCommandEvent &evt);
  void on_close(wxCloseEvent &evt);
  void on_url_pressed(wxCommandEvent &evt);
private:
  void close_dialog();
  void setup_changelog_ctrl();
  void write_changelog_title();
};

extern const wxEventType wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED;

#  define UPDATE_CHECK_START                 1
#  define UPDATE_CHECK_DONE_NO_NEW_RELEASE   2
#  define UPDATE_CHECK_DONE_NEW_RELEASE      3
#  define UPDATE_CHECK_DONE_ERROR            4
#  define UPDATE_CHECK_DONE_DIALOG_DISMISSED 5

# endif  // defined(HAVE_CURL_EASY_H)
#endif // __MMG_UPDATE_CHECKER_H
