/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  jobs.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief job queue management dialog
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "os.h"

#include <errno.h>
#include <unistd.h>

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/clipbrd.h"
#include "wx/confbase.h"
#include "wx/datetime.h"
#include "wx/file.h"
#include "wx/fileconf.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#if defined(SYS_WINDOWS)
#include <windef.h>
#include <winbase.h>
#include <io.h>
#endif

#include "common.h"
#include "mm_io.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "jobs.h"

job_dialog::job_dialog(wxWindow *parent):
  wxDialog(parent, -1, "Job queue management", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(800, 455),
#else
           wxSize(800, 395),
#endif
           wxCAPTION) {
  wxListItem item;
  wxString s;
  wxDateTime dt;
  int i;
  long dummy;

  new wxStaticText(this, -1, wxS("Current and past jobs:"), wxPoint(20, 20));
  lv_jobs =
    new wxListView(this, ID_JOBS_LV_JOBS, wxPoint(20, 40), wxSize(660, 300),
                   wxLC_REPORT | wxSUNKEN_BORDER);

  item.m_mask = wxLIST_MASK_TEXT;
  item.m_text = wxS("ID");
  item.m_image = -1;
  lv_jobs->InsertColumn(0, item);
  item.m_text = wxS("Status");
  lv_jobs->InsertColumn(1, item);
  item.m_text = wxS("Added on");
  lv_jobs->InsertColumn(2, item);
  item.m_text = wxS("Started on");
  lv_jobs->InsertColumn(3, item);
  item.m_text = wxS("Finished on");
  lv_jobs->InsertColumn(4, item);
  item.m_text = wxS("Description");
  lv_jobs->InsertColumn(5, item);

  if (jobs.size() == 0) {
    dummy = lv_jobs->InsertItem(0, wxS("12345"));
    lv_jobs->SetItem(dummy, 1, wxS("aborted"));
    lv_jobs->SetItem(dummy, 2, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 3, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 4, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 5, wxS("2004-05-06 07:08:09"));

  } else {
    for (i = 0; i < jobs.size(); i++) {
      s.Printf(wxS("%d"), jobs[i].id);
      dummy = lv_jobs->InsertItem(i, s);

      s.Printf(wxS("%s"),
               jobs[i].status == jobs_pending ? wxS("pending") :
               jobs[i].status == jobs_done ? wxS("done") :
               jobs[i].status == jobs_aborted ? wxS("aborted") :
               wxS("failed"));
      lv_jobs->SetItem(dummy, 1, s);

      dt.Set((time_t)jobs[i].added_on);
      s.Printf(wxS("%04d-%02d-%02d %02d:%02d:%02d"), dt.GetYear(),
               dt.GetMonth(), dt.GetDay(), dt.GetHour(), dt.GetMinute(),
               dt.GetSecond());
      lv_jobs->SetItem(dummy, 2, s);

      if (jobs[i].started_on != -1) {
        dt.Set((time_t)jobs[i].started_on);
        s.Printf(wxS("%04d-%02d-%02d %02d:%02d:%02d"), dt.GetYear(),
                 dt.GetMonth(), dt.GetDay(), dt.GetHour(), dt.GetMinute(),
                 dt.GetSecond());
      } else
        s = wxS("                   ");
      lv_jobs->SetItem(dummy, 3, s);

      if (jobs[i].finished_on != -1) {
        dt.Set((time_t)jobs[i].finished_on);
        s.Printf(wxS("%04d-%02d-%02d %02d:%02d:%02d"), dt.GetYear(),
                 dt.GetMonth(), dt.GetDay(), dt.GetHour(), dt.GetMinute(),
                 dt.GetSecond());
      } else
        s = wxS("                   ");
      lv_jobs->SetItem(dummy, 4, s);

      s = *jobs[i].description;
      while (s.length() < 15)
        s += wxS(" ");
      lv_jobs->SetItem(dummy, 5, s);
    }
  }

  for (i = 0; i < 6; i++)
    lv_jobs->SetColumnWidth(i, wxLIST_AUTOSIZE);

  if (jobs.size() == 0)
    lv_jobs->DeleteItem(0);

  b_up = new wxButton(this, ID_JOBS_B_UP, wxS("&Up"), wxPoint(700, 40),
                      wxSize(80, -1));
  b_down = new wxButton(this, ID_JOBS_B_DOWN, wxS("&Down"), wxPoint(700, 70),
                        wxSize(80, -1));
  b_reenable = new wxButton(this, ID_JOBS_B_REENABLE, wxS("&Re-enable"),
                            wxPoint(700, 115), wxSize(80, -1));
  b_delete = new wxButton(this, ID_JOBS_B_DELETE, wxS("D&elete"),
                          wxPoint(700, 150), wxSize(80, -1));

  b_ok = new wxButton(this, ID_JOBS_B_OK, wxS("&Ok"), wxPoint(20, 355),
                      wxSize(100, -1));
  b_ok->SetDefault();
  b_start = new wxButton(this, ID_JOBS_B_START, wxS("&Start"),
                         wxPoint(460, 355), wxSize(100, -1));
  b_start_selected = new wxButton(this, ID_JOBS_B_START_SELECTED,
                                  wxS("S&tart selected"),
                                  wxPoint(580, 355), wxSize(100, -1));

  enable_buttons(false);

  b_start->Enable(jobs.size() > 0);

  ShowModal();
}

void
job_dialog::enable_buttons(bool enable) {
  b_up->Enable(enable);
  b_down->Enable(enable);
  b_reenable->Enable(enable);
  b_delete->Enable(enable);
  b_start_selected->Enable(enable);
}

void
job_dialog::on_ok(wxCommandEvent &evt) {
  Close();
}

void
job_dialog::on_start(wxCommandEvent &evt) {
}

void
job_dialog::on_start_selected(wxCommandEvent &evt) {
}

void
job_dialog::on_delete(wxCommandEvent &evt) {
  long item, i, n;
  vector<job_t>::iterator dit;
  vector<long> selected;

  item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1)
      break;
    selected.push_back(item);
    delete jobs[item].description;
  }

  if (selected.size() == 0)
    return;

  sort(selected.begin(), selected.end());
  dit = jobs.begin();
  n = 0;
  for (i = 0; i < jobs.size(); i++) {
    if (selected[0] == i) {
      jobs.erase(dit);
      selected.erase(selected.begin());
      lv_jobs->DeleteItem(n);
    } else {
      dit++;
      n++;
    }

    if (selected.size() == 0)
      break;
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_up(wxCommandEvent &evt) {
}

void
job_dialog::on_down(wxCommandEvent &evt) {
}

void
job_dialog::on_reenable(wxCommandEvent &evt) {
  long item;

  item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1)
      break;
    jobs[item].status = jobs_pending;
    lv_jobs->SetItem(item, 1, wxS("pending"));
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_item_selected(wxCommandEvent &evt) {
  enable_buttons(lv_jobs->GetSelectedItemCount() > 0);
}

IMPLEMENT_CLASS(job_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_OK, job_dialog::on_ok)
  EVT_BUTTON(ID_JOBS_B_START, job_dialog::on_start)
  EVT_BUTTON(ID_JOBS_B_START_SELECTED, job_dialog::on_start_selected)
  EVT_BUTTON(ID_JOBS_B_UP, job_dialog::on_up)
  EVT_BUTTON(ID_JOBS_B_DOWN, job_dialog::on_down)
  EVT_BUTTON(ID_JOBS_B_DELETE, job_dialog::on_delete)
  EVT_BUTTON(ID_JOBS_B_REENABLE, job_dialog::on_reenable)
  EVT_LIST_ITEM_SELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
  EVT_LIST_ITEM_DESELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
END_EVENT_TABLE();
