/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tags from Matroska files into other files

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

#include <matroska/KaxTags.h>

#include "common.h"
#include "commonebml.h"
#include "mkvextract.h"
#include "mm_io.h"
#include "quickparser.h"
#include "tagwriter.h"

using namespace libmatroska;
using namespace std;

void
extract_tags(const char *file_name,
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

  EbmlMaster *m = qp->read_all(KaxTags::ClassInfos);
  if (NULL != m) {
    KaxTags *tags = dynamic_cast<KaxTags *>(m);
    assert(NULL != tags);

    mm_stdio_c out;
    out.write_bom("UTF-8");
    out.puts("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n"
             "<!DOCTYPE Tags SYSTEM \"matroskatags.dtd\">\n\n"
             "<Tags>\n");
    write_tags_xml(*tags, &out);
    out.puts("</Tags>\n");

    delete tags;
  }

  delete in;
  delete qp;
}
