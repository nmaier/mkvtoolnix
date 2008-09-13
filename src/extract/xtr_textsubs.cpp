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

xtr_srt_c::xtr_srt_c(const string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_num_entries(0)
  , m_sub_charset(tspec.sub_charset)
  , m_conv(utf8_init(tspec.sub_charset))
{
}

void
xtr_srt_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);
  m_out->write_bom(m_sub_charset);
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
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  if (-1 == duration) {
    mxwarn("Track " LLD ": Subtitle entry number %d is missing its duration. Assuming a duration of 1s.\n", m_tid, m_num_entries + 1);
    duration = 1000000000;
  }

  int64_t start = timecode / 1000000;
  int64_t end   = start + duration / 1000000;

  ++m_num_entries;
  char *text = new char[frame->get_size() + 1];
  memcpy(text, frame->get(), frame->get_size());
  text[frame->get_size()] = 0;

  string buffer = mxsprintf("%d\n"
                            LLD02 ":" LLD02 ":" LLD02 "," LLD03 " --> " LLD02 ":" LLD02 ":" LLD02 "," LLD03 "\n"
                            "%s\n\n",
                            m_num_entries,
                            start / 1000 / 60 / 60, (start / 1000 / 60) % 60, (start / 1000) % 60, start % 1000,
                            end   / 1000 / 60 / 60, (end   / 1000 / 60) % 60, (end   / 1000) % 60, end   % 1000,
                            from_utf8(m_conv, text).c_str());
  m_out->puts(buffer);
  delete []text;
}

// ------------------------------------------------------------------------

const char *xtr_ssa_c::ms_kax_ssa_fields[10] = {
  "readorder", "layer",   "style",   "name",
  "marginl",   "marginr", "marginv",
  "effect",    "text",    NULL
};

xtr_ssa_c::xtr_ssa_c(const string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_sub_charset(tspec.sub_charset)
  , m_conv(utf8_init(tspec.sub_charset))
  , m_warning_printed(false)
{
}

void
xtr_ssa_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec private \" element and cannot be extracted.\n", m_tid, m_codec_id.c_str());

  xtr_base_c::create_file(master, track);
  m_out->write_bom(m_sub_charset);

  memory_cptr mpriv = decode_codec_private(priv);

  const unsigned char *pd = mpriv->get();
  int priv_size           = mpriv->get_size();
  int bom_len             = 0;

  // Skip any BOM that might be present.
  if ((3 < priv_size) && (0xef == pd[0]) && (0xbb == pd[1]) && (0xbf == pd[2]))
    bom_len = 3;
  else if ((4 < priv_size) && (0xff == pd[0]) && (0xfe == pd[1]) && (0x00 == pd[2]) && (0x00 == pd[3]))
    bom_len = 4;
  else if ((4 < priv_size) && (0x00 == pd[0]) && (0x00 == pd[1]) && (0xfe == pd[2]) && (0x0ff == pd[3]))
    bom_len = 4;
  else if ((2 < priv_size) && (0xff == pd[0]) && (0xfe == pd[1]))
    bom_len = 2;
  else if ((2 < priv_size) && (0xfe == pd[0]) && (0xff == pd[1]))
    bom_len = 2;

  pd           += bom_len;
  priv_size    -= bom_len;

  char *s       = new char[priv_size + 1];
  memcpy(s, pd, priv_size);
  s[priv_size]  = 0;
  string sconv  = s;
  delete []s;

  const char *p1;
  if (((p1 = strstr(sconv.c_str(), "[Events]")) == NULL) || (strstr(p1, "Format:") == NULL)) {
    if (m_codec_id == MKV_S_TEXTSSA)
      sconv += "\n[Events]\nFormat: Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    else
      sconv += "\n[Events]\nFormat: Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text\n";

  } else if (sconv.empty() || (sconv[sconv.length() - 1] != '\n'))
    sconv += "\n";

  // Keep the Format: line so that the extracted file has the
  // correct field order.
  int pos1 = sconv.find("Format:", sconv.find("[Events]"));
  if (0 > pos1)
    mxerror("Internal bug: tracks.cpp SSA #1. %s", BUGMSG);

  int pos2 = sconv.find("\n", pos1);
  if (0 > pos2)
    pos2 = sconv.length();

  m_ssa_format = split(sconv.substr(pos1 + 7, pos2 - pos1 - 7), ",");
  strip(m_ssa_format, true);
  for (pos1 = 0; pos1 < m_ssa_format.size(); pos1++)
    m_ssa_format[pos1] = downcase(m_ssa_format[pos1]);

  sconv = from_utf8(m_conv, sconv);
  m_out->puts(sconv);
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

  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  if (0 > duration) {
    mxwarn("Subtitle track " LLD " is missing some duration elements. Please check the resulting SSA/ASS file for entries that have the same start and end time.\n",
           m_tid);
    m_warning_printed = true;
  }

  int64_t start = timecode / 1000000;
  int64_t end   = start + duration / 1000000;

  char *s       = (char *)safemalloc(frame->get_size() + 1);
  memory_c af_s((unsigned char *)s, 0, true);
  memcpy(s, frame->get(), frame->get_size());
  s[frame->get_size()] = 0;

  // Split the line into the fields.
  // Specs say that the following fields are to put into the block:
  // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
  // 6: MarginV, 7: Effect, 8: Text
  vector<string> fields = split(s, ",", 9);
  if (9 < fields.size()) {
    mxwarn("Invalid format for a SSA line ('%s') at timecode " FMT_TIMECODE ": Too many fields found (%u instead of 9). This entry will be skipped.\n",
           s, ARG_TIMECODE_NS(timecode), (unsigned int)fields.size());
    return;
  }

  while (9 != fields.size())
    fields.push_back("");

  // Convert the ReadOrder entry so that we can re-order the entries later.
  int num;
  if (!parse_int(fields[0], num)) {
    mxwarn("Invalid format for a SSA line ('%s') at timecode " FMT_TIMECODE ": The first field is not an integer. This entry will be skipped.\n",
           s, ARG_TIMECODE_NS(timecode));
    return;
  }

  // Reconstruct the 'original' line. It'll look like this for SSA:
  //   Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
  // and for ASS:
  //   Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text

  // Problem is that the CodecPrivate may contain a Format: line
  // that defines a different layout. So let's account for that.

  string line = "Dialogue: ";
  int i;
  for (i = 0; i < m_ssa_format.size(); i++) {
    string format = m_ssa_format[i];

    if (downcase(format) == "actor")
      format = "name";

    if (0 < i)
      line += ",";

    if (format == "marked")
      line += "Marked=0";

    else if (format == "start")
      line += mxsprintf(LLD ":" LLD02 ":" LLD02 "." LLD02, start / 1000 / 60 / 60, (start / 1000 / 60) % 60, (start / 1000) % 60, (start % 1000) / 10);

    else if (format == "end")
      line += mxsprintf(LLD ":" LLD02 ":" LLD02 "." LLD02, end   / 1000 / 60 / 60, (end   / 1000 / 60) % 60, (end   / 1000) % 60, (end   % 1000) / 10);

    else {
      int k;
      for (k = 0; NULL != ms_kax_ssa_fields[k]; ++k)
        if (format == ms_kax_ssa_fields[k]) {
          line += fields[k];
          break;
        }
    }
  }

  // Do the charset conversion.
  line  = from_utf8(m_conv, line);
  line += "\n";

  // Now store that entry.
  m_lines.push_back(ssa_line_c(line, num));
}

void
xtr_ssa_c::finish_file() {
  int i;

  // Sort the SSA lines according to their ReadOrder number and
  // write them.
  sort(m_lines.begin(), m_lines.end());
  for (i = 0; i < m_lines.size(); i++)
    m_out->puts(m_lines[i].m_line.c_str());
}

// ------------------------------------------------------------------------

xtr_usf_c::xtr_usf_c(const string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_sub_charset(tspec.sub_charset) {

  if (m_sub_charset == "")
    m_sub_charset = "UTF-8";
}

void
xtr_usf_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec private \" element and cannot be extracted.\n", m_tid, m_codec_id.c_str());

  init_content_decoder(track);

  memory_cptr new_priv = decode_codec_private(priv);
  m_codec_private.append((const char *)new_priv->get(), new_priv->get_size());

  KaxTrackLanguage *language = FINDFIRST(&track, KaxTrackLanguage);
  if (NULL == language)
    m_language = "eng";
  else
    m_language = string(*language);

  if (NULL != master) {
    xtr_usf_c *usf_master = dynamic_cast<xtr_usf_c *>(master);
    if (NULL == usf_master)
      mxerror("Cannot write track " LLD " with the CodecID '%s' to the file '%s' because track " LLD " with the CodecID '%s' is already being "
              "written to the same file.\n",
              m_tid, m_codec_id.c_str(), m_file_name.c_str(), master->m_tid, master->m_codec_id.c_str());

    if (m_codec_private != usf_master->m_codec_private)
      mxerror("Cannot write track " LLD " with the CodecID '%s' to the file '%s' because track " LLD " with the CodecID '%s' is already "
              "being written to the same file, and their CodecPrivate data (the USF styles etc) do not match.\n",
              m_tid, m_codec_id.c_str(), m_file_name.c_str(), master->m_tid, master->m_codec_id.c_str());

    m_formatter = usf_master->m_formatter;
    m_master    = usf_master;

  } else {
    try {
      string end_tag           = "</USFSubtitles>";
      string codec_private_mod = m_codec_private;
      int end_tag_pos          = codec_private_mod.find(end_tag);
      if (0 <= end_tag_pos)
        codec_private_mod.erase(end_tag_pos, end_tag.length());

      m_out       = new mm_file_io_c(m_file_name, MODE_CREATE);
      m_formatter = counted_ptr<xml_formatter_c>(new xml_formatter_c(m_out, m_sub_charset));

      m_formatter->set_doctype("USFSubtitles", "USFV100.dtd");
      m_formatter->set_stylesheet("text/xsl", "USFV100.xsl");
      m_formatter->write_header();
      m_formatter->format(codec_private_mod + "\n");

    } catch (mm_io_error_c &error) {
      mxerror("Failed to create the file '%s': %d (%s)\n", m_file_name.c_str(), errno, strerror(errno));

    } catch (xml_formatter_error_c &error) {
      mxerror("Failed to parse the USF codec private data for track " LLD ": " "%s\n", m_tid, error.get_error().c_str());
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
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  usf_entry_t entry("", timecode, timecode + duration);
  entry.m_text.append((const char *)frame->get(), frame->get_size());
  m_entries.push_back(entry);
}

void
xtr_usf_c::finish_track() {
  try {
    m_formatter->format(mxsprintf("<subtitles>\n<language code=\"%s\"/>\n", m_language.c_str()));

    vector<usf_entry_t>::const_iterator entry;
    mxforeach(entry, m_entries) {
      string text = entry->m_text;
      strip(text, true);
      m_formatter->format(mxsprintf("<subtitle start=\"" FMT_TIMECODE "\" stop=\"" FMT_TIMECODE "\">", ARG_TIMECODE_NS(entry->m_start), ARG_TIMECODE_NS(entry->m_end)));
      m_formatter->format_fixed(text);
      m_formatter->format("</subtitle>\n");
    }
    m_formatter->format("</subtitles>\n");

  } catch (xml_formatter_error_c &error) {
    mxerror("Failed to parse an USF subtitle entry for track " LLD ": %s\n", m_tid, error.get_error().c_str());
  }
}

void
xtr_usf_c::finish_file() {
  try {
    if (NULL == m_master) {
      m_formatter->format("</USFSubtitles>");
      m_out->printf("\n");
    }
  } catch (xml_formatter_error_c &error) {
    mxerror("Failed to parse the USF end tag for track " LLD ": %s\n", m_tid, error.get_error().c_str());
  }
}
