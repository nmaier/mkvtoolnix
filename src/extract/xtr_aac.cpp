/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common.h"
#include "commonebml.h"

#include "xtr_aac.h"

xtr_aac_c::xtr_aac_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  channels(0), id(0), profile(0), srate_idx(0) {
}

void
xtr_aac_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  int sfreq;

  xtr_base_c::create_file(_master, track);

  channels = kt_get_a_channels(track);
  sfreq = (int)kt_get_a_sfreq(track);

  // A_AAC/MPEG4/MAIN
  // 0123456789012345
  if (codec_id[10] == '4')
    id = 0;
  else if (codec_id[10] == '2')
    id = 1;
  else
    mxerror("Track ID " LLD " has an unknown AAC type.\n", tid);

  if (!strcmp(&codec_id[12], "MAIN"))
    profile = 0;
  else if (!strcmp(&codec_id[12], "LC") ||
           (strstr(&codec_id[12], "SBR") != NULL))
    profile = 1;
  else if (!strcmp(&codec_id[12], "SSR"))
    profile = 2;
  else if (!strcmp(&codec_id[12], "LTP"))
    profile = 3;
  else
    mxerror("Track ID " LLD " has an unknown AAC type.\n", tid);

  if (92017 <= sfreq)
    srate_idx = 0;
  else if (75132 <= sfreq)
    srate_idx = 1;
  else if (55426 <= sfreq)
    srate_idx = 2;
  else if (46009 <= sfreq)
    srate_idx = 3;
  else if (37566 <= sfreq)
    srate_idx = 4;
  else if (27713 <= sfreq)
    srate_idx = 5;
  else if (23004 <= sfreq)
    srate_idx = 6;
  else if (18783 <= sfreq)
    srate_idx = 7;
  else if (13856 <= sfreq)
    srate_idx = 8;
  else if (11502 <= sfreq)
    srate_idx = 9;
  else if (9391 <= sfreq)
    srate_idx = 10;
  else
    srate_idx = 11;
}

void
xtr_aac_c::handle_block(KaxBlock &block,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref) {
  char adts[56 / 8];
  int i, len;

  for (i = 0; i < block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);

    // Recreate the ADTS headers. What a fun. Like runing headlong into
    // a solid wall. But less painful. Well such is life, you know.
    // But then again I've just seen a beautiful girl walking by my
    // window, and suddenly the world is a bright place. Everything's
    // a matter of perspective. And if I didn't enjoy writing even this
    // code then I wouldn't do it at all. So let's get to it!

    // sync word, 12 bits
    adts[0] = 0xff;
    adts[1] = 0xf0;

    // ID, 1 bit
    adts[1] |= id << 3;
    // layer: 2 bits = 00

    // protection absent: 1 bit = 1 (ASSUMPTION!)
    adts[1] |= 1;

    // profile, 2 bits
    adts[2] = profile << 6;

    // sampling frequency index, 4 bits
    adts[2] |= srate_idx << 2;

    // private, 1 bit = 0 (ASSUMPTION!)

    // channels, 3 bits
    adts[2] |= (channels & 4) >> 2;
    adts[3] = (channels & 3) << 6;

    // original/copy, 1 bit = 0(ASSUMPTION!)

    // home, 1 bit = 0 (ASSUMPTION!)

    // copyright id bit, 1 bit = 0 (ASSUMPTION!)

    // copyright id start, 1 bit = 0 (ASSUMPTION!)

    // frame length, 13 bits
    len = data.Size() + 7;
    adts[3] |= len >> 11;
    adts[4] = (len >> 3) & 0xff;
    adts[5] = (len & 7) << 5;

    // adts buffer fullness, 11 bits, 0x7ff = VBR (ASSUMPTION!)
    adts[5] |= 0x1f;
    adts[6] = 0xfc;

    // number of raw frames, 2 bits, 0 (meaning 1 frame) (ASSUMPTION!)

    // Write the ADTS header and the data itself.
    out->write(adts, 56 / 8);
    out->write(data.Buffer(), data.Size());
  }
}
