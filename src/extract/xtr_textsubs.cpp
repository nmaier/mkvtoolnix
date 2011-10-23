/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include "common/ebml.h"
#include "common/matroska.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "extract/xtr_textsubs.h"

xtr_srt_c::xtr_srt_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_num_entries(0)
  , m_sub_charset(tspec.sub_charset)
  , m_conv(charset_converter_c::init(tspec.sub_charset))
{
}

void
xtr_srt_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);
  m_out->write_bom(m_sub_charset);
}

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
    mxwarn(boost::format(Y("Track %1%: Subtitle entry number %2% is missing its duration. Assuming a duration of 1s.\n")) % m_tid % (m_num_entries + 1));
    duration = 1000000000;
  }

  int64_t start = timecode / 1000000;
  int64_t end   = start + duration / 1000000;

  ++m_num_entries;
  char *text = new char[frame->get_size() + 1];
  memcpy(text, frame->get_buffer(), frame->get_size());
  text[frame->get_size()] = 0;

  std::string buffer =
    (boost::format("%1%\n"
                   "%|2$02d|:%|3$02d|:%|4$02d|,%|5$03d| --> %|6$02d|:%|7$02d|:%|8$02d|,%|9$03d|\n"
                   "%10%\n\n")
     % m_num_entries
     % (start / 1000 / 60 / 60) % ((start / 1000 / 60) % 60) % ((start / 1000) % 60) % (start % 1000)
     % (end   / 1000 / 60 / 60) % ((end   / 1000 / 60) % 60) % ((end   / 1000) % 60) % (end   % 1000)
     % m_conv->native(text)
     ).str();

  m_out->puts(buffer);
  m_out->flush();
  delete []text;
}

// ------------------------------------------------------------------------

const char *xtr_ssa_c::ms_kax_ssa_fields[10] = {
  "readorder", "layer",   "style",   "name",
  "marginl",   "marginr", "marginv",
  "effect",    "text",    NULL
};

xtr_ssa_c::xtr_ssa_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_sub_charset(tspec.sub_charset)
  , m_conv(charset_converter_c::init(tspec.sub_charset))
  , m_warning_printed(false)
{
}

void
xtr_ssa_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private \" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  xtr_base_c::create_file(master, track);
  m_out->write_bom(m_sub_charset);

  memory_cptr mpriv       = decode_codec_private(priv);

  const unsigned char *pd = mpriv->get_buffer();
  int priv_size           = mpriv->get_size();
  unsigned int bom_len    = 0;
  byte_order_e byte_order = BO_NONE;

  // Skip any BOM that might be present.
  mm_text_io_c::detect_byte_order_marker(pd, priv_size, byte_order, bom_len);

  pd                += bom_len;
  priv_size         -= bom_len;

  char *s            = new char[priv_size + 1];
  memcpy(s, pd, priv_size);
  s[priv_size]       = 0;
  std::string sconv  = s;
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
    mxerror(boost::format(Y("Internal bug: tracks.cpp SSA #1. %1%")) % BUGMSG);

  int pos2 = sconv.find("\n", pos1);
  if (0 > pos2)
    pos2 = sconv.length();

  std::string format_line = ba::to_lower_copy(sconv.substr(pos1 + 7, pos2 - pos1 - 7));
  if (std::string::npos == format_line.find("text")) {
    if (format_line[format_line.length() - 1] == '\r') {
      format_line.erase(format_line.length() - 1);
      --pos2;
    }

    format_line += ",text";
    sconv.insert(pos2, ", Text");
  }

  m_ssa_format = split(format_line, ",");
  strip(m_ssa_format, true);

  sconv = m_conv->native(sconv);
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
    mxwarn(boost::format(Y("Subtitle track %1% is missing some duration elements. "
                           "Please check the resulting SSA/ASS file for entries that have the same start and end time.\n"))
           % m_tid);
    m_warning_printed = true;
  }

  int64_t start = timecode / 1000000;
  int64_t end   = start + duration / 1000000;

  char *s       = (char *)safemalloc(frame->get_size() + 1);
  memory_c af_s((unsigned char *)s, 0, true);
  memcpy(s, frame->get_buffer(), frame->get_size());
  s[frame->get_size()] = 0;

  // Split the line into the fields.
  // Specs say that the following fields are to put into the block:
  // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
  // 6: MarginV, 7: Effect, 8: Text
  std::vector<std::string> fields = split(s, ",", 9);
  if (9 < fields.size()) {
    mxwarn(boost::format(Y("Invalid format for a SSA line ('%1%') at timecode %2%: Too many fields found (%3% instead of 9). This entry will be skipped.\n"))
           % s % format_timecode(timecode * 1000000, 3) % fields.size());
    return;
  }

  while (9 != fields.size())
    fields.push_back("");

  // Convert the ReadOrder entry so that we can re-order the entries later.
  int num;
  if (!parse_int(fields[0], num)) {
    mxwarn(boost::format(Y("Invalid format for a SSA line ('%1%') at timecode %2%: The first field is not an integer. This entry will be skipped.\n"))
           % s % format_timecode(timecode * 1000000, 3));
    return;
  }

  // Reconstruct the 'original' line. It'll look like this for SSA:
  //   Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
  // and for ASS:
  //   Layer, Start, End, Style, Actor, MarginL, MarginR, MarginV, Effect, Text

  // Problem is that the CodecPrivate may contain a Format: line
  // that defines a different layout. So let's account for that.

  std::string line = "Dialogue: ";
  size_t i;
  for (i = 0; i < m_ssa_format.size(); i++) {
    std::string format = m_ssa_format[i];

    if (ba::iequals(format, "actor"))
      format = "name";

    if (0 < i)
      line += ",";

    if (format == "marked")
      line += "Marked=0";

    else if (format == "start")
      line += (boost::format("%1%:%|2$02d|:%|3$02d|.%|4$02d|")
               % (start / 1000 / 60 / 60) % ((start / 1000 / 60) % 60) % ((start / 1000) % 60) % ((start % 1000) / 10)).str();

    else if (format == "end")
      line += (boost::format("%1%:%|2$02d|:%|3$02d|.%|4$02d|")
               % (end   / 1000 / 60 / 60) % ((end   / 1000 / 60) % 60) % ((end   / 1000) % 60) % ((end   % 1000) / 10)).str();

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
  line  = m_conv->native(line);
  line += "\n";

  // Now store that entry.
  m_lines.push_back(ssa_line_c(line, num));
}

void
xtr_ssa_c::finish_file() {
  size_t i;

  // Sort the SSA lines according to their ReadOrder number and
  // write them.
  std::sort(m_lines.begin(), m_lines.end());
  for (i = 0; i < m_lines.size(); i++)
    m_out->puts(m_lines[i].m_line.c_str());
}

// ------------------------------------------------------------------------

xtr_usf_c::xtr_usf_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_sub_charset(tspec.sub_charset)
{
  if (m_sub_charset.empty())
    m_sub_charset = "UTF-8";
}

void
xtr_usf_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  init_content_decoder(track);

  memory_cptr new_priv = decode_codec_private(priv);
  m_codec_private.append((const char *)new_priv->get_buffer(), new_priv->get_size());

  KaxTrackLanguage *language = FINDFIRST(&track, KaxTrackLanguage);
  if (NULL == language)
    m_language = "eng";
  else
    m_language = std::string(*language);

  if (NULL != master) {
    xtr_usf_c *usf_master = dynamic_cast<xtr_usf_c *>(master);
    if (NULL == usf_master)
      mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because "
                              "track %4% with the CodecID '%5%' is already being written to the same file.\n"))
              % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

    if (m_codec_private != usf_master->m_codec_private)
      mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because track %4% with the CodecID '%5%' is already "
                              "being written to the same file, and their CodecPrivate data (the USF styles etc) do not match.\n"))
              % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

    m_formatter = usf_master->m_formatter;
    m_master    = usf_master;

  } else {
    try {
      std::string end_tag           = "</USFSubtitles>";
      std::string codec_private_mod = m_codec_private;
      int end_tag_pos               = codec_private_mod.find(end_tag);
      if (0 <= end_tag_pos)
        codec_private_mod.erase(end_tag_pos, end_tag.length());

      m_out       = new mm_file_io_c(m_file_name, MODE_WRITE);
      m_formatter = counted_ptr<xml_formatter_c>(new xml_formatter_c(m_out, m_sub_charset));

      m_formatter->set_doctype("USFSubtitles", "USFV100.dtd");
      m_formatter->set_stylesheet("text/xsl", "USFV100.xsl");
      m_formatter->write_header();
      m_formatter->format(codec_private_mod + "\n");

    } catch (mm_io_error_c &) {
      mxerror(boost::format(Y("Failed to create the file '%1%': %2% (%3%)\n")) % m_file_name % errno % strerror(errno));

    } catch (xml_formatter_error_c &error) {
      mxerror(boost::format(Y("Failed to parse the USF codec private data for track %1%: %2%\n")) % m_tid % error.get_error());
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
  entry.m_text.append((const char *)frame->get_buffer(), frame->get_size());
  m_entries.push_back(entry);
}

void
xtr_usf_c::finish_track() {
  try {
    m_formatter->format((boost::format("<subtitles>\n<language code=\"%1%\"/>\n") % m_language).str());

    for (auto &entry : m_entries) {
      std::string text = entry.m_text;
      strip(text, true);
      m_formatter->format((boost::format("<subtitle start=\"%1%\" stop=\"%2%\">")
                           % format_timecode(entry.m_start * 1000000, 3) % format_timecode(entry.m_end * 1000000, 3)).str());
      m_formatter->format_fixed(text);
      m_formatter->format("</subtitle>\n");
    }
    m_formatter->format("</subtitles>\n");

  } catch (xml_formatter_error_c &error) {
    mxerror(boost::format(Y("Failed to parse an USF subtitle entry for track %1%: %2%\n")) % m_tid % error.get_error());
  }
}

void
xtr_usf_c::finish_file() {
  try {
    if (NULL == m_master) {
      m_formatter->format("</USFSubtitles>");
      m_out->puts("\n");
    }
  } catch (xml_formatter_error_c &error) {
    mxerror(boost::format(Y("Failed to parse the USF end tag for track %1%: %2%\n")) % m_tid % error.get_error());
  }
}
