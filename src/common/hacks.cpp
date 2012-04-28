/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   hacks :)

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <string.h>

#include "common/base64.h"
#include "common/hacks.h"
#include "common/strings/editing.h"

static const struct {
  unsigned int id;
  const char *name;
} s_available_hacks[] = {
  { ENGAGE_SPACE_AFTER_CHAPTERS,         "space_after_chapters"         },
  { ENGAGE_NO_CHAPTERS_IN_META_SEEK,     "no_chapters_in_meta_seek"     },
  { ENGAGE_NO_META_SEEK,                 "no_meta_seek"                 },
  { ENGAGE_LACING_XIPH,                  "lacing_xiph"                  },
  { ENGAGE_LACING_EBML,                  "lacing_ebml"                  },
  { ENGAGE_NATIVE_MPEG4,                 "native_mpeg4"                 },
  { ENGAGE_NO_VARIABLE_DATA,             "no_variable_data"             },
  { ENGAGE_FORCE_PASSTHROUGH_PACKETIZER, "force_passthrough_packetizer" },
  { ENGAGE_WRITE_HEADERS_TWICE,          "write_headers_twice"          },
  { ENGAGE_ALLOW_AVC_IN_VFW_MODE,        "allow_avc_in_vfw_mode"        },
  { ENGAGE_KEEP_BITSTREAM_AR_INFO,       "keep_bitstream_ar_info"       },
  { ENGAGE_NO_SIMPLE_BLOCKS,             "no_simpleblocks"              },
  { ENGAGE_OLD_AAC_CODECID,              "old_aac_codecid"              },
  { ENGAGE_USE_CODEC_STATE_ONLY,         "use_codec_state_only"         },
  { ENGAGE_ENABLE_TIMECODE_WARNING,      "enable_timecode_warning"      },
  { ENGAGE_MERGE_TRUEHD_FRAMES,          "merge_truehd_frames"          },
  { ENGAGE_REMOVE_BITSTREAM_AR_INFO,     "remove_bitstream_ar_info"     },
  { ENGAGE_VOBSUB_SUBPIC_STOP_CMDS,      "vobsub_subpic_stop_cmds"      },
  { 0,                                   nullptr },
};
static std::vector<bool> s_engaged_hacks(ENGAGE_MAX_IDX + 1, false);

bool
hack_engaged(unsigned int id) {
  return (s_engaged_hacks.size() > id) && s_engaged_hacks[id];
}

void
engage_hacks(const std::string &hacks) {
  std::vector<std::string> engage_args = split(hacks, ",");
  size_t aidx, hidx;

  for (aidx = 0; engage_args.size() > aidx; aidx++)
    if (engage_args[aidx] == "list") {
      mxinfo(Y("Valid hacks are:\n"));
      for (hidx = 0; s_available_hacks[hidx].name; ++hidx)
        mxinfo(boost::format("%1%\n") % s_available_hacks[hidx].name);
      mxexit(0);

    } else if (engage_args[aidx] == "cow") {
      const std::string initial = "ICAgICAgICAgIChfXykKICAgICAgICAgICgqKikg"
        "IE9oIGhvbmV5LCB0aGF0J3Mgc28gc3dlZXQhCiAgIC8tLS0tLS0tXC8gICBPZiB"
        "jb3Vyc2UgSSdsbCBtYXJyeSB5b3UhCiAgLyB8ICAgICB8fAogKiAgfHwtLS0tfH"
        "wKICAgIF5eICAgIF5eCg==";
      char correction[200];
      memset(correction, 0, 200);
      base64_decode(initial, (unsigned char *)correction);
      mxinfo(correction);
      mxexit(0);
    }

  for (aidx = 0; engage_args.size() > aidx; aidx++) {
    bool valid_hack = false;
    for (hidx = 0; s_available_hacks[hidx].name; hidx++)
      if (engage_args[aidx] == s_available_hacks[hidx].name) {
        valid_hack = true;
        s_engaged_hacks[s_available_hacks[hidx].id] = true;
        break;
      }

    if (!valid_hack)
      mxerror(boost::format(Y("'%1%' is not a valid hack.\n")) % engage_args[aidx]);
  }
}

void
init_hacks() {
  std::vector<std::string> env_vars = { "MKVTOOLNIX_ENGAGE", "MTX_ENGAGE", balg::to_upper_copy(get_program_name()) + "_ENGAGE" };

  for (auto &name : env_vars) {
    auto value = getenv(name.c_str());
    if (value)
      engage_hacks(value);
  }
}
