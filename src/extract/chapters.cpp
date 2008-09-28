/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts chapters from Matroska files into other files
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
#include <cassert>
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

#include <matroska/KaxChapters.h>

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

  EbmlMaster *m = qp->read_all(KaxChapters::ClassInfos);
  if (NULL != m) {
    KaxChapters *chapters = dynamic_cast<KaxChapters *>(m);
    assert(chapters != NULL);

    mm_stdio_c out;
    if (!chapter_format_simple) {
      out.write_bom("UTF-8");
      out.puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "\n"
               "<!-- <!DOCTYPE Tags SYSTEM \"matroskatags.dtd\"> -->\n"
               "\n"
               "<Chapters>\n");
      write_chapters_xml(chapters, &out);
      out.puts("</Chapters>\n");

    } else {
      int dummy = 1;
      write_chapters_simple(dummy, chapters, &out);
    }

    delete chapters;
  }

  delete in;
  delete qp;
}
