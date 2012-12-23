/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts attachments from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>
#include <iostream>

#include <avilib.h>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxSegment.h>

#include "common/ebml.h"
#include "common/kax_analyzer.h"
#include "common/mm_io_x.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

struct attachment_t {
  std::string name, type;
  int64_t size, id;
  KaxFileData *fdata;
  bool valid;

  attachment_t()
    : size(-1)
    , id(-1)
    , fdata(nullptr)
    , valid(false)
  {
  };

  attachment_t &parse(KaxAttached &att);
  static attachment_t parse_new(KaxAttached &att);
};

attachment_t
attachment_t::parse_new(KaxAttached &att) {
  attachment_t attachment;
  return attachment.parse(att);
}

attachment_t &
attachment_t::parse(KaxAttached &att) {
  size_t k;
  for (k = 0; att.ListSize() > k; ++k) {
    EbmlElement *e = att[k];

    if (EbmlId(*e) == EBML_ID(KaxFileName))
      name = static_cast<KaxFileName *>(e)->GetValueUTF8();

    else if (EbmlId(*e) == EBML_ID(KaxMimeType))
      type = static_cast<KaxMimeType *>(e)->GetValue();

    else if (EbmlId(*e) == EBML_ID(KaxFileUID))
      id = static_cast<KaxFileUID *>(e)->GetValue();

    else if (EbmlId(*e) == EBML_ID(KaxFileData)) {
      fdata = static_cast<KaxFileData *>(e);
      size  = fdata->GetSize();
    }
  }

  valid = (-1 != id) && (-1 != size) && !type.empty();

  return *this;
}

static void
handle_attachments(KaxAttachments *atts,
                   std::vector<track_spec_t> &tracks) {
  int64_t attachment_ui_id = 0;
  std::map<int64_t, attachment_t> attachments;

  size_t i;
  for (i = 0; atts->ListSize() > i; ++i) {
    KaxAttached *att = dynamic_cast<KaxAttached *>((*atts)[i]);
    assert(att);

    attachment_t attachment = attachment_t::parse_new(*att);
    if (!attachment.valid)
      continue;

    ++attachment_ui_id;
    attachments[attachment_ui_id] = attachment;
  }

  for (auto &track : tracks) {
    attachment_t attachment = attachments[ track.tid ];

    if (!attachment.valid)
      mxerror(boost::format(Y("An attachment with the ID %1% was not found.\n")) % track.tid);

    // check for output name
    if (track.out_name.empty())
      track.out_name = attachment.name;

    mxinfo(boost::format(Y("The attachment #%1%, ID %2%, MIME type %3%, size %4%, is written to '%5%'.\n"))
           % track.tid % attachment.id % attachment.type % attachment.size % track.out_name);
    try {
      mm_file_io_c out(track.out_name, MODE_CREATE);
      out.write(attachment.fdata->GetBuffer(), attachment.fdata->GetSize());
    } catch (mtx::mm_io::exception &ex) {
      mxerror(boost::format(Y("The file '%1%' could not be opened for writing: %2%.\n")) % track.out_name % ex);
    }
  }
}

void
extract_attachments(const std::string &file_name,
                    std::vector<track_spec_t> &tracks,
                    kax_analyzer_c::parse_mode_e parse_mode) {
  if (tracks.empty())
    mxerror(Y("Nothing to do.\n"));

  kax_analyzer_cptr analyzer;

  // open input file
  try {
    analyzer = kax_analyzer_cptr(new kax_analyzer_c(file_name));
    if (!analyzer->process(parse_mode, MODE_READ))
      throw false;
  } catch (mtx::mm_io::exception &ex) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading: %2%.\n")) % file_name % ex);
    return;
  }

  ebml_master_cptr attachments_m(analyzer->read_all(EBML_INFO(KaxAttachments)));
  KaxAttachments *attachments = dynamic_cast<KaxAttachments *>(attachments_m.get());
  if (attachments)
    handle_attachments(attachments, tracks);
}
