/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

// The Debian g++ 3.3.1 has problems in its standard C++ headers with min
// being defined differently. So just include these files now when min
// has not been defined yet.
#if __GNUC__ != 2
#include <limits>
#endif
#include <iostream>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSeekHead.h>

#include "kax_analyzer.h"
#include "mmg.h"

using namespace std;
using namespace libebml;

// static void
// dump_analyzer_data(vector<analyzer_data_c *> &data) {
//   int i;

//   mxinfo("DATA dump:\n");
//   for (i = 0; i < data.size(); i++)
//     mxinfo(boost::format("%|1$04d|: 0x%|2$08x| at %3% size %4%\n") % i % data[i]->id.Value % data[i]->pos % data[i]->size);
// }

kax_analyzer_dlg_c::kax_analyzer_dlg_c(wxWindow *_parent,
                                       mm_io_c *_file,
                                       vector<analyzer_data_c *> &_data):
  wxDialog(_parent, -1, Z("Analysis running"), wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(300, 130),
#else
           wxSize(300, 100),
#endif
           wxCAPTION),
  data(_data),
  file(_file),
  segment(NULL) {

  new wxStaticText(this, -1, Z("Analyzing Matroska file"), wxPoint(10, 10));
  g_progress = new wxGauge(this, -1, 100, wxPoint(10, 30), wxSize(280, -1));

  wxButton *b_abort = new wxButton(this, ID_B_ANALYZER_ABORT, Z("Abort"), wxPoint(0, 0), wxDefaultSize);
#if defined(SYS_WINDOWS)
  b_abort->Move(wxPoint(150 - b_abort->GetSize().GetWidth() / 2, 70));
#else
  b_abort->Move(wxPoint(150 - b_abort->GetSize().GetWidth() / 2, 60));
#endif
}

#define in_parent(p) (file->getFilePointer() < (p->GetElementPosition() + p->ElementSize()))

bool
kax_analyzer_dlg_c::process() {
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
    throw error_c(Y("Not a valid Matroska file (no EBML head found)"));

  // Don't verify its data for now.
  l0->SkipData(es, l0->Generic().Context);
  delete l0;

  while (1) {
    // Next element must be a segment
    l0 = es.FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
    if (l0 == NULL)
      throw error_c(Y("Not a valid Matroska file (no segment/level 0 element found)"));
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
  l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFFFFFFFFFLL, true, 1);
  while ((l1 != NULL) && (upper_lvl_el <= 0)) {
    data.push_back(new analyzer_data_c(l1->Generic().GlobalId, l1->GetElementPosition(), l1->ElementSize(true)));
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

    l1 = es.FindNextElement(segment->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
  } // while (l1 != NULL)

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

void
kax_analyzer_dlg_c::on_abort(wxCommandEvent &evt) {
  abort = true;
}

kax_analyzer_c::kax_analyzer_c(wxWindow *_parent,
                               string _name):
  file_name(_name),
  segment(NULL) {
  try {
    file = new mm_file_io_c(file_name.c_str(), MODE_WRITE);
  } catch (...) {
    throw error_c(boost::format(Y("Could not open the file '%1%'.")) % file_name);
  }

  dlg = new kax_analyzer_dlg_c(_parent, file, data);
}

kax_analyzer_c::~kax_analyzer_c() {
  uint32_t i;

  delete file;
  for (i = 0; i < data.size(); i++)
    delete data[i];
  if (segment != NULL)
    delete segment;
}

bool
kax_analyzer_c::probe(string file_name) {
  try {
    unsigned char data[4];
    mm_file_io_c in(file_name.c_str());

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

bool
kax_analyzer_c::process() {
  bool result;

  if (dlg == NULL)
    return false;

  result = dlg->process();
  segment = dlg->segment;
  dlg->Destroy();

  return result;
}

EbmlElement *
kax_analyzer_c::read_element(analyzer_data_c *element_data,
                             const EbmlCallbacks &callbacks) {
  EbmlElement *e;
  int upper_lvl_el;

  upper_lvl_el = 0;
  file->setFilePointer(element_data->pos);
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

void
kax_analyzer_c::overwrite_elements(EbmlElement *e,
                                   int found_where) {
  vector<analyzer_data_c *>::iterator dit;
  string info;
  int64_t pos, size;
  uint32_t i, k;

  // 1. Overwrite the original element.
  info += (boost::format(Y("Found a suitable place at %1%.\n")) % data[found_where]->pos).str();
  file->setFilePointer(data[found_where]->pos);
  e->Render(*file);

  if (e->ElementSize() > data[found_where]->size) {
    int64_t new_size;
    EbmlVoid evoid;

    // 2a. Adjust any following EbmlVoid element if necessary.
    pos = file->getFilePointer();
    for (i = found_where + 1; pos >= (data[i]->pos + data[i]->size); i++) {
      data[i]->delete_this = true;
      info += (boost::format(Y("Deleting internal element %1% at %2% with size %3%.\n")) % i % data[i]->pos % data[i]->size).str();
    }
    new_size = (data[i]->pos + data[i]->size) - file->getFilePointer();
    info += (boost::format(Y("Adjusting EbmlVoid element %1% at %2% with size %3% to new pos %4% with new size %5%.\n"))
             % i % data[i]->pos % data[i]->size % file->getFilePointer() % new_size).str();
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
      info += Y("Inserting new EbmlVoid not possible, remaining size too small.\n");
    } else {
      EbmlVoid evoid;
      info += (boost::format(Y("Inserting new EbmlVoid element at %1% with size %2%.\n")) % file->getFilePointer() % (data[found_where]->size - e->ElementSize())).str();
      evoid.SetSize(data[found_where]->size - e->ElementSize());
      evoid.UpdateSize();
      evoid.SetSize(data[found_where]->size - e->ElementSize() - evoid.HeadSize());
      evoid.Render(*file);
      dit = data.begin();
      dit += found_where + 1;
      data.insert(dit, new analyzer_data_c(evoid.Generic().GlobalId, evoid.GetElementPosition(), evoid.ElementSize()));
    }

  } else {
    info += Y("Great! Exact size. Just overwriting :)\n");
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
        while (((i + 1) < data.size()) &&
               (data[i + 1]->id == EbmlVoid::ClassInfos.GlobalId))
          data.erase(data.begin() + i + 1);
      }
    }
    i++;
  }

//   mxinfo("overwrite_elements: %s", info.c_str());
}

bool
kax_analyzer_c::update_element(EbmlElement *e) {
  uint32_t i, k, found_where, found_what;
  int64_t space_here, pos;
  vector<KaxSeekHead *> all_heads;
  vector<int64_t> free_space;
  KaxSegment *new_segment;
  KaxSeekHead *new_head;
  EbmlElement *new_e;
  string info;

  if (e == NULL)
    return false;

  found_where = 0;
  for (i = 0; i < data.size(); i++)
    data[i]->delete_this = false;

  try {
//     mxinfo(boost::format("INFO 0:\n"));
//     dump_analyzer_data(data);

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
      info += Y("No suitable place found. Appending at the end.\n");
      // 1. Append e to the end of the file.
      file->setFilePointer(0, seek_end);
      e->Render(*file);

      // 2. Ajust the segment size.
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, segment->HeadSize() - 4);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  segment->HeadSize() -
                                  segment->GetElementPosition())) {
        segment->OverwriteHead(*file);
        wxMessageBox(Z("The element was written at the end of the file, but the segment size could not be updated. "
                       "Therefore the element will not be visible. "
                       "The process will be aborted. "
                       "The file has been changed!"),
                     Z("Error writing Matroska file"),
                     wxCENTER | wxOK | wxICON_ERROR);
        delete new_segment;
        return false;
      }
      new_segment->OverwriteHead(*file);
      delete segment;
      segment = new_segment;

      // 3. Overwrite the original element(s) if any were found.
      for (i = 0; i < data.size(); i++)
        if (data[i]->id == EbmlId(*e)) {
          EbmlVoid evoid;
          file->setFilePointer(data[i]->pos);
          info += (boost::format(Y("Overwriting/voiding element at %1%.\n")) % data[i]->pos).str();
          evoid.SetSize(data[i]->size);
          evoid.UpdateSize();
          evoid.SetSize(data[i]->size - evoid.HeadSize());
          evoid.Render(*file);
          data[i]->id = EbmlVoid::ClassInfos.GlobalId;
        }
      data.push_back(new analyzer_data_c(e->Generic().GlobalId, e->GetElementPosition(), e->ElementSize()));
      found_where = data.size() - 1;

    } else
      overwrite_elements(e, found_where);

    // Remove the internal elements. Avoids re-analyzing the file.
    i = 0;
    while (data.size() > i) {
      if (data[i]->delete_this) {
        delete data[i];
        data.erase(data.begin() + i);
      } else
        ++i;
    }

//     mxinfo(boost::format("INFO 1:\n%1%\n") % info);
//     dump_analyzer_data(data);

    // 1. Find all seek heads.
    free_space.clear();
    for (i = 0; i < data.size(); i++) {
      if (data[i]->id == KaxSeekHead::ClassInfos.GlobalId) {
        new_e = read_element(i, KaxSeekHead::ClassInfos);
        if (new_e != NULL) {
          all_heads.push_back(static_cast<KaxSeekHead *>(new_e));
          space_here = data[i]->size;
          for (k = i + 1; ((k < data.size()) && (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
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
          if ((sid != NULL) && (spos != NULL) && (segment->GetGlobalPosition(uint64(*spos)) == data[found_where]->pos)) {
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
          mxerror(Y("found_what == 0. Should not have happened. Please file a bug report.\n"));
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
      info += Y("Appending new seek head.\n");
      // Append the new seek head to the end of the file.
      pos = all_heads[0]->GetElementPosition();
      new_head = new KaxSeekHead;
      file->setFilePointer(0, seek_end);
      all_heads[0]->IndexThis(*e, *segment);
      all_heads[0]->UpdateSize();
      all_heads[0]->Render(*file);
      data.push_back(new analyzer_data_c(all_heads[0]->Generic().GlobalId, all_heads[0]->GetElementPosition(), all_heads[0]->ElementSize()));

      // Adjust the segment size.
      info += Y("Adjusting segment size.\n");
      new_segment = new KaxSegment;
      file->setFilePointer(segment->GetElementPosition());
      new_segment->WriteHead(*file, segment->HeadSize() - 4);
      file->setFilePointer(0, seek_end);
      if (!new_segment->ForceSize(file->getFilePointer() -
                                  segment->HeadSize() -
                                  segment->GetElementPosition())) {
        segment->OverwriteHead(*file);
        wxMessageBox(Z("The meta seek element was written at the end of the file, but the segment size could not be updated. "
                       "Therefore the element will not be visible. "
                       "The process will be aborted. "
                       "The file has been changed!"),
                     Z("Error writing Matroska file"),
                     wxCENTER | wxOK | wxICON_ERROR);
        delete new_segment;
        throw false;
      }
      new_segment->OverwriteHead(*file);
      delete segment;
      segment = new_segment;

      // Create a new seek head to replace the old one.
      info += Y("Creating new seek head.\n");
      new_head->IndexThis(*all_heads[0], *segment);
      found_what = 0;
      for (i = 0; i < data.size(); i++)
        if (data[i]->pos == pos) {
          found_what = 1;
          found_where = i;
        }
      if (!found_what)
        mxerror(Y("found_what == 0, 2nd time. Should not have happened. Please file a bug report.\n"));
      overwrite_elements(new_head, found_where);

      all_heads.push_back(all_heads[0]);
      all_heads.erase(all_heads.begin());
      all_heads.insert(all_heads.begin(), new_head);

//       mxinfo(boost::format("INFO 2:\n%1%\n") % info);
//       dump_analyzer_data(data);

      throw true;
    }

    // 5. No seek head found at all: Search for enough space to create a
    //    seek head before the first cluster.
    new_head = new KaxSeekHead;
    new_head->IndexThis(*e, *segment);
    new_head->UpdateSize();
    found_what = 0;
    for (i = 0; ((i < data.size()) && !(data[i]->id == KaxCluster::ClassInfos.GlobalId)); i++) {
      space_here = 0;
      for (k = i; ((k < data.size()) && (data[k]->id == EbmlVoid::ClassInfos.GlobalId)); k++)
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
//       dump_analyzer_data(data);

      throw true;
    } else
      delete new_head;

    // 6. If no seek head found at all: Issue a warning that the element might
    //    not be found by players.
    wxMessageBox(Z("The Matroska file was modified, but the meta seek entry could not be updated. "
                   "This means that players might have a hard time finding this element. "
                   "Please use your favorite player to check this file.\n\n"
                   "The proper solution is to save these chapters into a XML file and then to remux the file with the chapters included."),
                 Z("File structure warning"),
                 wxOK | wxCENTER | wxICON_EXCLAMATION);
  } catch (bool b) {
    for (i = 0; i < all_heads.size(); i++)
      delete all_heads[i];
    all_heads.clear();

//     mxinfo("DUMP after work:\n");
//     dump_analyzer_data(data);
//     process();
//     mxinfo("DUMP after re-process()ing:\n");
//     dump_analyzer_data(data);

    return b;
  }

  for (i = 0; i < all_heads.size(); i++)
    delete all_heads[i];
  all_heads.clear();

  return false;
}

IMPLEMENT_CLASS(kax_analyzer_dlg_c, wxDialog);
BEGIN_EVENT_TABLE(kax_analyzer_dlg_c, wxDialog)
  EVT_BUTTON(ID_B_ANALYZER_ABORT, kax_analyzer_dlg_c::on_abort)
END_EVENT_TABLE();
