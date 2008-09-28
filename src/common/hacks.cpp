/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   hacks :)

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <string.h>

#include <string>
#include <vector>

using namespace std;

#include "base64.h"
#include "common.h"
#include "hacks.h"

static const char *mkvtoolnix_hacks[] = {
  ENGAGE_SPACE_AFTER_CHAPTERS,
  ENGAGE_NO_CHAPTERS_IN_META_SEEK,
  ENGAGE_NO_META_SEEK,
  ENGAGE_LACING_XIPH,
  ENGAGE_LACING_EBML,
  ENGAGE_NATIVE_MPEG4,
  ENGAGE_NO_VARIABLE_DATA,
  ENGAGE_NO_DEFAULT_HEADER_VALUES,
  ENGAGE_FORCE_PASSTHROUGH_PACKETIZER,
  ENGAGE_WRITE_HEADERS_TWICE,
  ENGAGE_ALLOW_AVC_IN_VFW_MODE,
  ENGAGE_KEEP_BITSTREAM_AR_INFO,
  ENGAGE_USE_SIMPLE_BLOCK,
  ENGAGE_OLD_AAC_CODECID,
  ENGAGE_USE_CODEC_STATE,
  ENGAGE_ENABLE_TIMECODE_WARNING,
  NULL
};
static vector<string> engaged_hacks;

bool
hack_engaged(const string &hack) {
  vector<string>::const_iterator hit;

  mxforeach(hit, engaged_hacks)
    if (*hit == hack)
      return true;

  return false;
}

void
engage_hacks(const string &hacks) {
  vector<string> engage_args;
  int aidx, hidx;
  bool valid_hack;

  engage_args = split(hacks, ",");
  for (aidx = 0; aidx < engage_args.size(); aidx++)
    if (engage_args[aidx] == "list") {
      mxinfo(Y("Valid hacks are:\n"));
      for (hidx = 0; mkvtoolnix_hacks[hidx] != NULL; hidx++)
        mxinfo(boost::format("%1%\n") % mkvtoolnix_hacks[hidx]);
      mxexit(0);

    } else if (engage_args[aidx] == "cow") {
      const string initial = "ICAgICAgICAgIChfXykKICAgICAgICAgICgqKikg"
        "IE9oIGhvbmV5LCB0aGF0J3Mgc28gc3dlZXQhCiAgIC8tLS0tLS0tXC8gICBPZiB"
        "jb3Vyc2UgSSdsbCBtYXJyeSB5b3UhCiAgLyB8ICAgICB8fAogKiAgfHwtLS0tfH"
        "wKICAgIF5eICAgIF5eCg==";
      char correction[200];
      memset(correction, 0, 200);
      base64_decode(initial, (unsigned char *)correction);
      mxinfo(correction);
      mxexit(0);
    }

  for (aidx = 0; aidx < engage_args.size(); aidx++) {
    valid_hack = false;
    for (hidx = 0; mkvtoolnix_hacks[hidx] != NULL; hidx++)
      if (engage_args[aidx] == mkvtoolnix_hacks[hidx]) {
        valid_hack = true;
        engaged_hacks.push_back(mkvtoolnix_hacks[hidx]);
        break;
      }
    if (!valid_hack)
      mxerror(boost::format(Y("'%1%' is not a valid hack.\n")) % engage_args[aidx]);
  }
}
