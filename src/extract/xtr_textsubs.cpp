/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <algorithm>

#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "smart_pointers.h"

#include "xtr_textsubs.h"

xtr_srt_c::xtr_srt_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  num_entries(0), sub_charset(tspec.sub_charset) {

  conv = utf8_init(tspec.sub_charset);
}

void
xtr_srt_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {

  xtr_base_c::create_file(_master, track);
  out->write_bom(sub_charset);
}

void
xtr_srt_c::handle_block(KaxBlock &block,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref) {
  char *text;
  int64_t start, end;
  string buffer;

  if (-1 == duration) {
    mxwarn("Track %lld: Subtitle entry number %d is missing its duration. "
           "Assuming a duration of 1s.\n", tid, num_entries + 1);
    duration = 1000000000;
  }

  DataBuffer &data = block.GetBuffer(0);

  start = timecode / 1000000;
  end = start + duration / 1000000;

  ++num_entries;
  text = new char[data.Size() + 1];
  memcpy(text, data.Buffer(), data.Size());
  text[data.Size()] = 0;
  buffer = mxsprintf("%d\n"
                     "%02lld:%02lld:%02lld,%03lld --> "
                     "%02lld:%02lld:%02lld,%03lld\n"
                     "%s\n\n",
                     num_entries,
                     start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                     (start / 1000) % 60, start % 1000,
                     end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                     (end / 1000) % 60, end % 1000,
                     from_utf8(conv, text).c_str());
  out->puts_unl(buffer);
  delete []text;
}

// ------------------------------------------------------------------------

const char *xtr_ssa_c::kax_ssa_fields[10] = {
  "readorder", "layer", "style", "name", "marginl", "marginr",
  "marginv", "effect", "text", NULL
};

xtr_ssa_c::xtr_ssa_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  sub_charset(tspec.sub_charset), warning_printed(false) {

  conv = utf8_init(tspec.sub_charset);
}

void
xtr_ssa_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  char *s, *p1;
  const unsigned char *pd;
  int bom_len, pos1, pos2, priv_size;
  string sconv;
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track %lld with the CodecID '%s' is missing the \"codec private"
            "\" element and cannot be extracted.\n", tid, codec_id.c_str());

  xtr_base_c::create_file(_master, track);
  out->write_bom(sub_charset);

  pd = (const unsigned char *)priv->GetBuffer();
  priv_size = priv->GetSize();
  bom_len = 0;
  // Skip any BOM that might be present.
  if ((priv_size > 3) &&
      (pd[0] == 0xef) && (pd[1] == 0xbb) && (pd[2] == 0xbf))
    bom_len = 3;
  else if ((priv_size > 4) &&
           (pd[0] == 0xff) && (pd[1] == 0xfe) &&
           (pd[2] == 0x00) && (pd[3] == 0x00))
    bom_len = 4;
  else if ((priv_size > 4) &&
           (pd[0] == 0x00) && (pd[1] == 0x00) &&
           (pd[2] == 0xfe) && (pd[3] == 0xff))
    bom_len = 4;
  else if ((priv_size > 2) &&
           (pd[0] == 0xff) && (pd[1] == 0xfe))
    bom_len = 2;
  else if ((priv_size > 2) &&
           (pd[0] == 0xfe) && (pd[1] == 0xff))
    bom_len = 2;
  pd += bom_len;
  priv_size -= bom_len;

  s = new char[priv_size + 1];
  memcpy(s, pd, priv_size);
  s[priv_size] = 0;
  sconv = s;
  delete []s;

  if (((p1 = strstr(sconv.c_str(), "[Events]")) == NULL) ||
      (strstr(p1, "Format:") == NULL)) {
    if (codec_id == MKV_S_TEXTSSA)
      sconv += "\n[Events]\nFormat: Marked, Start, End, "
        "Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    else
      sconv += "\n[Events]\nFormat: Layer, Start, End, "
        "Style, Actor, MarginL, MarginR, MarginV, Effect, Text\n";
  } else if ((sconv.length() == 0) ||
             (sconv[sconv.length() - 1] != '\n'))
    sconv += "\n";

  // Keep the Format: line so that the extracted file has the
  // correct field order.
  pos1 = sconv.find("Format:", sconv.find("[Events]"));
  if (pos1 < 0)
    mxerror("Internal bug: tracks.cpp SSA #1. %s", BUGMSG);
  pos2 = sconv.find("\n", pos1);
  if (pos2 < 0)
    pos2 = sconv.length();
  ssa_format = split(sconv.substr(pos1 + 7, pos2 - pos1 - 7), ",");
  strip(ssa_format, true);
  for (pos1 = 0; pos1 < ssa_format.size(); pos1++)
    ssa_format[pos1] = downcase(ssa_format[pos1]);

  sconv = from_utf8(conv, sconv);
  out->puts_unl(sconv);
}

void
xtr_ssa_c::handle_block(KaxBlock &block,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref) {
  int i, k, num;
  char *s;
  vector<string> fields;
  ssa_line_c ssa_line;
  string line;
  int64_t start, end;

  if (0 > duration) {
    mxwarn("Subtitle track %lld is missing some duration elements. "
           "Please check the resulting SSA/ASS file for entries that "
           "have the same start and end time.\n", tid);
    warning_printed = true;
  }
  start = timecode / 1000000;
  end = start + duration / 1000000;

  DataBuffer &data = block.GetBuffer(0);
  autofree_ptr<char> af_s(s = (char *)safemalloc(data.Size() + 1));
  memcpy(s, data.Buffer(), data.Size());
  s[data.Size()] = 0;

  // Split the line into the fields.
  // Specs say that the following fields are to put into the block:
  // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
  // 6: MarginV, 7: Effect, 8: Text
  fields = split(s, ",", 9);
  if (9 < fields.size()) {
    mxwarn("Invalid format for a SSA line ('%s') at timecode " FMT_TIMECODE
           ": Too many fields found (%d instead of 9). This entry will be "
           "skipped.\n", s, ARG_TIMECODE_NS(timecode), fields.size());
    return;
  }
  while (9 != fields.size())
    fields.push_back("");

  // Convert the ReadOrder entry so that we can re-order the entries later.
  if (!parse_int(fields[0], num)) {
    mxwarn("Invalid format for a SSA line ('%s') at timecode " FMT_TIMECODE
           ": The first field is not an integer. This entry will be "
           "skipped.\n", s, ARG_TIMECODE_NS(timecode));
    return;
  }

  // Reconstruct the 'original' line. It'll look like this for SSA:
  // Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect,
  // Text
  // and for ASS:
  // Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect,
  // Text

  // Problem is that the CodecPrivate may contain a Format: line
  // that defines a different layout. So let's account for that.

  line = "Dialogue: ";
  for (i = 0; i < ssa_format.size(); i++) {
    string &format = ssa_format[i];

    if (0 < i)
      line += ",";
    if (format == "marked")
      line += "Marked=0";
    else if (format == "start")
      line += mxsprintf("%lld:%02lld:%02lld.%02lld",
                        start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                        (start / 1000) % 60, (start % 1000) / 10);
    else if (format == "end")
      line += mxsprintf("%lld:%02lld:%02lld.%02lld",
                        end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                        (end / 1000) % 60, (end % 1000) / 10);
    else {
      for (k = 0; kax_ssa_fields[k] != NULL; k++)
        if (format == kax_ssa_fields[k]) {
          line += fields[k];
          break;
        }
    }
  }

  // Do the charset conversion.
  line = from_utf8(conv, line);
  line += "\n";

  // Now store that entry.
  ssa_line.num = num;
  ssa_line.line = line;
  lines.push_back(ssa_line);
}

void
xtr_ssa_c::finish_file() {
  int i;

  // Sort the SSA lines according to their ReadOrder number and
  // write them.
  sort(lines.begin(), lines.end());
  for (i = 0; i < lines.size(); i++)
    out->puts_unl(lines[i].line.c_str());
}

