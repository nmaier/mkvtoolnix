/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  kax_analyzer.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Matroska file analyzer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <vector>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSeekHead.h>

#include "kax_analyzer.h"
#include "mmg.h"

using namespace std;
using namespace libebml;

// static void dumpme(vector<analyzer_data_c *> &data) {
//   uint32_t i;

//   mxinfo("DATA dump:\n");
//   for (i = 0; i < data.size(); i++)
//     mxinfo("%03d: 0x%08x at %lld, size %lld\n", i, data[i]->id.Value,
//            data[i]->pos, data[i]->size);
// }

kax_analyzer_c::kax_analyzer_c(wxWindow *nparent, string nname):
  wxDialog(nparent, -1, "Analysis running", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(300, 100),
#else
           wxSize(300, 100),
#endif
           wxCAPTION), file_name(nname), segment(NULL) {
  try {
    file = new mm_io_c(file_name.c_str(), MODE_SAFE);
  } catch (...) {
    throw error_c(string("Could not open the file " + file_name + "."));
  }

  new wxStaticText(this, -1, _("Analyzing Matroska file"), wxPoint(10, 10));
  g_progress =
    new wxGauge(this, -1, 100, wxPoint(10, 30), wxSize(280, -1));

  wxButton *b_abort =
    new wxButton(this, ID_B_ANALYZER_ABORT, _("Abort"), wxPoint(0, 0),
                 wxDefaultSize);
  b_abort->Move(wxPoint(150 - b_abort->GetSize().GetWidth() / 2, 60));
}

kax_analyzer_c::~kax_analyzer_c() {
  uint32_t i;

  delete file;
  for (i = 0; i < data.size(); i++)
    delete data[i];
  if (segment != NULL)
    delete segment;
}

bool kax_analyzer_c::probe(string file_name) {
  try {
    unsigned char data[4];
    mm_io_c in(file_name.c_str(), MODE_READ);

    if (in.read(data, 4) != 4)
      return false;
    if ((data[0] != 0x1A) || (data[1] != 0x45) ||
        (data[2] != 0xDF) || (data[3] != 0xA3))
      return false;
    return true;
  } catch (...) {
    return false;
  }
}

#define PFX "Not a valid Matroska file "
#define in_parent(p) (file->getFilePointer() < \
                      (p->GetElementPosition() + p->ElementSize()))

bool kax_analyzer_c::process() {
  int upper_lvl_el;
  uint32_t i;
  int64_t file_size;
  EbmlElement *l0, *l1;

  file->setFilePointer(0);
  EbmlStream es(*file);

  for (i = 0; i < data.size(); i++)
    delete data[i];
  data.clear();

  // Find the EbmlHead element. Must be the first one.
  l0 = es.FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
  if (l0 == NULL)
    throw error_c(PFX "(no EBML head found)");
      
  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL)
      throw error_c(PFX "(no segment/level 0 element found)");
    if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)
      break;
    l0->SkipData(es, l0->Generic().Context);
    delete l0;
  }

  file_size = file->get_size();
  g_progress->SetValue(0);
  Show(true);

  abort = false;
  segment = static_cast<KaxSegment *>(l0);
  upper_lvl_el = 0;
  // We've got our segment, so let's find all level 1 elements.
  l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el,
                          0xFFFFFFFFL, true, 1);
  while ((l1 != NULL) && (upper_lvl_el <= 0)) {
    data.push_back(new analyzer_data_c(l1->Generic().GlobalId,
                                       l1->GetElementPosition(),
                                       l1->ElementSize()));
    while (app->Pending())
      app->Dispatch();
    
    l1->SkipData(es, l1->Generic().Context);
    delete l1;
    l1 = NULL;

    g_progress->SetValue((int)(file->getFilePointer() * 100 / file_size));

    if (!in_parent(segment) || abort)
      break;

    while (app->Pending())
      app->Dispatch();

    l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el,
                            0xFFFFFFFFL, true);
  } // while (l1 != NULL)

  Show(false);

  if (l1 != NULL)
    delete l1;

  if (abort) {
    delete segment;
    segment = NULL;
    for (i = 0; i < data.size(); i++)
      delete data[i];
    data.clear();
    return false;
  }

  return true;
}

EbmlElement *kax_analyzer_c::read_element(uint32_t num,
                                          const EbmlCallbacks &callbacks) {
  EbmlElement *e;
  int upper_lvl_el;

  upper_lvl_el = 0;
  file->setFilePointer(data[num]->pos);
  EbmlStream es(*file);
  e = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                         true, 1);
  if ((e == NULL) || !(EbmlId(*e) == callbacks.GlobalId)) {
    if (e != NULL)
      delete e;
    return NULL;
  }
  upper_lvl_el = 0;
  e->Read(es, callbacks.Context, upper_lvl_el, e, true);

  return e;
}

void kax_analyzer_c::overwrite_elements(EbmlElement *e, int found_where) {
  vector<analyzer_data_c *>::iterator dit;
  string info;
  int64_t pos, size;
  uint32_t i, k;

  // 1. Overwrite the original element.
  info += string("Found a suitable place at ") +
    to_string(data[found_where]->pos) + string(".\n");
  file->setFilePointer(data[found_where]->pos);
  e->Render(*file);

  if (e->ElementSize() > data[found_where]->size) {
    int64_t new_size;
    EbmlVoid evoid;

    // 2a. Adjust any following EbmlVoid element if necessary.
    pos = file->getFilePointer();
    for (i = found_where + 1; pos >= (data[i]->pos + data[i]->size); i++) {
      data[i]->delete_this = true;
      info += string("Deleting internal element ") + to_string(i) +
        string(" at ") + to_string(data[i]->pos) + string(" with size ") +
        to_string(data[i]->size) + string(".\n");
    }
    new_size = (data[i]->pos + data[i]->size) - file->getFilePointer();
    info += string("Adjusting EbmlVoid element ") + to_string(i) +
      string(" at ") + to_string(data[i]->pos) + string(" with size ") +
      to_string(data[i]->size) + string(" to new pos ") +
      to_string(file->getFilePointer()) + string(" with new size ") +
      to_string(new_size) + string(".\n");
    data[i]->pos = file->getFilePointer();
    data[i]->size = new_size;
    evoid.SetSize(new_size);
    evoid.UpdateSize();
    evoid.SetSize(new_size - evoid.HeadSize());
    evoid.Render(*file);

  } else if (e->ElementSize() < data[found_where]->size) {
    // 2b. Insert a new EbmlVoid element.
    if ((data[found_where]->size - e->ElementSize()) < 5) {
      char zero[5] = {0, 0, 0, 0, 0};
      file->write(zero, data[found_where]->size - e->ElementSize());
      info += "Inserting new EbmlVoid not possible, remaining size "
        "too small.\n";
    } else {
      EbmlVoid evoid;
      info += string("Inserting new EbmlVoid element at ") +
        to_string(file->getFilePointer()) + string(" with size ") +
        to_string(data[found_where]->size - e->ElementSize());
      evoid.SetSize(data[found_where]->size - e->ElementSize());
      evoid.UpdateSize();
      evoid.SetSize(data[found_where]->size - e->ElementSize() -
                    evoid.HeadSize());
      evoid.Render(*file);
      dit = data.begin();
      dit += found_where + 1;
      data.insert(dit, new analyzer_data_c(evoid.Generic().GlobalId,
                                           evoid.GetElementPosition(),
                                           evoid.ElementSize()));
    }

  } else {
    info += "Great! Exact size. Just overwriting :)\n";
    file->setFilePointer(data[found_where]->pos);
    e->Render(*file);
  }
  data[found_where]->size = e->ElementSize();
  data[found_where]->id = e->Generic().GlobalId;

  // Glue EbmlVoid elements.
  i = 0;
  while (i < data.size()) {
    if (data[i]->id == EbmlVoid::ClassInfos.GlobalId) {
      EbmlVoid evoid;
      size = data[i]->size;
      for (k = i + 1; ((k < data.size()) &&
                       (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
        size += data[k]->size;
      if (size > data[i]->size) {
        evoid.SetSize(size);
        evoid.SetSize(size - evoid.HeadSize());
        file->setFilePointer(data[i]->pos);
        evoid.Render(*file);
        data[i]->size = size;
        while (data[i + 1]->id == EbmlVoid::ClassInfos.GlobalId) {
          dit = data.begin();
          dit += i + 1;
          data.erase(dit);
        }
      }
    }
    i++;
  }

//   mxinfo("overwrite_elements: %s", info.c_str());
}

bool kax_analyzer_c::update_element(EbmlElement *e) {
  uint32_t i, k, found_where, found_what;
  int64_t space_here, pos;
  vector<KaxSeekHead *> all_heads;
  vector<int64_t> free_space;
  vector<analyzer_data_c *>::iterator dit;
  KaxSegment *new_segment;
  KaxSeekHead *new_head;
  EbmlElement *new_e;
  string info;

  if (e == NULL)
    return false;

//   dumpme(data);

  for (i = 0; i < data.size(); i++)
    data[i]->delete_this = false;

  try {
    found_what = 0;
    for (i = 0; i < data.size(); i++)
      if (data[i]->id == EbmlId(*e)) {
        space_here = data[i]->size;
        for (k = i + 1; ((k < data.size()) &&
                         (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[k]->size;
        if (space_here >= e->ElementSize()) {
          found_what = 1;
          found_where = i;
          break;
        }
      }

    if (!found_what) {
      for (i = 0; i < data.size(); i++) {
        space_here = 0;
        for (k = i; ((k < data.size()) &&
                     (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
          space_here += data[k]->size;
        if (space_here >= e->ElementSize()) {
          found_what = 2;
          found_where = i;
          break;
        }
      }
    }

    if (!found_what) {
      info += "No suitable place found. Appending at the end.\n";
      // 1. Append e to the end of the file.
      file->setFilePointer(0, seek_end);
      e->Render(*file);

      // 2. Ajust the segment size.
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, 5);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  new_segment->HeadSize() -
                                  new_segment->GetElementPosition()) ||
          (new_segment->HeadSize() != segment->HeadSize())) {
        wxMessageBox(_("Wrote the element at the end of the file but could "
                       "not update the segment size. Therefore the element "
                       "will not be visible. Aborting the process. The file "
                       "has been changed!"), _("Error writing Matroska file"),
                     wxCENTER | wxOK | wxICON_ERROR);
        delete new_segment;
        return false;
      }
      delete segment;
      segment = new_segment;

      // 3. Overwrite the original element(s) if any were found.
      for (i = 0; i < data.size(); i++)
        if (data[i]->id == EbmlId(*e)) {
          EbmlVoid evoid;
          file->setFilePointer(data[i]->pos);
          info += string("Overwriting/voiding element at ") +
            to_string(data[i]->pos) + string(".\n");
          evoid.SetSize(data[i]->size);
          evoid.UpdateSize();
          evoid.SetSize(data[i]->size - evoid.HeadSize());
          evoid.Render(*file);
          data[i]->id = EbmlVoid::ClassInfos.GlobalId;
        }
      data.push_back(new analyzer_data_c(e->Generic().GlobalId,
                                         e->GetElementPosition(),
                                         e->ElementSize()));
      found_where = data.size() - 1;

    } else
      overwrite_elements(e, found_where);

    // Remove the internal elements. Avoids re-analyzing the file.
    dit = data.begin();
    while (dit < data.end()) {
      if ((*dit)->delete_this) {
        delete *dit;
        data.erase(dit);
      } else
        dit++;
    }

//     mxinfo("INFO:\n%s", info.c_str());
//     dumpme(data);

    // 1. Find all seek heads.
    free_space.clear();
    for (i = 0; i < data.size(); i++) {
      if (data[i]->id == KaxSeekHead::ClassInfos.GlobalId) {
        new_e = read_element(i, KaxSeekHead::ClassInfos);
        if (new_e != NULL) {
          all_heads.push_back(static_cast<KaxSeekHead *>(new_e));
          space_here = data[i]->size;
          for (k = i + 1; ((k < data.size()) &&
                           (data[k]->id == EbmlVoid::ClassInfos.GlobalId));
               k++)
            space_here += data[k]->size;
          free_space.push_back(space_here);
        }
      }
    }

    // 2. Look for a seek head that already pointed to the element that was
    //    overwritten.
    if (found_what == 1) {
      for (i = 0; i < all_heads.size(); i++) {
        for (k = 0; k < all_heads[i]->ListSize(); k++) {
          KaxSeek *seek;
          KaxSeekID *sid;
          KaxSeekPosition *spos;
          seek = static_cast<KaxSeek *>((*all_heads[i])[k]);
          sid = FindChild<KaxSeekID>(*seek);
          spos = FindChild<KaxSeekPosition>(*seek);
          if ((sid != NULL) && (spos != NULL) &&
              (segment->GetGlobalPosition(uint64(*spos)) ==
               data[found_where]->pos)) {
            EbmlId this_id(sid->GetBuffer(), sid->GetSize());
            if (this_id == EbmlId(*e))
              throw true;
          }
        }
      }
    }

    // 3. If none found: Look for a seek head that has enough space available
    //    (maybe with EbmlVoids) to contain another entry.
    found_what = 0;
    for (i = 0; i < all_heads.size(); i++) {
      KaxSeekHead *head;
      head = all_heads[i];
      head->IndexThis(*e, *segment);
      head->UpdateSize();
      if (head->ElementSize() <= free_space[i]) {
        for (k = 0; k < data.size(); k++)
          if (data[k]->pos == all_heads[i]->GetElementPosition()) {
            found_what = i + 1;
            found_where = k;
            break;
          }
        if (found_what == 0)
          mxerror("found_what == 0. Should not have happened. Please file a "
                  "bug report.\n");
        break;
      } else {
        delete (*head)[head->ListSize() - 1];
        head->Remove(head->ListSize() - 1);
      }
    }

    if (found_what) {
      overwrite_elements(all_heads[found_what - 1], found_where);
      throw true;
    }

    // 4. If none found: Look for the first seek head. Link to a new seek head
    //    entry at the end of the file which will contain all the entries the
    //    original seek head contained + the link to the updated element.
    //    Ajust segment size.
    if (all_heads.size() > 0) {
      info += "Appending new seek head.\n";
      // Append the new seek head to the end of the file.
      pos = all_heads[0]->GetElementPosition();
      printf("POS: %lld\n", pos);
      new_head = new KaxSeekHead;
      file->setFilePointer(0, seek_end);
      all_heads[0]->IndexThis(*e, *segment);
      all_heads[0]->UpdateSize();
      all_heads[0]->Render(*file);
      data.push_back(new analyzer_data_c(all_heads[0]->Generic().GlobalId,
                                         all_heads[0]->GetElementPosition(),
                                         all_heads[0]->ElementSize()));

      // Adjust the segment size.
      info += "Adjusting segment size.\n";
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, 5);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  new_segment->HeadSize() -
                                  new_segment->GetElementPosition()) ||
          (new_segment->HeadSize() != segment->HeadSize())) {
        wxMessageBox(_("Wrote the meta seek element at the end of the file "
                       "but could not update the segment size. Therefore the "
                       "element will not be visible. Aborting the process. "
                       "The file has been changed!"),
                     _("Error writing Matroska file"),
                     wxCENTER | wxOK | wxICON_ERROR);
        delete new_segment;
        throw false;
      }
      delete segment;
      segment = new_segment;

      // Create a new seek head to replace the old one.
      info += "Creating new seek head.\n";
      new_head->IndexThis(*all_heads[0], *segment);
      found_what = 0;
      for (i = 0; i < data.size(); i++)
        if (data[i]->pos == pos) {
          found_what = 1;
          found_where = i;
        }
      if (!found_what)
          mxerror("found_what == 0, 2nd time. Should not have happened. "
                  "Please file a bug report.\n");
      overwrite_elements(new_head, found_where);

      all_heads.push_back(all_heads[0]);
      all_heads.erase(all_heads.begin());
      all_heads.insert(all_heads.begin(), new_head);
//       mxinfo("MORE INFO:\n%s", info.c_str());
//       dumpme(data);

      throw true;
    }

    // 5. No seek head found at all: Search for enough space to create a
    //    seek head before the first cluster.
    new_head = new KaxSeekHead;
    new_head->IndexThis(*e, *segment);
    new_head->UpdateSize();
    found_what = 0;
    for (i = 0; ((i < data.size()) &&
                 !(data[i]->id == KaxCluster::ClassInfos.GlobalId)); i++) {
      space_here = 0;
      for (k = i; ((k < data.size()) &&
                   (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
        space_here += data[k]->size;
      if (space_here >= new_head->ElementSize()) {
        found_what = 1;
        found_where = i;
        break;
      }
    }
    if (found_what) {
      all_heads.insert(all_heads.begin(), new_head);
      overwrite_elements(new_head, found_where);
//       dumpme(data);

      throw true;
    } else
      delete new_head;

    // 6. If no seek head found at all: Issue a warning that the element might
    //    not be found by players.
    wxMessageBox(_("Wrote to the Matroska file, but the meta seek entry "
                   "could not be updated. This might mean that players will "
                   "have a hard time finding this element. Please use your "
                   "favorite player to check this file.\n\nThe proper "
                   "solution is to save these chapters into a XML file and "
                   "then to remux the file with the chapters included."),
                 _("File structure warning"), wxOK | wxCENTER |
                 wxICON_EXCLAMATION);
  } catch (bool b) {
    for (i = 0; i < all_heads.size(); i++)
      delete all_heads[i];
    all_heads.clear();

//     mxinfo("DUMP after work:\n");
//     dumpme(data);
//     process();
//     mxinfo("DUMP after re-process()ing:\n");
//     dumpme(data);

    return b;
  }

  for (i = 0; i < all_heads.size(); i++)
    delete all_heads[i];
  all_heads.clear();

  return false;
}

void kax_analyzer_c::on_abort(wxCommandEvent &evt) {
  abort = true;
}

IMPLEMENT_CLASS(kax_analyzer_c, wxDialog);
BEGIN_EVENT_TABLE(kax_analyzer_c, wxDialog)
  EVT_BUTTON(ID_B_ANALYZER_ABORT, kax_analyzer_c::on_abort)
END_EVENT_TABLE();
