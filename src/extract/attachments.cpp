/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts attachments from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cassert>
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
#include <matroska/KaxSegment.h>

#include "common.h"
#include "commonebml.h"
#include "mkvextract.h"
#include "mm_io.h"
#include "quickparser.h"

using namespace libmatroska;
using namespace std;

static void
handle_attachments(KaxAttachments *atts,
                   vector<track_spec_t> &tracks) {
  int i;
  for (i = 0; atts->ListSize() > i; ++i) {
    KaxAttached  *att = dynamic_cast<KaxAttached *>((*atts)[i]);
    assert(NULL != att);

    string name, type;
    int64_t size       = -1;
    int64_t id         = -1;
    KaxFileData *fdata = NULL;

    int k;
    for (k = 0; att->ListSize() > k; ++k) {
      EbmlElement *e = (*att)[k];

      if (EbmlId(*e) == KaxFileName::ClassInfos.GlobalId)
        name = UTFstring_to_cstr(UTFstring(*static_cast<KaxFileName *>(e)));

      else if (EbmlId(*e) == KaxMimeType::ClassInfos.GlobalId)
        type = string(*static_cast<KaxMimeType *>(e));

      else if (EbmlId(*e) == KaxFileUID::ClassInfos.GlobalId)
        id = uint32(*static_cast<KaxFileUID *>(e));

      else if (EbmlId(*e) == KaxFileData::ClassInfos.GlobalId) {
        fdata = (KaxFileData *)e;
        size  = fdata->GetSize();
      }

    }

    if ((-1 != id) && (-1 != size) && !type.empty()) {
      bool found = false;

      for (k = 0; k < tracks.size(); k++)
        if (tracks[k].tid == id) {
          found = true;
          break;
        }

      if (found && !tracks[k].done) {
        // check for output name
        if (strlen(tracks[k].out_name) == 0) {
          safefree(tracks[k].out_name);
          tracks[k].out_name = safestrdup(name.c_str());
        }

        mxinfo(boost::format(Y("The attachment #%1%, MIME type %2%, size %3%, is written to '%4%'.\n")) % id % type % size % tracks[k].out_name);
        mm_io_c *out = NULL;
        try {
          out = new mm_file_io_c(tracks[k].out_name, MODE_CREATE);
        } catch (...) {
          mxerror(boost::format(Y("The file '%1%' could not be opened for writing (%2%, %3%).\n")) % tracks[k].out_name % errno % strerror(errno));
        }
        out->write(fdata->GetBuffer(), fdata->GetSize());
        delete out;
        tracks[k].done = true;
      }
    }
  }
}

void
extract_attachments(const char *file_name,
                    vector<track_spec_t> &tracks,
                    bool parse_fully) {
  mm_io_c *in;
  kax_quickparser_c *qp;

  // open input file
  try {
    in = new mm_file_io_c(file_name);
    qp = new kax_quickparser_c(*in, parse_fully);
  } catch (...) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading (%2%).")) % file_name % strerror(errno));
    return;
  }

  KaxAttachments *attachments = dynamic_cast<KaxAttachments *>(qp->read_all(KaxAttachments::ClassInfos));
  if (attachments != NULL) {
    handle_attachments(attachments, tracks);
    delete attachments;
  }

  delete in;
  delete qp;

  int i;
  for (i = 0; i < tracks.size(); i++)
    if (!tracks[i].done)
      mxinfo(boost::format(Y("An attachment with the ID %1% was not found.\n")) % tracks[i].tid);
}
