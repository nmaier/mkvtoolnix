/*
 * mkvextract -- extract tracks from Matroska files into other files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * extracts tags from Matroska files into other files
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
#include "quickparser.h"
#include "tagwriter.h"

using namespace libmatroska;
using namespace std;

void
extract_tags(const char *file_name,
             bool parse_fully) {
  EbmlMaster *m;
  mm_io_c *in;
  mm_stdio_c out;
  kax_quickparser_c *qp;
  KaxTags *tags;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
    qp = new kax_quickparser_c(*in, parse_fully);
  } catch (std::exception &ex) {
    show_error(_("The file '%s' could not be opened for reading (%s)."),
               file_name, strerror(errno));
    return;
  }

  m = qp->read_all(KaxTags::ClassInfos);
  if (m != NULL) {
    tags = dynamic_cast<KaxTags *>(m);
    assert(tags != NULL);

    if (verbose > 0)
      debug_dump_elements(tags, 0);

    out.write_bom("UTF-8");
    out.printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
               "<!DOCTYPE Tags SYSTEM \"matroskatags.dtd\">\n\n"
               "<Tags>\n");
    write_tags_xml(*tags, &out);
    out.printf("</Tags>\n");

    delete tags;
  }

  delete in;
  delete qp;
}
