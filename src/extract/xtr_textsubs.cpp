/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

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
                        KaxBlockAdditions *,
                        int64_t timecode,
                        int64_t duration,
                        int64_t,
                        int64_t,
                        bool,
                        bool,
                        bool) {
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
  "effect",    "text",    nullptr
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
  KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
  if (nullptr == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

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
  if (((p1 = strstr(sconv.c_str(), "[Events]")) == nullptr) || (strstr(p1, "Format:") == nullptr)) {
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

  std::string format_line = balg::to_lower_copy(sconv.substr(pos1 + 7, pos2 - pos1 - 7));
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
                        KaxBlockAdditions *,
                        int64_t timecode,
                        int64_t duration,
                        int64_t,
                        int64_t,
                        bool,
                        bool,
                        bool) {
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

    if (balg::iequals(format, "actor"))
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
      for (k = 0; nullptr != ms_kax_ssa_fields[k]; ++k)
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

  m_simplified_sub_charset = boost::regex_replace(balg::to_lower_copy(m_sub_charset), boost::regex("[^a-z0-9]+", boost::regex::perl), "");
}

void
xtr_usf_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
  if (nullptr == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  init_content_decoder(track);

  memory_cptr new_priv = decode_codec_private(priv);
  m_codec_private.append((const char *)new_priv->get_buffer(), new_priv->get_size());

  KaxTrackLanguage *language = FindChild<KaxTrackLanguage>(&track);
  if (nullptr == language)
    m_language = "eng";
  else
    m_language = std::string(*language);

  if (nullptr != master) {
    xtr_usf_c *usf_master = dynamic_cast<xtr_usf_c *>(master);
    if (nullptr == usf_master)
      mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because "
                              "track %4% with the CodecID '%5%' is already being written to the same file.\n"))
              % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

    if (m_codec_private != usf_master->m_codec_private)
      mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because track %4% with the CodecID '%5%' is already "
                              "being written to the same file, and their CodecPrivate data (the USF styles etc) do not match.\n"))
              % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

    m_doc    = usf_master->m_doc;
    m_master = usf_master;

  } else {
    try {
      m_out = mm_file_io_c::open(m_file_name, MODE_CREATE);
      m_doc = std::make_shared<pugi::xml_document>();

      std::stringstream codec_private{m_codec_private};
      auto result = m_doc->load(codec_private, pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype | pugi::parse_pi | pugi::parse_comments);
      if (!result)
        throw mtx::xml::xml_parser_x{result};

      pugi::xml_node doctype_node, declaration_node, stylesheet_node;
      for (auto child : *m_doc)
        if (child.type() == pugi::node_declaration)
          declaration_node = child;

        else if (child.type() == pugi::node_doctype)
          doctype_node = child;

        else if ((child.type() == pugi::node_pi) && (std::string{child.name()} == "xml-stylesheet"))
          stylesheet_node = child;

      if (!declaration_node)
        declaration_node = m_doc->prepend_child(pugi::node_declaration);

      if (!doctype_node)
        doctype_node = m_doc->insert_child_after(pugi::node_doctype, declaration_node);

      if (!stylesheet_node)
        stylesheet_node = m_doc->insert_child_after(pugi::node_pi, declaration_node);

      if (!balg::starts_with(m_simplified_sub_charset, "utf"))
        declaration_node.append_attribute("encoding").set_value(m_sub_charset.c_str());
      doctype_node.set_value("USFSubtitles SYSTEM \"USFV100.dtd\"");
      stylesheet_node.set_name("xml-stylesheet");
      stylesheet_node.append_attribute("type").set_value("text/xsl");
      stylesheet_node.append_attribute("href").set_value("USFV100.xsl");

    } catch (mtx::mm_io::exception &) {
      mxerror(boost::format(Y("Failed to create the file '%1%': %2% (%3%)\n")) % m_file_name % errno % strerror(errno));

    } catch (mtx::xml::exception &ex) {
      mxerror(boost::format(Y("Failed to parse the USF codec private data for track %1%: %2%\n")) % m_tid % ex.what());
    }
  }
}

void
xtr_usf_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *,
                        int64_t timecode,
                        int64_t duration,
                        int64_t,
                        int64_t,
                        bool,
                        bool,
                        bool) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  usf_entry_t entry("", timecode, timecode + duration);
  entry.m_text.append((const char *)frame->get_buffer(), frame->get_size());
  m_entries.push_back(entry);
}

void
xtr_usf_c::finish_track() {
  auto subtitles = m_doc->document_element().append_child("subtitles");
  subtitles.append_child("language").append_attribute("code").set_value(m_language.c_str());

  for (auto &entry : m_entries) {
    std::string text = std::string{"<subtitle>"} + entry.m_text + "</subtitle>";
    strip(text, true);

    std::stringstream text_in(text);
    pugi::xml_document subtitle_doc;
    if (!subtitle_doc.load(text_in, pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype | pugi::parse_pi | pugi::parse_comments)) {
      mxwarn(boost::format(Y("Track %1%: An USF subtitle entry starting at timecode %2% is not well-formed XML and will be skipped.\n")) % m_tid % format_timecode(entry.m_start * 1000000, 3));
      continue;
    }

    auto subtitle = subtitles.append_child("subtitle");
    subtitle.append_attribute("start").set_value(format_timecode(entry.m_start * 1000000, 3).c_str());
    subtitle.append_attribute("stop"). set_value(format_timecode(entry.m_end   * 1000000, 3).c_str());

    for (auto child : subtitle_doc.document_element())
      subtitle.append_copy(child);
  }
}

void
xtr_usf_c::finish_file() {
  if (nullptr != m_master)
    return;

  auto is_utf   = true;
  auto encoding = pugi::encoding_utf8;

  if (m_simplified_sub_charset == "utf8")
    ;
  else if (m_simplified_sub_charset == "utf16le")
    encoding = pugi::encoding_utf16_le;
  else if (m_simplified_sub_charset == "utf16be")
    encoding = pugi::encoding_utf16_be;
  else if (m_simplified_sub_charset == "utf16")
    encoding = pugi::encoding_utf16;

  else if (m_simplified_sub_charset == "utf32le")
    encoding = pugi::encoding_utf32_le;
  else if (m_simplified_sub_charset == "utf32be")
    encoding = pugi::encoding_utf32_be;
  else if (m_simplified_sub_charset == "utf32")
    encoding = pugi::encoding_utf32;

  else
    is_utf = false;

  std::stringstream out;
  m_doc->save(out, "  ", pugi::format_default | (is_utf ? pugi::format_write_bom : 0), encoding);

  m_out->puts(is_utf ? out.str() : charset_converter_c::init(m_sub_charset)->native(out.str()));
}
