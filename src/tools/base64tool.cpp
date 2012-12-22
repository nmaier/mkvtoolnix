/*
   base64util - Utility for encoding and decoding Base64 files.

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   command line parameter parsing, looping, output handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <errno.h>
#include <string.h>

#include <string>

#include "common/base64.h"
#include "common/command_line.h"
#include "common/common_pch.h"
#include "common/mm_io_x.h"
#include "common/mm_write_buffer_io.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/version.h"

void
set_usage() {
  usage_text = Y(
    "base64util <encode|decode> <input> <output> [maxlen]\n"
    "\n"
    "  encode - Read from <input>, encode to Base64 and write to <output>.\n"
    "           Max line length can be specified and is 72 if left out.\n"
    "  decode - Read from <input>, decode to binary and write to <output>.\n"
    );

  version_info = get_version_info("base64util", vif_full);
}

int
main(int argc,
     char *argv[]) {
  int maxlen;
  uint64_t size;
  unsigned char *buffer;
  char mode;
  std::string s, line;

  mtx_common_init("base64tool");

  set_usage();

  if (argc < 4)
    usage(0);

  mode = 0;
  if (!strcmp(argv[1], "encode"))
    mode = 'e';
  else if (!strcmp(argv[1], "decode"))
    mode = 'd';
  else
    mxerror(boost::format(Y("Invalid mode '%1%'.\n")) % argv[1]);

  maxlen = 72;
  if ((argc == 5) && (mode == 'e')) {
    if (!parse_number(argv[4], maxlen) || (maxlen < 4))
      mxerror(Y("Max line length must be >= 4.\n\n"));
  } else if ((argc > 5) || ((argc > 4) && (mode == 'd')))
    usage(2);

  maxlen = ((maxlen + 3) / 4) * 4;

  mm_io_cptr in, intext;
  try {
    in = mm_io_cptr(new mm_file_io_c(argv[2]));
    if (mode != 'e')
      intext = mm_io_cptr(new mm_text_io_c(in.get(), false));
  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for reading: %2%.\n")) % argv[2] % ex);
  }

  mm_io_cptr out;
  try {
    out = mm_write_buffer_io_c::open(argv[3], 128 * 1024);
  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for writing: %2%.\n")) % argv[3] % ex);
  }

  in->save_pos();
  in->setFilePointer(0, seek_end);
  size = in->getFilePointer();
  in->restore_pos();

  if (mode == 'e') {
    buffer = (unsigned char *)safemalloc(size);
    size = in->read(buffer, size);

    s = base64_encode(buffer, size, true, maxlen);
    safefree(buffer);

    out->write(s.c_str(), s.length());

  } else {

    while (intext->getline2(line)) {
      strip(line);
      s += line;
    }

    buffer = (unsigned char *)safemalloc(s.length() / 4 * 3 + 100);
    try {
      size = base64_decode(s, buffer);
    } catch(...) {
      mxerror(Y("The Base64 encoded data could not be decoded.\n"));
    }
    out->write(buffer, size);

    safefree(buffer);
  }

  mxinfo(Y("Done.\n"));

  return 0;
}
