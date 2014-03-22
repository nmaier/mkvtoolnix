/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   subtitle helper

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/extern_data.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "input/subtitles.h"
#include "merge/output_control.h"
#include "merge/packet_extensions.h"

// ------------------------------------------------------------

void
subtitles_c::process(generic_packetizer_c *p) {
  if (empty() || (entries.end() == current))
    return;

  packet_cptr packet(new packet_t(new memory_c((unsigned char *)current->subs.c_str(), 0, false), current->start, current->end - current->start));
  packet->extensions.push_back(packet_extension_cptr(new subtitle_number_packet_extension_c(current->number)));
  p->process(packet);
  ++current;
}

// ------------------------------------------------------------

#define SRT_RE_VALUE         "\\s*(-?)\\s*(\\d+)"
#define SRT_RE_TIMECODE      SRT_RE_VALUE ":" SRT_RE_VALUE ":" SRT_RE_VALUE "[,\\.:]" SRT_RE_VALUE
#define SRT_RE_TIMECODE_LINE "^" SRT_RE_TIMECODE "\\s*[\\-\\s]+>\\s*" SRT_RE_TIMECODE "\\s*"
#define SRT_RE_COORDINATES   "([XY]\\d+:\\d+\\s*){4}\\s*$"

bool
srt_parser_c::probe(mm_text_io_c *io) {
  try {
    io->setFilePointer(0, seek_beginning);
    std::string s;
    do {
      s = io->getline();
      strip(s);
    } while (s.empty());

    int64_t dummy;
    if (!parse_number(s, dummy))
      return false;

    s = io->getline();
    boost::regex timecode_re(SRT_RE_TIMECODE_LINE, boost::regex::perl);
    boost::smatch matches;
    if (!boost::regex_search(s, timecode_re))
      return false;

    s = io->getline();
    io->setFilePointer(0, seek_beginning);

  } catch (...) {
    return false;
  }

  return true;
}

srt_parser_c::srt_parser_c(mm_text_io_c *io,
                           const std::string &file_name,
                           int64_t tid)
  : m_io(io)
  , m_file_name(file_name)
  , m_tid(tid)
  , m_coordinates_warning_shown(false)
{
}

void
srt_parser_c::parse() {
  boost::regex timecode_re(SRT_RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex number_re("^\\d+$", boost::regex::perl);
  boost::regex coordinates_re(SRT_RE_COORDINATES, boost::regex::perl);

  int64_t start                 = 0;
  int64_t end                   = 0;
  int64_t previous_start        = 0;
  bool timecode_warning_printed = false;
  parser_state_e state          = STATE_INITIAL;
  int line_number               = 0;
  unsigned int subtitle_number  = 0;
  unsigned int timecode_number  = 0;
  std::string subtitles;

  m_io->setFilePointer(0, seek_beginning);

  while (1) {
    std::string s;
    if (!m_io->getline2(s))
      break;

    line_number++;
    strip_back(s);

    if (s.empty()) {
      if ((STATE_INITIAL == state) || (STATE_TIME == state))
        continue;

      state = STATE_SUBS_OR_NUMBER;

      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += "\n";
      continue;
    }

    if (STATE_INITIAL == state) {
      if (!boost::regex_match(s, number_re)) {
        mxwarn_tid(m_file_name, m_tid, boost::format(Y("Error in line %1%: expected subtitle number and found some text.\n")) % line_number);
        break;
      }
      state = STATE_TIME;
      parse_number(s, subtitle_number);

    } else if (STATE_TIME == state) {
      boost::smatch matches;
      if (!boost::regex_search(s, matches, timecode_re)) {
        mxwarn_tid(m_file_name, m_tid, boost::format(Y("Error in line %1%: expected a SRT timecode line but found something else. Aborting this file.\n")) % line_number);
        break;
      }

      int s_h = 0, s_min = 0, s_sec = 0, e_h = 0, e_min = 0, e_sec = 0;

      //        1         2       3      4        5     6             7    8
      // "\\s*(-?)\\s*(\\d+):\\s(-?)*(\\d+):\\s*(-?)(\\d+)[,\\.]\\s*(-?)(\\d+)?"

      parse_number(matches[ 2].str(), s_h);
      parse_number(matches[ 4].str(), s_min);
      parse_number(matches[ 6].str(), s_sec);
      parse_number(matches[10].str(), e_h);
      parse_number(matches[12].str(), e_min);
      parse_number(matches[14].str(), e_sec);

      std::string s_rest = matches[ 8].str();
      std::string e_rest = matches[16].str();

      auto neg_calculator = [&](size_t const start_idx) -> int64_t {
        int64_t neg = 1;
        for (size_t idx = start_idx; idx <= (start_idx + 6); idx += 2)
          neg *= matches[idx].str() == "-" ? -1 : 1;
        return neg;
      };

      int64_t s_neg = neg_calculator(1);
      int64_t e_neg = neg_calculator(9);

      if (boost::regex_search(s, coordinates_re) && !m_coordinates_warning_shown) {
        mxwarn_tid(m_file_name, m_tid,
                   Y("This file contains coordinates in the timecode lines. "
                     "Such coordinates are not supported by the Matroska SRT subtitle format. "
                     "The coordinates will be removed automatically.\n"));
        m_coordinates_warning_shown = true;
      }

      // The previous entry is done now. Append it to the list of subtitles.
      if (!subtitles.empty()) {
        strip_back(subtitles, true);
        add(start, end, timecode_number, subtitles.c_str());
      }

      // Calculate the start and end time in ns precision for the following entry.
      start  = (int64_t)s_h * 60 * 60 + s_min * 60 + s_sec;
      end    = (int64_t)e_h * 60 * 60 + e_min * 60 + e_sec;

      start *= 1000000000ll * s_neg;
      end   *= 1000000000ll * e_neg;

      while (s_rest.length() < 9)
        s_rest += "0";
      if (s_rest.length() > 9)
        s_rest.erase(9);
      start += atol(s_rest.c_str());

      while (e_rest.length() < 9)
        e_rest += "0";
      if (e_rest.length() > 9)
        e_rest.erase(9);
      end += atol(e_rest.c_str());

      if (0 > start) {
        mxwarn_tid(m_file_name, m_tid,
                   boost::format(Y("Line %1%: Negative timestamp encountered. The entry will be adjusted to start from 00:00:00.000.\n")) % line_number);
        end   -= start;
        start  = 0;
        if (0 > end)
          end *= -1;
      }

      // There are files for which start timecodes overlap. Matroska requires
      // blocks to be sorted by their timecode. mkvmerge does this at the end
      // of this function, but warn the user that the original order is being
      // changed.
      if (!timecode_warning_printed && (start < previous_start)) {
        mxwarn_tid(m_file_name, m_tid, boost::format(Y("Warning in line %1%: The start timecode is smaller than that of the previous entry. "
                                                       "All entries from this file will be sorted by their start time.\n")) % line_number);
        timecode_warning_printed = true;
      }

      previous_start  = start;
      subtitles       = "";
      state           = STATE_SUBS;
      timecode_number = subtitle_number;

    } else if (STATE_SUBS == state) {
      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += s;

    } else if (boost::regex_match(s, number_re)) {
      state = STATE_TIME;
      parse_number(s, subtitle_number);

    } else {
      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += s;
    }
  }

  if (!subtitles.empty()) {
    strip_back(subtitles, true);
    add(start, end, timecode_number, subtitles.c_str());
  }

  sort();
}

// ------------------------------------------------------------

bool
ssa_parser_c::probe(mm_text_io_c *io) {
  boost::regex script_info_re("^\\s*\\[script\\s+info\\]",   boost::regex::perl | boost::regex::icase);
  boost::regex styles_re(     "^\\s*\\[V4\\+?\\s+Styles\\]", boost::regex::perl | boost::regex::icase);
  boost::regex comment_re(    "^\\s*$|^\\s*;",               boost::regex::perl | boost::regex::icase);

  try {
    int line_number = 0;
    io->setFilePointer(0, seek_beginning);

    std::string line;
    while (io->getline2(line)) {
      ++line_number;

      // Read at most 100 lines.
      if (100 < line_number)
        return false;

      // Skip comments and empty lines.
      if (boost::regex_search(line, comment_re))
        continue;

      // This is the line mkvmerge is looking for: positive match.
      if (boost::regex_search(line, script_info_re) || boost::regex_search(line, styles_re))
        return true;

      // Neither a wanted line nor an empty one/a comment: negative result.
      return false;
    }
  } catch (...) {
  }

  return false;
}

ssa_parser_c::ssa_parser_c(generic_reader_c *reader,
                           mm_text_io_c *io,
                           const std::string &file_name,
                           int64_t tid)
  : m_reader(reader)
  , m_io(io)
  , m_file_name(file_name)
  , m_tid(tid)
  , m_cc_utf8(charset_converter_c::init("UTF-8"))
  , m_is_ass(false)
  , m_attachment_id(0)
{
}

void
ssa_parser_c::parse() {
  boost::regex sec_styles_ass_re("^\\s*\\[V4\\+\\s+Styles\\]", boost::regex::perl | boost::regex::icase);
  boost::regex sec_styles_re(    "^\\s*\\[V4\\s+Styles\\]",    boost::regex::perl | boost::regex::icase);
  boost::regex sec_info_re(      "^\\s*\\[Script\\s+Info\\]",  boost::regex::perl | boost::regex::icase);
  boost::regex sec_events_re(    "^\\s*\\[Events\\]",          boost::regex::perl | boost::regex::icase);
  boost::regex sec_graphics_re(  "^\\s*\\[Graphics\\]",        boost::regex::perl | boost::regex::icase);
  boost::regex sec_fonts_re(     "^\\s*\\[Fonts\\]",           boost::regex::perl | boost::regex::icase);

  int num                        = 0;
  ssa_section_e section          = SSA_SECTION_NONE;
  ssa_section_e previous_section = SSA_SECTION_NONE;
  std::string name_field         = "Name";

  std::string attachment_name, attachment_data_uu;

  m_io->setFilePointer(0, seek_beginning);

  while (!m_io->eof()) {
    std::string line;
    if (!m_io->getline2(line))
      break;

    bool add_to_global = true;

    // A normal line. Let's see if this file is ASS and not SSA.
    if (!strcasecmp(line.c_str(), "ScriptType: v4.00+"))
      m_is_ass = true;

    else if (boost::regex_search(line, sec_styles_ass_re)) {
      m_is_ass = true;
      section  = SSA_SECTION_V4STYLES;

    } else if (boost::regex_search(line, sec_styles_re))
      section = SSA_SECTION_V4STYLES;

    else if (boost::regex_search(line, sec_info_re))
      section = SSA_SECTION_INFO;

    else if (boost::regex_search(line, sec_events_re))
      section = SSA_SECTION_EVENTS;

    else if (boost::regex_search(line, sec_graphics_re)) {
      section       = SSA_SECTION_GRAPHICS;
      add_to_global = false;

    } else if (boost::regex_search(line, sec_fonts_re)) {
      section       = SSA_SECTION_FONTS;
      add_to_global = false;

    } else if (SSA_SECTION_EVENTS == section) {
      if (balg::istarts_with(line, "Format: ")) {
        // Analyze the format string.
        m_format = split(&line.c_str()[strlen("Format: ")]);
        strip(m_format);

        // Let's see if "Actor" is used in the format instead of "Name".
        size_t i;
        for (i = 0; m_format.size() > i; ++i)
          if (balg::iequals(m_format[i], "actor")) {
            name_field = "Actor";
            break;
          }

      } else if (balg::istarts_with(line, "Dialogue: ")) {
        if (m_format.empty())
          throw mtx::input::extended_x(Y("ssa_reader: Invalid format. Could not find the \"Format\" line in the \"[Events]\" section."));

        std::string orig_line = line;

        line.erase(0, strlen("Dialogue: ")); // Trim the start.

        // Split the line into fields.
        std::vector<std::string> fields = split(line.c_str(), ",", m_format.size());
        while (fields.size() < m_format.size())
          fields.push_back(std::string(""));

        // Parse the start time.
        std::string stime = get_element("Start", fields);
        int64_t start     = parse_time(stime);
        if (0 > start) {
          mxwarn_tid(m_file_name, m_tid, boost::format(Y("Malformed line? (%1%)\n")) % orig_line);
          continue;
        }

        // Parse the end time.
        stime       = get_element("End", fields);
        int64_t end = parse_time(stime);
        if (0 > end) {
          mxwarn_tid(m_file_name, m_tid, boost::format(Y("Malformed line? (%1%)\n")) % orig_line);
          continue;
        }

        if (end < start) {
          mxwarn_tid(m_file_name, m_tid, boost::format(Y("Malformed line? (%1%)\n")) % orig_line);
          continue;
        }

        // Specs say that the following fields are to put into the block:
        // ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect,
        //   Text

        std::string comma = ",";
        line
          = to_string(num)                          + comma
          + get_element("Layer", fields)            + comma
          + get_element("Style", fields)            + comma
          + get_element(name_field.c_str(), fields) + comma
          + get_element("MarginL", fields)          + comma
          + get_element("MarginR", fields)          + comma
          + get_element("MarginV", fields)          + comma
          + get_element("Effect", fields)           + comma
          + recode_text(fields);

        add(start, end, num, line);
        num++;

        add_to_global = false;
      }

    } else if ((SSA_SECTION_FONTS == section) || (SSA_SECTION_GRAPHICS == section)) {
      if (balg::istarts_with(line, "fontname:")) {
        add_attachment_maybe(attachment_name, attachment_data_uu, section);

        line.erase(0, strlen("fontname:"));
        strip(line, true);
        attachment_name = line;

      } else {
        strip(line, true);
        attachment_data_uu += line;
      }

      add_to_global = false;
    }

    if (add_to_global) {
      m_global += line;
      m_global += "\r\n";
    }

    if (previous_section != section)
      add_attachment_maybe(attachment_name, attachment_data_uu, previous_section);

    previous_section = section;
  }

  sort();
}

std::string
ssa_parser_c::get_element(const char *index,
                          std::vector<std::string> &fields) {
  size_t i;

  for (i = 0; i < m_format.size(); i++)
    if (m_format[i] == index)
      return fields[i];

  return std::string("");
}

int64_t
ssa_parser_c::parse_time(std::string &stime) {
  int64_t th, tm, ts, tds;

  int pos = stime.find(':');
  if (0 > pos)
    return -1;

  std::string s = stime.substr(0, pos);
  if (!parse_number(s, th))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find(':');
  if (0 > pos)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_number(s, tm))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find('.');
  if (0 > pos)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_number(s, ts))
    return -1;
  stime.erase(0, pos + 1);

  if (!parse_number(stime, tds))
    return -1;

  return (tds * 10 + ts * 1000 + tm * 60 * 1000 + th * 60 * 60 * 1000) * 1000000;
}

std::string
ssa_parser_c::recode_text(std::vector<std::string> &fields) {
  return m_cc_utf8->utf8(get_element("Text", fields));
}

void
ssa_parser_c::add_attachment_maybe(std::string &name,
                                   std::string &data_uu,
                                   ssa_section_e section) {
  if (name.empty() || data_uu.empty() || ((SSA_SECTION_FONTS != section) && (SSA_SECTION_GRAPHICS != section))) {
    name    = "";
    data_uu = "";
    return;
  }

  ++m_attachment_id;

  if (!m_reader->attachment_requested(m_attachment_id)) {
    name    = "";
    data_uu = "";
    return;
  }

  attachment_t attachment;

  std::string short_name = m_file_name;
  size_t pos             = short_name.rfind('/');

  if (std::string::npos != pos)
    short_name.erase(0, pos + 1);
  pos = short_name.rfind('\\');
  if (std::string::npos != pos)
    short_name.erase(0, pos + 1);

  attachment.ui_id        = m_attachment_id;
  attachment.name         = m_cc_utf8->utf8(name);
  attachment.description  = (boost::format(SSA_SECTION_FONTS == section ? Y("Imported font from %1%") : Y("Imported picture from %1%")) % short_name).str();
  attachment.to_all_files = true;

  size_t data_size        = data_uu.length() % 4;
  data_size               = 3 == data_size ? 2 : 2 == data_size ? 1 : 0;
  data_size              += data_uu.length() / 4 * 3;
  attachment.data         = memory_c::alloc(data_size);
  auto out                = attachment.data->get_buffer();
  auto in                 = reinterpret_cast<unsigned char const *>(data_uu.c_str());

  for (auto end = in + (data_uu.length() / 4) * 4; in < end; in += 4, out += 3)
    decode_chars(in, out, 4);

  decode_chars(in, out, data_uu.length() % 4);

  attachment.mime_type = guess_mime_type(name, false);

  if (attachment.mime_type == "")
    attachment.mime_type = "application/octet-stream";

  add_attachment(attachment);

  name    = "";
  data_uu = "";
}

void
ssa_parser_c::decode_chars(unsigned char const *in,
                           unsigned char *out,
                           size_t bytes_in) {
  if (!bytes_in)
    return;

  size_t bytes_out = 4 == bytes_in ? 3 : 3 == bytes_in ? 2 : 1;
  uint32_t value   = 0;

  for (size_t idx = 0; idx < bytes_in; ++idx)
    value |= (static_cast<uint32_t>(in[idx]) - 33) << (6 * (3 - idx));

  for (size_t idx = 0; idx < bytes_out; ++idx)
    out[idx] = (value >> ((2 - idx) * 8)) & 0xff;
}

// ------------------------------------------------------------

int64_t
spu_extract_duration(unsigned char *data,
                     size_t buf_size,
                     int64_t timecode) {
  uint32_t date, control_start, next_off, start_off, off;
  unsigned char type;
  int duration;
  bool unknown;

  control_start = get_uint16_be(data + 2);
  next_off = control_start;
  duration = -1;
  start_off = 0;

  while ((start_off != next_off) && (next_off < buf_size)) {
    start_off = next_off;
    date = get_uint16_be(data + start_off) * 1024;
    next_off = get_uint16_be(data + start_off + 2);
    if (next_off < start_off) {
      mxwarn(boost::format(Y("spu_extraction_duration: Encountered broken SPU packet (next_off < start_off) at timecode %1%. "
                             "This packet might be displayed incorrectly or not at all.\n")) % format_timecode(timecode, 3));
      return -1;
    }
    mxverb(4, boost::format("spu_extraction_duration: date = %1%\n") % date);
    off = start_off + 4;
    for (type = data[off++]; type != 0xff; type = data[off++]) {
      mxverb(4, boost::format("spu_extraction_duration: cmd = %1% ") % type);
      unknown = false;
      switch(type) {
        case 0x00:
          /* Menu ID, 1 byte */
          mxverb(4, "menu ID");
          break;
        case 0x01:
          /* Start display */
          mxverb(4, "start display");
          break;
        case 0x02:
          /* Stop display */
          mxverb(4, boost::format("stop display: %1%") % (date / 90));
          return (int64_t)date * 1000000 / 90;
          break;
        case 0x03:
          /* Palette */
          mxverb(4, "palette");
          off+=2;
          break;
        case 0x04:
          /* Alpha */
          mxverb(4, "alpha");
          off+=2;
          break;
        case 0x05:
          mxverb(4, "coords");
          off+=6;
          break;
        case 0x06:
          mxverb(4, "graphic lines");
          off+=4;
          break;
        case 0xff:
          /* All done, bye-bye */
          mxverb(4, "done");
          return duration;
        default:
          mxverb(4, boost::format("unknown (0x%|1$02x|), skipping %2% bytes.") % type % (next_off - off));
          unknown = true;
      }
      mxverb(4, "\n");
      if (unknown)
        break;
    }
  }
  return duration;
}
