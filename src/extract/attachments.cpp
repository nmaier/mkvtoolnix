/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts attachments from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

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

#include "common/common.h"
#include "common/ebml.h"
#include "common/kax_analyzer.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

static void
handle_attachments(KaxAttachments *atts,
                   std::vector<track_spec_t> &tracks) {
  static int64_t attachment_ui_id = 0;

  int i;
  for (i = 0; atts->ListSize() > i; ++i) {
    KaxAttached  *att = dynamic_cast<KaxAttached *>((*atts)[i]);
    assert(NULL != att);

    std::string name, type;
    int64_t size       = -1;
    int64_t id         = -1;
    KaxFileData *fdata = NULL;

    int k;
    for (k = 0; att->ListSize() > k; ++k) {
      EbmlElement *e = (*att)[k];

      if (EbmlId(*e) == KaxFileName::ClassInfos.GlobalId)
        name = UTFstring_to_cstrutf8(UTFstring(*static_cast<KaxFileName *>(e)));

      else if (EbmlId(*e) == KaxMimeType::ClassInfos.GlobalId)
        type = std::string(*static_cast<KaxMimeType *>(e));

      else if (EbmlId(*e) == KaxFileUID::ClassInfos.GlobalId)
        id = uint32(*static_cast<KaxFileUID *>(e));

      else if (EbmlId(*e) == KaxFileData::ClassInfos.GlobalId) {
        fdata = (KaxFileData *)e;
        size  = fdata->GetSize();
      }

    }

    if ((-1 != id) && (-1 != size) && !type.empty()) {
      ++attachment_ui_id;

      bool found = false;

      for (k = 0; k < tracks.size(); k++)
        if (tracks[k].tid == attachment_ui_id) {
          found = true;
          break;
        }

      if (found && !tracks[k].done) {
        // check for output name
        if (tracks[k].out_name.empty())
          tracks[k].out_name = name;

        mxinfo(boost::format(Y("The attachment #%1%, ID %2%, MIME type %3%, size %4%, is written to '%5%'.\n")) % attachment_ui_id % id % type % size % tracks[k].out_name);
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
extract_attachments(const std::string &file_name,
                    std::vector<track_spec_t> &tracks,
                    kax_analyzer_c::parse_mode_e parse_mode) {
  kax_analyzer_cptr analyzer;

  // open input file
  try {
    analyzer = kax_analyzer_cptr(new kax_analyzer_c(file_name));
    analyzer->process(parse_mode);
  } catch (...) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading (%2%).")) % file_name % strerror(errno));
    return;
  }

  KaxAttachments *attachments = dynamic_cast<KaxAttachments *>(analyzer->read_all(KaxAttachments::ClassInfos));
  if (NULL != attachments) {
    handle_attachments(attachments, tracks);
    delete attachments;
  }

  int i;
  for (i = 0; i < tracks.size(); i++)
    if (!tracks[i].done)
      mxinfo(boost::format(Y("An attachment with the ID %1% was not found.\n")) % tracks[i].tid);
}
