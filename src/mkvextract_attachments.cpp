/*
  mkvextract -- extract tracks from Matroska files into other files

  mkvextract_attachments.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief extracts attachments from Matroska files into other files
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

static void
handle_attachments(mm_io_c *in,
                   EbmlStream *es,
                   EbmlElement *l0,
                   int64_t pos) {
  KaxAttachments *atts;
  KaxAttached *att;
  KaxFileData *fdata;
  EbmlElement *l1, *l2;
  int upper_lvl_el, i, k;
  string name, type;
  int64_t size, id;
  char *str;
  bool found;
  mm_io_c *out;

  out = NULL;
  in->save_pos(pos);
  l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                           true);

  if ((l1 != NULL) && (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId)) {
    atts = (KaxAttachments *)l1;
    l2 = NULL;
    upper_lvl_el = 0;
    atts->Read(*es, KaxAttachments::ClassInfos.Context, upper_lvl_el, l2,
               true);
    for (i = 0; i < atts->ListSize(); i++) {
      att = (KaxAttached *)(*atts)[i];
      if (EbmlId(*att) == KaxAttached::ClassInfos.GlobalId) {
        name = "";
        type = "";
        size = -1;
        id = -1;
        fdata = NULL;

        for (k = 0; k < att->ListSize(); k++) {
          l2 = (*att)[k];

          if (EbmlId(*l2) == KaxFileName::ClassInfos.GlobalId) {
            KaxFileName &fname = *static_cast<KaxFileName *>(l2);
            str = UTFstring_to_cstr(UTFstring(fname));
            name = str;
            safefree(str);

          } else if (EbmlId(*l2) == KaxMimeType::ClassInfos.GlobalId) {
            KaxMimeType &mtype = *static_cast<KaxMimeType *>(l2);
            type = string(mtype);

          } else if (EbmlId(*l2) == KaxFileUID::ClassInfos.GlobalId) {
            KaxFileUID &fuid = *static_cast<KaxFileUID *>(l2);
            id = uint32(fuid);

          } else if (EbmlId(*l2) == KaxFileData::ClassInfos.GlobalId) {
            fdata = (KaxFileData *)l2;
            size = fdata->GetSize();

          }
        }

        if ((id != -1) && (size != -1) && (type.length() != 0)) {
          found = false;

          for (k = 0; k < tracks.size(); k++)
            if (tracks[k].tid == id) {
              found = true;
              break;
            }

          if (found && !tracks[k].done) {
            mxinfo("Writing attachment #%lld, type %s, size %lld, "
                   "to '%s'.\n", id, type.c_str(), size, tracks[k].out_name);
            try {
              out = new mm_io_c(tracks[k].out_name, MODE_WRITE);
            } catch (...) {
              mxerror("Could not create '%s' (%d, %s).\n",
                      tracks[k].out_name, errno, strerror(errno));
            }
            out->write(fdata->GetBuffer(), fdata->GetSize());
            delete out;
            tracks[k].done = true;
          }
        }
      }
    }

    delete l1;
  }

  in->restore_pos();
}

void
extract_attachments(const char *file_name) {
  int upper_lvl_el, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL;
  EbmlStream *es;
  mm_io_c *in;
  bool done;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error("Error: Couldn't open input file %s (%s).", file_name,
               strerror(errno));
    return;
  }

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error("Error: No EBML head found.");
      delete es;

      return;
    }
      
    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error("No segment/level 0 element found.");
        return;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, "Segment");
        break;
      }

      show_element(l0, 0, "Next level 0 element is not a segment but %s",
                   l0->Generic().DebugName);

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    done = false;

    // We've got our segment, so let's find the attachments
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {

      if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId) {
        handle_attachments(in, es, l0, l1->GetElementPosition());

        done = true;
        for (i = 0; i < tracks.size(); i++)
          if (!tracks[i].done) {
            done = false;
            break;
          }

        if (done)
          break;

      } else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        int k;
        EbmlElement *el;
        KaxSeekHead &seek_head = *static_cast<KaxSeekHead *>(l1);
        int64_t pos;
        bool is_attachments;

        i = 0;
        seek_head.Read(*es, KaxSeekHead::ClassInfos.Context, i, el, true);
        for (i = 0; i < seek_head.ListSize(); i++)
          if (EbmlId(*seek_head[i]) == KaxSeek::ClassInfos.GlobalId) {
            KaxSeek &seek = *static_cast<KaxSeek *>(seek_head[i]);
            pos = -1;
            is_attachments = false;

            for (k = 0; k < seek.ListSize(); k++)
              if (EbmlId(*seek[k]) == KaxSeekID::ClassInfos.GlobalId) {
                KaxSeekID &sid = *static_cast<KaxSeekID *>(seek[k]);
                EbmlId id(sid.GetBuffer(), sid.GetSize());
                if (id == KaxAttachments::ClassInfos.GlobalId)
                  is_attachments = true;

              } else if (EbmlId(*seek[k]) ==
                         KaxSeekPosition::ClassInfos.GlobalId)
                pos = uint64(*static_cast<KaxSeekPosition *>(seek[k]));

            if ((pos != -1) && is_attachments) {
              handle_attachments(in, es, l0,
                                 ((KaxSegment *)l0)->GetGlobalPosition(pos));
              done = true;
              for (k = 0; k < tracks.size(); k++)
                if (!tracks[k].done) {
                  done = false;
                  break;
                }

              if (done)
                break;
            }
          }


      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (done)
        break;

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

  } catch (exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;
  }

  for (i = 0; i < tracks.size(); i++)
    if (!tracks[i].done)
      mxinfo("An attachment with the ID %lld was not found.\n", tracks[i].tid);
}
