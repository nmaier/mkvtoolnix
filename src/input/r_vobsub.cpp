/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   VobSub stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/hacks.h"
#include "common/iso639.h"
#include "common/endian.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "input/r_vobsub.h"
#include "input/subtitles.h"
#include "merge/output_control.h"
#include "output/p_vobsub.h"


#define hexvalue(c) (  isdigit(c)        ? (c) - '0' \
                     : tolower(c) == 'a' ? 10        \
                     : tolower(c) == 'b' ? 11        \
                     : tolower(c) == 'c' ? 12        \
                     : tolower(c) == 'd' ? 13        \
                     : tolower(c) == 'e' ? 14        \
                     :                     15)
#define ishexdigit(s) (isdigit(s) || (strchr("abcdefABCDEF", s) != nullptr))

const std::string vobsub_reader_c::id_string("# VobSub index file, v");

bool
vobsub_entry_c::operator < (const vobsub_entry_c &cmp) const {
  return timestamp < cmp.timestamp;
}

int
vobsub_reader_c::probe_file(mm_io_c *in,
                            uint64_t) {
  char chunk[80];

  try {
    in->setFilePointer(0, seek_beginning);
    if (in->read(chunk, 80) != 80)
      return 0;
    if (strncasecmp(chunk, id_string.c_str(), id_string.length()))
      return 0;
    in->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

vobsub_reader_c::vobsub_reader_c(const track_info_c &ti,
                                 const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , delay(0)
{
}

void
vobsub_reader_c::read_headers() {
  try {
    m_idx_file = mm_text_io_cptr(new mm_text_io_c(m_in.get_object(), false));
  } catch (...) {
    throw mtx::input::open_x();
  }

  std::string sub_name = m_ti.m_fname;
  size_t len           = sub_name.rfind(".");
  if (std::string::npos != len)
    sub_name.erase(len);
  sub_name += ".sub";

  try {
    m_sub_file = mm_file_io_cptr(new mm_file_io_c(sub_name));
  } catch (...) {
    throw mtx::input::extended_x(boost::format(Y("%1%: Could not open the sub file")) % get_format_name());
  }

  idx_data = "";
  len      = id_string.length();

  std::string line;
  if (!m_idx_file->getline2(line) || !ba::istarts_with(line, id_string) || (line.length() < (len + 1)))
    mxerror_fn(m_ti.m_fname, Y("No version number found.\n"));

  version = line[len] - '0';
  len++;
  while ((len < line.length()) && isdigit(line[len])) {
    version = version * 10 + line[len] - '0';
    len++;
  }
  if (version < 7)
    mxerror_fn(m_ti.m_fname, Y("Only v7 and newer VobSub files are supported. If you have an older version then use the VSConv utility from "
                               "http://sourceforge.net/projects/guliverkli/ to convert these files to v7 files.\n"));

  parse_headers();
  show_demuxer_info();
}

vobsub_reader_c::~vobsub_reader_c() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++) {
    mxverb(2,
           boost::format("r_vobsub track %1% SPU size: %2%, overall size: %3%, overhead: %4% (%|5$.3f|%%)\n")
           % i % tracks[i]->spu_size % (tracks[i]->spu_size + tracks[i]->overhead) % tracks[i]->overhead
           % (float)(100.0 * tracks[i]->overhead / (tracks[i]->overhead + tracks[i]->spu_size)));
    delete tracks[i];
  }
}

void
vobsub_reader_c::create_packetizer(int64_t tid) {
  if ((tracks.size() <= static_cast<size_t>(tid)) || !demuxing_requested('s', tid) || (-1 != tracks[tid]->ptzr))
    return;

  vobsub_track_c *track = tracks[tid];
  m_ti.m_id             = tid;
  m_ti.m_language       = tracks[tid]->language;
  track->ptzr           = add_packetizer(new vobsub_packetizer_c(this, idx_data.c_str(), idx_data.length(), m_ti));

  int64_t avg_duration;
  if (!track->entries.empty()) {
    avg_duration = 0;
    size_t k;
    for (k = 0; k < (track->entries.size() - 1); k++) {
      int64_t duration            = track->entries[k + 1].timestamp - track->entries[k].timestamp;
      track->entries[k].duration  = duration;
      avg_duration               += duration;
    }
  } else
    avg_duration = 1000000000;

  if (1 < track->entries.size())
    avg_duration /= (track->entries.size() - 1);
  track->entries[track->entries.size() - 1].duration = avg_duration;

  num_indices += track->entries.size();

  m_ti.m_language = "";
  show_packetizer_info(tid, PTZR(track->ptzr));
}

void
vobsub_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    create_packetizer(i);
}

void
vobsub_reader_c::parse_headers() {
  std::string language, line;
  vobsub_track_c *track  = nullptr;
  int64_t line_no        = 0;
  int64_t last_timestamp = 0;
  bool sort_required     = false;
  num_indices            = 0;
  indices_processed      = 0;

  m_idx_file->setFilePointer(0, seek_beginning);

  while (1) {
    if (!m_idx_file->getline2(line))
      break;
    line_no++;

    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    const char *sline = line.c_str();

    if (!strncasecmp(sline, "id:", 3)) {
      if (line.length() >= 6) {
        language        = sline[4];
        language       += sline[5];
        int lang_index  = map_to_iso639_2_code(language.c_str());
        if (-1 != lang_index)
          language = iso639_languages[lang_index].iso639_2_code;
        else
          language = "";
      } else
        language = "";

      if (nullptr != track) {
        if (track->entries.empty())
          delete track;
        else {
          tracks.push_back(track);
          if (sort_required) {
            mxverb(2, boost::format("vobsub_reader: Sorting track %1%\n") % tracks.size());
            std::stable_sort(track->entries.begin(), track->entries.end());
          }
        }
      }
      track          = new vobsub_track_c(language);
      delay          = 0;
      last_timestamp = 0;
      sort_required  = false;
      continue;
    }

    if (!strncasecmp(sline, "alt:", 4) || !strncasecmp(sline, "langidx:", 8))
      continue;

    if (ba::istarts_with(line, "delay:")) {
      line.erase(0, 6);
      strip(line);

      int factor = 1;
      if (!line.empty() && (line[0] == '-')) {
        factor = -1;
        line.erase(0, 1);
      }

      int64_t timestamp;
      if (!parse_timecode(line, timestamp, true))
        mxerror_fn(m_ti.m_fname, boost::format(Y("line %1%: The 'delay' timestamp could not be parsed.\n")) % line_no);
      delay += timestamp * factor;
    }

    if ((7 == version) && ba::istarts_with(line, "timestamp:")) {
      if (nullptr == track)
        mxerror_fn(m_ti.m_fname, Y("The .idx file does not contain an 'id: ...' line to indicate the language.\n"));

      strip(line);
      shrink_whitespace(line);
      std::vector<std::string> parts = split(line.c_str(), " ");

      if ((4 != parts.size()) || (13 > parts[1].length()) || !ba::iequals(parts[2], "filepos:")) {
        mxwarn_fn(m_ti.m_fname, boost::format(Y("Line %1%: The line seems to be a subtitle entry but the format couldn't be recognized. This entry will be skipped.\n")) % line_no);
        continue;
      }

      int idx         = 0;
      sline           = parts[3].c_str();
      int64_t filepos = hexvalue(sline[idx]);
      idx++;
      while ((0 != sline[idx]) && ishexdigit(sline[idx])) {
        filepos = (filepos << 4) + hexvalue(sline[idx]);
        idx++;
      }

      parts[1].erase(parts[1].length() - 1);
      int factor = 1;
      if ('-' == parts[1][0]) {
        factor = -1;
        parts[1].erase(0, 1);
      }

      int64_t timestamp;
      if (!parse_timecode(parts[1], timestamp)) {
        mxwarn_fn(m_ti.m_fname,
                  boost::format(Y("Line %1%: The line seems to be a subtitle entry but the format couldn't be recognized. This entry will be skipped.\n")) % line_no);
        continue;
      }

      vobsub_entry_c entry;
      entry.position  = filepos;
      entry.timestamp = timestamp * factor + delay;

      if (   (0 >  delay)
          && (0 != last_timestamp)
          && (entry.timestamp < last_timestamp)) {
        delay           += last_timestamp - entry.timestamp;
        entry.timestamp  = last_timestamp;
      }

      if (0 > entry.timestamp) {
        mxwarn_fn(m_ti.m_fname,
                  boost::format(Y("Line %1%: The line seems to be a subtitle entry but the timecode was negative even after adding the track "
                                  "delay. Negative timecodes are not supported in Matroska. This entry will be skipped.\n")) % line_no);
        continue;
      }

      track->entries.push_back(entry);

      if ((entry.timestamp < last_timestamp) &&
          demuxing_requested('s', tracks.size())) {
        mxwarn_fn(m_ti.m_fname,
                  boost::format(Y("Line %1%: The current timestamp (%2%) is smaller than the previous one (%3%). "
                                  "The entries will be sorted according to their timestamps. "
                                  "This might result in the wrong order for some subtitle entries. "
                                  "If this is the case then you have to fix the .idx file manually.\n"))
                  % line_no % format_timecode(entry.timestamp, 3) % format_timecode(last_timestamp, 3));
        sort_required = true;
      }
      last_timestamp = entry.timestamp;

      continue;
    }

    idx_data += line;
    idx_data += "\n";
  }

  if (nullptr != track) {
    if (track->entries.size() == 0)
      delete track;
    else {
      tracks.push_back(track);
      if (sort_required) {
        mxverb(2, boost::format("vobsub_reader: Sorting track %1%\n") % tracks.size());
        std::stable_sort(track->entries.begin(), track->entries.end());
      }
    }
  }

  if (!g_identifying && (1 < verbose)) {
    size_t i, k, tsize = tracks.size();
    for (i = 0; i < tsize; i++) {
      mxinfo(boost::format("vobsub_reader: Track number %1%\n") % i);
      for (k = 0; k < tracks[i]->entries.size(); k++)
        mxinfo(boost::format("vobsub_reader:  %|1$04u| position: %|2$12d| (0x%|3$04x|%|4$08x|), timecode: %|5$12d| (%6%)\n")
               % k % tracks[i]->entries[k].position % (tracks[i]->entries[k].position >> 32) % (tracks[i]->entries[k].position & 0xffffffff)
               % (tracks[i]->entries[k].timestamp / 1000000) % format_timecode(tracks[i]->entries[k].timestamp, 3));
    }
  }
}

#define deliver() deliver_packet(dst_buf, dst_size, timecode, duration, PTZR(track->ptzr));
int
vobsub_reader_c::deliver_packet(unsigned char *buf,
                                int size,
                                int64_t timecode,
                                int64_t default_duration,
                                generic_packetizer_c *ptzr) {
  if ((nullptr == buf) || (0 == size)) {
    safefree(buf);
    return -1;
  }

  int64_t duration = spu_extract_duration(buf, size, timecode);
  if (1 == -duration) {
    duration = default_duration;
    mxverb(2, boost::format("vobsub_reader: Could not extract the duration for a SPU packet (timecode: %1%).") % format_timecode(timecode, 3));

    int dcsq  =                   get_uint16_be(&buf[2]);
    int dcsq2 = dcsq + 3 < size ? get_uint16_be(&buf[dcsq + 2]) : -1;

    // Some players ignore sub-pictures if there is no stop display command.
    // Add a stop display command only if 1 command chain exists and the hack is enabled.

    if ((dcsq == dcsq2) && hack_engaged(ENGAGE_VOBSUB_SUBPIC_STOP_CMDS)) {
      dcsq         += 2;        // points to first command chain
      dcsq2        += 4;        // find end of first command chain
      bool unknown  = false;
      unsigned char type;
      for (type = buf[dcsq2++]; 0xff != type; type = buf[dcsq2++]) {
        switch(type) {
          case 0x00:            // Menu ID, 1 byte
            break;
          case 0x01:            // Start display
            break;
          case 0x02:            // Stop display
            unknown = true;     // Can only have one Stop Display command
            break;
          case 0x03:            // Palette
            dcsq2 += 2;
            break;
          case 0x04:            // Alpha
            dcsq2 += 2;
            break;
          case 0x05:            // Coords
            dcsq2 += 6;
            break;
          case 0x06:            // Graphic lines
            dcsq2 += 4;
            break;
          default:
            unknown = true;
        }
      }

      if (!unknown) {
        size          += 6;
        uint32_t len   = (buf[0] << 8 | buf[1]) + 6; // increase sub-picture length by 6
        buf[0]         = (uint8_t)(len >> 8);
        buf[1]         = (uint8_t)(len);
        uint32_t stm   = duration / 1000000;         // calculate STM for Stop Display
        stm            = ((stm - 1) * 90 >> 10) + 1;
        buf[dcsq]      = (uint8_t)(dcsq2 >> 8);      // set pointer to 2nd chain
        buf[dcsq + 1]  = (uint8_t)(dcsq2);
        buf[dcsq2]     = (uint8_t)(stm >> 8);        // set DCSQ_STM
        buf[dcsq2 + 1] = (uint8_t)(stm);
        buf[dcsq2 + 2] = (uint8_t)(dcsq2 >> 8);      // last chain so point to itself
        buf[dcsq2 + 3] = (uint8_t)(dcsq2);
        buf[dcsq2 + 4] = 0x02;                       // stop display command
        buf[dcsq2 + 5] = 0xff;                       // end command
        mxverb(2, boost::format(" Added Stop Display cmd (SP_DCSQ_STM=0x%|1$04x|)") % stm);
      }
    }

    mxverb(2, boost::format("\n"));
  }

  if (2 != -duration)
    ptzr->process(new packet_t(new memory_c(buf, size, true), timecode, duration));
  else
    safefree(buf);

  return -1;
}

// Adopted from mplayer's vobsub.c
int
vobsub_reader_c::extract_one_spu_packet(int64_t track_id) {
  uint32_t len, idx, mpeg_version;
  int c, packet_aid;
  /* Goto start of a packet, it starts with 0x000001?? */
  const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];

  vobsub_track_c *track         = tracks[track_id];
  int64_t timecode              = track->entries[track->idx].timestamp;
  int64_t duration              = track->entries[track->idx].duration;
  uint64_t extraction_start_pos = track->entries[track->idx].position;
  uint64_t extraction_end_pos   = track->idx >= track->entries.size() - 1 ? m_sub_file->get_size() : track->entries[track->idx + 1].position;

  int64_t pts                   = 0;
  unsigned char *dst_buf        = nullptr;
  uint32_t dst_size             = 0;
  uint32_t packet_size          = 0;
  unsigned int spu_len          = 0;
  bool spu_len_valid            = false;

  m_sub_file->setFilePointer(extraction_start_pos);
  track->packet_num++;

  while (1) {
    if (spu_len_valid && ((dst_size >= spu_len) || (m_sub_file->getFilePointer() >= extraction_end_pos))) {
      if (dst_size != spu_len)
        mxverb(3,
               boost::format("r_vobsub.cpp: stddeliver spu_len different from dst_size; pts %5% spu_len %1% dst_size %2% curpos %3% endpos %4%\n")
               % spu_len % dst_size % m_sub_file->getFilePointer() % extraction_end_pos % format_timecode(pts));
      if (2 < dst_size)
        put_uint16_be(dst_buf, dst_size);

      return deliver();
    }
    if (m_sub_file->read(buf, 4) != 4)
      return deliver();
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
      c = m_sub_file->getch();
      if (0 > c)
        return deliver();
      memmove(buf, buf + 1, 3);
      buf[3] = c;
    }
    switch (buf[3]) {
      case 0xb9:                // System End Code
        return deliver();
        break;

      case 0xba:                // Packet start code
        c = m_sub_file->getch();
        if (0 > c)
          return deliver();
        if ((c & 0xc0) == 0x40)
          mpeg_version = 4;
        else if ((c & 0xf0) == 0x20)
          mpeg_version = 2;
        else {
          if (!track->mpeg_version_warning_printed) {
            mxwarn_tid(m_ti.m_fname, track_id,
                       boost::format(Y("Unsupported MPEG mpeg_version: 0x%|1$02x| in packet %2% for timecode %3%, assuming MPEG2. "
                                       "No further warnings will be printed for this track.\n"))
                       % c % track->packet_num % format_timecode(timecode, 3));
            track->mpeg_version_warning_printed = true;
          }
          mpeg_version = 2;
        }

        if (4 == mpeg_version) {
          if (!m_sub_file->setFilePointer2(9, seek_current))
            return deliver();
        } else if (2 == mpeg_version) {
          if (!m_sub_file->setFilePointer2(7, seek_current))
            return deliver();
        } else
          abort();
        break;

      case 0xbd:                // packet
        if (m_sub_file->read(buf, 2) != 2)
          return deliver();

        len = buf[0] << 8 | buf[1];
        idx = m_sub_file->getFilePointer() - extraction_start_pos;
        c   = m_sub_file->getch();

        if (0 > c)
          return deliver();
        if ((c & 0xC0) == 0x40) { // skip STD scale & size
          if (m_sub_file->getch() < 0)
            return deliver();
          c = m_sub_file->getch();
          if (0 > c)
            return deliver();
        }
        if ((c & 0xf0) == 0x20) // System-1 stream timestamp
          abort();
        else if ((c & 0xf0) == 0x30)
          abort();
        else if ((c & 0xc0) == 0x80) { // System-2 (.VOB) stream
          uint32_t pts_flags, hdrlen, dataidx;
          c = m_sub_file->getch();
          if (0 > c)
            return deliver();

          pts_flags = c;
          c         = m_sub_file->getch();
          if (0 > c)
            return deliver();

          hdrlen  = c;
          dataidx = m_sub_file->getFilePointer() - extraction_start_pos + hdrlen;
          if (dataidx > idx + len) {
            mxwarn_fn(m_ti.m_fname, boost::format(Y("Invalid header length: %1% (total length: %2%, idx: %3%, dataidx: %4%)\n")) % hdrlen % len % idx % dataidx);
            return deliver();
          }

          if ((pts_flags & 0xc0) == 0x80) {
            if (m_sub_file->read(buf, 5) != 5)
              return deliver();
            if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) && (buf[2] & 1) && (buf[4] & 1))) {
              mxwarn_fn(m_ti.m_fname, boost::format(Y("PTS error: 0x%|1$02x| %|2$02x|%|3$02x| %|4$02x|%|5$02x|\n")) % buf[0] % buf[1] % buf[2] % buf[3] % buf[4]);
              pts = 0;
            } else
              pts = ((int64_t)((buf[0] & 0x0e) << 29 | buf[1] << 22 | (buf[2] & 0xfe) << 14 | buf[3] << 7 | (buf[4] >> 1))) * 100000 / 9;
          }

          m_sub_file->setFilePointer2(dataidx + extraction_start_pos, seek_beginning);
          packet_aid = m_sub_file->getch();
          if (0 > packet_aid) {
            mxwarn_fn(m_ti.m_fname, boost::format(Y("Bogus aid %1%\n")) % packet_aid);
            return deliver();
          }

          packet_size = len - ((unsigned int)m_sub_file->getFilePointer() - extraction_start_pos - idx);
          if (-1 == track->aid)
            track->aid = packet_aid;
          else if (track->aid != packet_aid) {
            // The packet does not belong to the current subtitle stream.
            mxverb(3,
                   boost::format("vobsub_reader: skipping sub packet with aid %1% (wanted aid: %2%) with size %3% at %4%\n")
                   % packet_aid % track->aid % packet_size % (m_sub_file->getFilePointer() - extraction_start_pos));
            m_sub_file->skip(packet_size);
            idx = len;
            break;
          }

          if (hack_engaged(ENGAGE_VOBSUB_SUBPIC_STOP_CMDS)) {
            // Space for the stop display command and padding
            dst_buf = (unsigned char *)saferealloc(dst_buf, dst_size + packet_size + 6);
            memset(dst_buf + dst_size + packet_size, 0xff, 6);

          } else
            dst_buf = (unsigned char *)saferealloc(dst_buf, dst_size + packet_size);

          mxverb(3, boost::format("vobsub_reader: sub packet data: aid: %1%, pts: %2%, packet_size: %3%\n") % track->aid % format_timecode(pts, 3) % packet_size);
          if (m_sub_file->read(&dst_buf[dst_size], packet_size) != packet_size) {
            mxwarn(Y("vobsub_reader: sub file read failure"));
            return deliver();
          }
          if (!spu_len_valid) {
            spu_len       = get_uint16_be(dst_buf);
            spu_len_valid = true;
          }

          dst_size        += packet_size;
          track->spu_size += packet_size;
          track->overhead += m_sub_file->getFilePointer() - extraction_start_pos - packet_size;

          idx              = len;
        }
        break;

      case 0xbe:                // Padding
        if (m_sub_file->read(buf, 2) != 2)
          return deliver();
        len = buf[0] << 8 | buf[1];
        if ((0 < len) && !m_sub_file->setFilePointer2(len, seek_current))
          return deliver();
        break;

      default:
        if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
          // MPEG audio or video
          if (m_sub_file->read(buf, 2) != 2)
            return deliver();
          len = (buf[0] << 8) | buf[1];
          if ((0 < len) && !m_sub_file->setFilePointer2(len, seek_current))
            return deliver();

        } else {
          mxwarn_fn(m_ti.m_fname, boost::format(Y("Unknown header 0x%|1$02x|%|2$02x|%|3$02x|%|4$02x|\n")) % buf[0] % buf[1] % buf[2] % buf[3]);
          return deliver();
        }
    }
  }

  return deliver();
}

file_status_e
vobsub_reader_c::read(generic_packetizer_c *ptzr,
                      bool) {
  vobsub_track_c *track = nullptr;
  uint32_t id;

  for (id = 0; id < tracks.size(); ++id)
    if ((-1 != tracks[id]->ptzr) && (PTZR(tracks[id]->ptzr) == ptzr)) {
      track = tracks[id];
      break;
    }

  if (!track)
    return FILE_STATUS_DONE;

  if (track->idx >= track->entries.size())
    return flush_packetizers();

  extract_one_spu_packet(id);
  track->idx++;
  indices_processed++;

  return track->idx >= track->entries.size() ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

int
vobsub_reader_c::get_progress() {
  return 100 * indices_processed / num_indices;
}

void
vobsub_reader_c::identify() {
  std::vector<std::string> verbose_info;
  size_t i;

  id_result_container();

  for (i = 0; i < tracks.size(); i++) {
    verbose_info.clear();

    if (!tracks[i]->language.empty())
      verbose_info.push_back(std::string("language:") + tracks[i]->language);

    id_result_track(i, ID_RESULT_TRACK_SUBTITLES, "VobSub", verbose_info);
  }
}

file_status_e
vobsub_reader_c::flush_packetizers() {
  for (auto track : tracks)
    if (track->ptzr != -1)
      PTZR(track->ptzr)->flush();

  return FILE_STATUS_DONE;
}

void
vobsub_reader_c::add_available_track_ids() {
  add_available_track_id_range(tracks.size());
}
