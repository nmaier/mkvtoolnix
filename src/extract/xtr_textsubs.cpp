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

#define LLD02 "%02" PRId64
#define LLD03 "%03" PRId64

void
xtr_srt_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  char *text;
  int64_t start, end;
  string buffer;

  if (-1 == duration) {
    mxwarn("Track " LLD ": Subtitle entry number %d is missing its duration. "
           "Assuming a duration of 1s.\n", tid, num_entries + 1);
    duration = 1000000000;
  }

  start = timecode / 1000000;
  end = start + duration / 1000000;

  ++num_entries;
  text = new char[frame->get_size() + 1];
  memcpy(text, frame->get(), frame->get_size());
  text[frame->get_size()] = 0;
  buffer = mxsprintf("%d\n"
                     LLD02 ":" LLD02 ":" LLD02 "," LLD03 " --> "
                     LLD02 ":" LLD02 ":" LLD02 "," LLD03 "\n"
                     "%s\n\n",
                     num_entries,
                     start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                     (start / 1000) % 60, start % 1000,
                     end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                     (end / 1000) % 60, end % 1000,
                     from_utf8(conv, text).c_str());
  out->puts(buffer);
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
  char *s;
  const char *p1;
  const unsigned char *pd;
  int bom_len, pos1, pos2, priv_size;
  string sconv;
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private \" element and cannot be extracted.\n", tid,
            codec_id.c_str());

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
  out->puts(sconv);
}

void
xtr_ssa_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  int i, k, num;
  char *s;
  vector<string> fields;
  ssa_line_c ssa_line;
  string line;
  int64_t start, end;

  if (0 > duration) {
    mxwarn("Subtitle track " LLD " is missing some duration elements. "
           "Please check the resulting SSA/ASS file for entries that "
           "have the same start and end time.\n", tid);
    warning_printed = true;
  }
  start = timecode / 1000000;
  end = start + duration / 1000000;

  s = (char *)safemalloc(frame->get_size() + 1);
  memory_c af_s((unsigned char *)s, 0, true);
  memcpy(s, frame->get(), frame->get_size());
  s[frame->get_size()] = 0;

  // Split the line into the fields.
  // Specs say that the following fields are to put into the block:
  // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
  // 6: MarginV, 7: Effect, 8: Text
  fields = split(s, ",", 9);
  if (9 < fields.size()) {
    mxwarn("Invalid format for a SSA line ('%s') at timecode " FMT_TIMECODE
           ": Too many fields found (%u instead of 9). This entry will be "
           "skipped.\n", s, ARG_TIMECODE_NS(timecode),
           (unsigned int)fields.size());
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
    string format = ssa_format[i];

    if (downcase(format) == "actor")
      format = "name";
    if (0 < i)
      line += ",";
    if (format == "marked")
      line += "Marked=0";
    else if (format == "start")
      line += mxsprintf(LLD ":" LLD02 ":" LLD02 "." LLD02,
                        start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                        (start / 1000) % 60, (start % 1000) / 10);
    else if (format == "end")
      line += mxsprintf(LLD ":" LLD02 ":" LLD02 "." LLD02,
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
    out->puts(lines[i].line.c_str());
}

// ------------------------------------------------------------------------

xtr_usf_c::xtr_usf_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  m_sub_charset(tspec.sub_charset) {

  if (m_sub_charset == "")
    m_sub_charset = "UTF-8";
}

void
xtr_usf_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv;
  KaxTrackLanguage *language;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private \" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  if (!content_decoder.initialize(track))
    mxerror("Tracks with unsupported content encoding schemes (compression "
            "or encryption) cannot be extracted.\n");

  memory_cptr new_priv(new memory_c(priv->GetBuffer(), priv->GetSize(),
                                    false));
  content_decoder.reverse(new_priv, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
  m_codec_private.append((const char *)new_priv->get(), new_priv->get_size());

  language = FINDFIRST(&track, KaxTrackLanguage);
  if (NULL == language)
    m_language = "eng";
  else
    m_language = string(*language);

  if (NULL != _master) {
    xtr_usf_c *usf_master;

    usf_master = dynamic_cast<xtr_usf_c *>(_master);
    if (NULL == usf_master)
      mxerror("Cannot write track " LLD " with the CodecID '%s' to the file "
              "'%s' because track " LLD " with the CodecID '%s' is already "
              "being written to the same file.\n", tid, codec_id.c_str(),
              file_name.c_str(), _master->tid, _master->codec_id.c_str());
    if (m_codec_private != usf_master->m_codec_private)
      mxerror("Cannot write track " LLD " with the CodecID '%s' to the file "
              "'%s' because track " LLD " with the CodecID '%s' is already "
              "being written to the same file, and their CodecPrivate data "
              "(the USF styles etc) do not match.\n", tid, codec_id.c_str(),
              file_name.c_str(), _master->tid, _master->codec_id.c_str());

    m_formatter = usf_master->m_formatter;
    master = usf_master;

  } else {
    try {
      int end_tag_pos;
      string codec_private_mod, end_tag = "</USFSubtitles>";

      codec_private_mod = m_codec_private;
      end_tag_pos = codec_private_mod.find(end_tag);
      if (0 <= end_tag_pos)
        codec_private_mod.erase(end_tag_pos, end_tag.length());

      out = new mm_file_io_c(file_name, MODE_CREATE);

      m_formatter =
        counted_ptr<xml_formatter_c>(new xml_formatter_c(out, m_sub_charset));
      m_formatter->set_doctype("USFSubtitles", "USFV100.dtd");
      m_formatter->set_stylesheet("text/xsl", "USFV100.xsl");
      m_formatter->write_header();
      m_formatter->format(codec_private_mod + "\n");
    } catch (mm_io_error_c &error) {
      mxerror("Failed to create the file '%s': %d (%s)\n", file_name.c_str(),
              errno, strerror(errno));
    } catch (xml_formatter_error_c &error) {
      mxerror("Failed to parse the USF codec private data for track " LLD ": "
              "%s\n", tid, error.get_error().c_str());

    }
  }
}

void
xtr_usf_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  usf_entry_t entry("", timecode, timecode + duration);
  content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);
  entry.m_text.append((const char *)frame->get(), frame->get_size());
  m_entries.push_back(entry);
}

void
xtr_usf_c::finish_track() {
  vector<usf_entry_t>::const_iterator entry;

  try {
    m_formatter->format(mxsprintf("<subtitles>\n<language code=\"%s\"/>\n",
                                  m_language.c_str()));
    foreach(entry, m_entries) {
      string text = entry->m_text;
      strip(text, true);
      m_formatter->format(mxsprintf("<subtitle start=\"" FMT_TIMECODE
                                    "\" stop=\"" FMT_TIMECODE "\">",
                                    ARG_TIMECODE_NS(entry->m_start),
                                    ARG_TIMECODE_NS(entry->m_end)));
      m_formatter->format_fixed(text);
      m_formatter->format("</subtitle>\n");
    }
    m_formatter->format("</subtitles>\n");
  } catch (xml_formatter_error_c &error) {
    mxerror("Failed to parse an USF subtitle entry for track " LLD ": %s\n",
            tid, error.get_error().c_str());
  }
}

void
xtr_usf_c::finish_file() {
  try {
    if (NULL == master) {
      m_formatter->format("</USFSubtitles>");
      out->printf("\n");
    }
  } catch (xml_formatter_error_c &error) {
    mxerror("Failed to parse the USF end tag for track " LLD ": %s\n", tid,
            error.get_error().c_str());
  }
}
