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
#include "quickparser.h"

using namespace libmatroska;
using namespace std;

static void
handle_attachments(KaxAttachments *atts,
                   vector<track_spec_t> &tracks) {
  KaxAttached *att;
  KaxFileData *fdata;
  EbmlElement *e;
  int i, k;
  string name, type;
  int64_t size, id;
  bool found;
  mm_io_c *out;

  out = NULL;
  for (i = 0; i < atts->ListSize(); i++) {
    att = dynamic_cast<KaxAttached *>((*atts)[i]);
    assert(att != NULL);

    name = "";
    type = "";
    size = -1;
    id = -1;
    fdata = NULL;

    for (k = 0; k < att->ListSize(); k++) {
      e = (*att)[k];

      if (EbmlId(*e) == KaxFileName::ClassInfos.GlobalId) {
        KaxFileName &fname = *static_cast<KaxFileName *>(e);
        name = UTFstring_to_cstr(UTFstring(fname));

      } else if (EbmlId(*e) == KaxMimeType::ClassInfos.GlobalId) {
        KaxMimeType &mtype = *static_cast<KaxMimeType *>(e);
        type = string(mtype);

      } else if (EbmlId(*e) == KaxFileUID::ClassInfos.GlobalId) {
        KaxFileUID &fuid = *static_cast<KaxFileUID *>(e);
        id = uint32(fuid);

      } else if (EbmlId(*e) == KaxFileData::ClassInfos.GlobalId) {
        fdata = (KaxFileData *)e;
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
        mxinfo(_("The attachment #" LLD ", MIME type %s, size " LLD ", "
                 "is written to '%s'.\n"), id, type.c_str(), size,
               tracks[k].out_name);
        try {
          out = new mm_file_io_c(tracks[k].out_name, MODE_CREATE);
        } catch (...) {
          mxerror(_("The file '%s' could not be opened for writing "
                    "(%d, %s).\n"),
                  tracks[k].out_name, errno, strerror(errno));
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
  mm_stdio_c out;
  kax_quickparser_c *qp;
  KaxAttachments *attachments;
  int i;

  // open input file
  try {
    in = new mm_file_io_c(file_name);
    qp = new kax_quickparser_c(*in, parse_fully);
  } catch (...) {
    show_error(_("The file '%s' could not be opened for reading (%s)."),
               file_name, strerror(errno));
    return;
  }

  attachments =
    dynamic_cast<KaxAttachments *>(qp->read_all(KaxAttachments::ClassInfos));
  if (attachments != NULL) {
    if (verbose > 0)
      debug_dump_elements(attachments, 0);

    handle_attachments(attachments, tracks);

    delete attachments;
  }

  delete in;
  delete qp;

  for (i = 0; i < tracks.size(); i++)
    if (!tracks[i].done)
      mxinfo(_("An attachment with the ID " LLD " was not found.\n"),
             tracks[i].tid);
}
