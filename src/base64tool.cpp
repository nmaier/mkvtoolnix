/*
   base64util - Utility for encoding and decoding Base64 files.

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   command line parameter parsing, looping, output handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <errno.h>
#include <string.h>

#include <string>

#include "os.h"

#include "base64.h"
#include "common.h"
#include "mm_io.h"

using namespace std;

void
usage(int retval) {
  mxinfo(_(
    "base64util <encode|decode> <input> <output> [maxlen]\n"
    "\n"
    "  encode - Read from <input>, encode to Base64 and write to <output>.\n"
    "           Max line length can be specified and is 72 if left out.\n"
    "  decode - Read from <input>, decode to binary and write to <output>.\n"
    ));

  exit(retval);
}

int
main(int argc,
     char *argv[]) {
  int maxlen;
  uint64_t size;
  unsigned char *buffer;
  char mode;
  string s, line;
  mm_io_c *in, *out;
  mm_text_io_c *intext;

  if (argc < 4)
    usage(0);

  mode = 0;
  in = NULL;
  out = NULL;
  intext = NULL;
  if (!strcmp(argv[1], "encode"))
    mode = 'e';
  else if (!strcmp(argv[1], "decode"))
    mode = 'd';
  else
    mxerror(_("Invalid mode '%s'.\n"), argv[1]);

  maxlen = 72;
  if ((argc == 5) && (mode == 'e')) {
    if (!parse_int(argv[4], maxlen) || (maxlen < 4))
      mxerror(_("Max line length must be >= 4.\n\n"));
  } else if ((argc > 5) || ((argc > 4) && (mode == 'd')))
    usage(1);

  maxlen = ((maxlen + 3) / 4) * 4;

  try {
    in = new mm_file_io_c(argv[2]);
    if (mode != 'e')
      in = new mm_text_io_c(in);
  } catch(...) {
    mxerror(_("The file '%s' could not be opened for reading (%d, %s).\n"),
            argv[2], errno, strerror(errno));
  }

  try {
    out = new mm_file_io_c(argv[3], MODE_CREATE);
  } catch(...) {
    mxerror(_("The file '%s' could not be opened for writing (%d, %s).\n"),
            argv[3], errno, strerror(errno));
  }

  in->save_pos();
  in->setFilePointer(0, seek_end);
  size = in->getFilePointer();
  in->restore_pos();

  if (mode == 'e') {
    buffer = (unsigned char *)safemalloc(size);
    size = in->read(buffer, size);
    delete in;

    s = base64_encode(buffer, size, true, maxlen);
    safefree(buffer);

    out->write(s.c_str(), s.length());
    delete out;

  } else {

    while (intext->getline2(line)) {
      strip(line);
      s += line;
    }
    delete intext;

    buffer = (unsigned char *)safemalloc(s.length() / 4 * 3 + 100);
    try {
      size = base64_decode(s, buffer);
    } catch(...) {
      delete in;
      delete out;
      mxerror(_("The Base64 encoded data could not be decoded.\n"));
    }
    out->write(buffer, size);

    safefree(buffer);
    delete out;
  }

  mxinfo(_("Done.\n"));

  return 0;
}
