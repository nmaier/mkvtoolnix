/*
 * mkvextract -- extract tracks from Matroska files into other files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * extracts chapters from Matroska files into other files
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

#include <matroska/KaxAttachments.h>
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

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"
#include "quickparser.h"

using namespace libmatroska;
using namespace std;

void
extract_chapters(const char *file_name,
                 bool chapter_format_simple,
                 bool parse_fully) {
  EbmlMaster *m;
  mm_io_c *in;
  mm_stdio_c out;
  kax_quickparser_c *qp;
  KaxChapters *chapters;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
    qp = new kax_quickparser_c(*in, parse_fully);
  } catch (std::exception &ex) {
    show_error(_("The file '%s' could not be opened for reading (%s)."),
               file_name, strerror(errno));
    return;
  }

  m = qp->read_all(KaxChapters::ClassInfos);
  if (m != NULL) {
    chapters = dynamic_cast<KaxChapters *>(m);
    assert(chapters != NULL);

    if (verbose > 0)
      debug_dump_elements(chapters, 0);

    if (!chapter_format_simple) {
      out.write_bom("UTF-8");
      out.printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                 "\n"
                 "<!-- <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> -->\n"
                 "\n"
                 "<Chapters>\n");
      write_chapters_xml(chapters, &out);
      out.printf("</Chapters>\n");
    } else {
      int dummy = 1;
      write_chapters_simple(dummy, chapters, &out);
    }

    delete chapters;
  }

  delete in;
  delete qp;
}
