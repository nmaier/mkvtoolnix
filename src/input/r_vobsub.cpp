/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   VobSub stream reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "iso639.h"
#include "mm_io.h"
#include "output_control.h"
#include "p_vobsub.h"
#include "r_vobsub.h"
#include "subtitles.h"

using namespace std;

#define hexvalue(c) (isdigit(c) ? (c) - '0' : \
                     tolower(c) == 'a' ? 10 : \
                     tolower(c) == 'b' ? 11 : \
                     tolower(c) == 'c' ? 12 : \
                     tolower(c) == 'd' ? 13 : \
                     tolower(c) == 'e' ? 14 : 15)
#define istimecodestr(s)       (!strncmp(s, "timecode: ", 10))
#define istimestampstr(s)      (!strncmp(s, "timestamp: ", 11))
#define iscommafileposstr(s)   (!strncmp(s, ", filepos: ", 11))
#define iscolon(s)             (*(s) == ':')
#define istwodigits(s)         (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s)       (isdigit(*(s)) && isdigit(*(s + 1)) && \
                                isdigit(*(s + 2)))
#define istwodigitscolon(s)    (istwodigits(s) && iscolon(s + 2))
#define istimecode(s)         (istwodigitscolon(s) && \
                                istwodigitscolon(s + 3) && \
                                istwodigitscolon(s + 6) && \
                                isthreedigits(s + 9))
#define ishexdigit(s)          (isdigit(s) || \
                                (strchr("abcdefABCDEF", s) != NULL))
#define isfilepos(s)           (ishexdigit(*(s)) && ishexdigit(*(s + 1)) && \
                                ishexdigit(*(s + 2)) && \
                                ishexdigit(*(s + 3)) && \
                                ishexdigit(*(s + 4)) && \
                                ishexdigit(*(s + 5)) && \
                                ishexdigit(*(s + 6)) && \
                                ishexdigit(*(s + 7)) && \
                                ishexdigit(*(s + 8)))
#define isvobsubline_v7(s)     ((strlen(s) >= 42) && \
                                istimestampstr(s) && istimecode(s + 11) && \
                                iscommafileposstr(s + 23) && \
                                isfilepos(s + 34))

#define PFX "vobsub_reader: "

bool
vobsub_entry_c::operator < (const vobsub_entry_c &cmp) const {
  return timestamp < cmp.timestamp;
}

int
vobsub_reader_c::probe_file(mm_io_c *mm_io,
                            int64_t size) {
  char chunk[80];

  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(chunk, 80) != 80)
      return 0;
    if (strncasecmp(chunk, "# VobSub index file, v",
                    strlen("# VobSub index file, v")))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

vobsub_reader_c::vobsub_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  string sub_name, line;
  int len;

  try {
    idx_file = new mm_text_io_c(new mm_file_io_c(ti.fname));
  } catch (...) {
    throw error_c(PFX "Cound not open the source file.");
  }

  sub_name = ti.fname;
  len = sub_name.rfind(".");
  if (len >= 0)
    sub_name.erase(len);
  sub_name += ".sub";

  try {
    sub_file = new mm_file_io_c(sub_name.c_str());
  } catch (...) {
    string emsg = PFX "Could not open the sub file '";
    emsg += sub_name;
    emsg += "'.";
    throw error_c(emsg.c_str());
  }

  idx_data = "";

  len = strlen("# VobSub index file, v");
  if (!idx_file->getline2(line) ||
      strncasecmp(line.c_str(), "# VobSub index file, v", len) ||
      (line.length() < (len + 1)))
    mxerror(PFX "%s: No version number found.\n", ti.fname.c_str());

  version = line[len] - '0';
  len++;
  while ((len < line.length()) && isdigit(line[len])) {
    version = version * 10 + line[len] - '0';
    len++;
  }
  if (version < 7)
    mxerror(PFX "%s: Only v7 and newer VobSub files are supported. If you "
            "have an older version then use the VSConv utility from "
            "http://sourceforge.net/projects/guliverkli/ to convert these "
            "files to v7 files.\n", ti.fname.c_str());

  parse_headers();
  if (verbose)
    mxinfo(FMT_FN "Using the VobSub subtitle reader (SUB file '%s').\n",
           ti.fname.c_str(), sub_name.c_str());
}

vobsub_reader_c::~vobsub_reader_c() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++) {
    mxverb(2, "r_vobsub track %u SPU size: %lld, overall size: %lld, "
           "overhead: %lld (%.3f%%)\n", i, tracks[i]->spu_size,
           tracks[i]->spu_size + tracks[i]->overhead,
           tracks[i]->overhead, (float)(100.0 * tracks[i]->overhead /
                                        (tracks[i]->overhead +
                                         tracks[i]->spu_size)));
    delete tracks[i];
  }
  delete sub_file;
  delete idx_file;
}

void
vobsub_reader_c::create_packetizer(int64_t tid) {
  uint32_t k;
  int64_t avg_duration, duration;
  char language[4];
  const char *c;
  vobsub_track_c *track;

  if ((tid < tracks.size()) && demuxing_requested('s', tid) &&
      (tracks[tid]->ptzr == -1)) {
    track = tracks[tid];
    ti.id = tid;
    if ((c = map_iso639_1_to_iso639_2(tracks[tid]->language)) != NULL) {
      strcpy(language, c);
      ti.language = language;
    } else
      ti.language = "";
    track->ptzr =
      add_packetizer(new vobsub_packetizer_c(this, idx_data.c_str(),
                                             idx_data.length(), ti));
    if (track->entries.size() > 0) {
      avg_duration = 0;
      for (k = 0; k < (track->entries.size() - 1); k++) {
        duration = track->entries[k + 1].timestamp -
          track->entries[k].timestamp;
        track->entries[k].duration = duration;
        avg_duration += duration;
      }
    } else
      avg_duration = 1000000000;

    if (track->entries.size() > 1)
      avg_duration /= (track->entries.size() - 1);
    track->entries[track->entries.size() - 1].duration = avg_duration;

    num_indices += track->entries.size();

    mxinfo(FMT_TID "Using the VobSub subtitle output module (language: %s).\n",
           ti.fname.c_str(), (int64_t)tid, track->language);
    ti.language = "";
  }
}

void
vobsub_reader_c::create_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    create_packetizer(i);
}

void
vobsub_reader_c::parse_headers() {
  string line;
  const char *sline;
  char language[3];
  vobsub_track_c *track;
  int64_t filepos, timestamp, line_no, last_timestamp;
  int hour, minute, second, msecond, idx;
  uint32_t i, k, tsize;
  vobsub_entry_c entry;
  bool sort_required;

  language[0] = 0;
  track = NULL;
  line_no = 0;
  last_timestamp = 0;
  sort_required = false;
  num_indices = 0;
  indices_processed = 0;

  while (1) {
    if (!idx_file->getline2(line))
      break;
    line_no++;

    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    sline = line.c_str();

    if (!strncasecmp(sline, "id:", 3)) {
      if (line.length() >= 6) {
        language[0] = sline[4];
        language[1] = sline[5];
        language[2] = 0;
      } else
        language[0] = 0;
      if (track != NULL) {
        if (track->entries.size() == 0)
          delete track;
        else {
          tracks.push_back(track);
          if (sort_required) {
            mxverb(2, PFX "Sorting track %u\n", tracks.size());
            stable_sort(track->entries.begin(), track->entries.end());
          }
        }
      }
      track = new vobsub_track_c(language);
      last_timestamp = 0;
      sort_required = false;
      continue;
    }

    if (!strncasecmp(sline, "alt:", 4) ||
        !strncasecmp(sline, "langidx:", 8))
      continue;

    if ((version == 7) && isvobsubline_v7(sline)) {
      if (track == NULL)
        mxerror(PFX ".idx file does not contain an 'id: ...' line to indicate "
                "the language.\n");

      idx = 34;
      filepos = hexvalue(sline[idx]);
      idx++;
      while ((idx < line.length()) && ishexdigit(sline[idx])) {
        filepos = filepos * 16 + hexvalue(sline[idx]);
        idx++;
      }
      entry.position = filepos;

      sscanf(&sline[11], "%02d:%02d:%02d:%03d", &hour, &minute, &second,
             &msecond);
      timestamp = (int64_t)hour * 60 * 60 * 1000 +
        (int64_t)minute * 60 * 1000 + (int64_t)second * 1000 +
        (int64_t)msecond;
      entry.timestamp = timestamp * 1000000;
      track->entries.push_back(entry);

      if ((timestamp < last_timestamp) &&
          demuxing_requested('s', tracks.size())) {
        mxwarn(PFX "'%s', line %lld: The current timestamp (" FMT_TIMECODE
               ") is smaller than the last one (" FMT_TIMECODE"). mkvmerge "
               "will sort the entries according to their timestamps. This "
               "might result in the wrong order for some subtitle entries. If "
               "this is the case then you have to fix the .idx file "
               "manually.\n", ti.fname.c_str(), line_no,
               ARG_TIMECODE(timestamp), ARG_TIMECODE(last_timestamp));
        sort_required = true;
      }
      last_timestamp = timestamp;

      continue;
    }

    idx_data += line;
    idx_data += "\n";
  }
  if (track != NULL) {
    if (track->entries.size() == 0)
      delete track;
    else {
      tracks.push_back(track);
      if (sort_required) {
        mxverb(2, PFX "Sorting track %u\n", tracks.size());
        stable_sort(track->entries.begin(), track->entries.end());
      }
    }
  }

  if (!identifying && (verbose > 1)) {
    tsize = tracks.size();
    for (i = 0; i < tsize; i++) {
      mxinfo("vobsub_reader: Track number %u\n", i);
      for (k = 0; k < tracks[i]->entries.size(); k++)
        mxinfo("vobsub_reader:  %04u position: %12lld (0x%04x%08x), "
               "timecode: %12lld (" FMT_TIMECODE ")\n", k,
               tracks[i]->entries[k].position,
               (uint32_t)(tracks[i]->entries[k].position >> 32),
               (uint32_t)(tracks[i]->entries[k].position & 0xffffffff),
               tracks[i]->entries[k].timestamp / 1000000,
               ARG_TIMECODE_NS(tracks[i]->entries[k].timestamp));
    }
  }
}

#define deliver() deliver_packet(dst_buf, dst_size, timecode, duration, \
                                 PTZR(track->ptzr));
int
vobsub_reader_c::deliver_packet(unsigned char *buf,
                                int size,
                                int64_t timecode,
                                int64_t default_duration,
                                generic_packetizer_c *ptzr) {
  int64_t duration;

  if ((buf == NULL) || (size == 0)) {
    safefree(buf);
    return -1;
  }

  duration = spu_extract_duration(buf, size, timecode);
  if (duration == -1) {
    mxverb(2, PFX "Could not extract the duration for a SPU packet in track "
           "%lld of '%s' (timecode: " FMT_TIMECODE ").\n", ti.id,
           ti.fname.c_str(), ARG_TIMECODE(timecode));
    duration = default_duration;
  }
  if (duration != -2) {
    memory_c mem(buf, size, true);
    ptzr->process(mem, timecode, duration);
  } else
    safefree(buf);

  return -1;
}

// Adopted from mplayer's vobsub.c
int
vobsub_reader_c::extract_one_spu_packet(int64_t timecode,
                                        int64_t duration,
                                        int64_t track_id) {
  unsigned char *dst_buf;
  uint32_t len, idx, version, packet_size, dst_size;
  int64_t extraction_start_pos;
  int c, packet_aid, spu_len;
  float pts;
  /* Goto start of a packet, it starts with 0x000001?? */
  const unsigned char wanted[] = { 0, 0, 1 };
  unsigned char buf[5];
  vobsub_track_c *track;

  track = tracks[track_id];
  extraction_start_pos = sub_file->getFilePointer();

  pts = 0.0;
  track->packet_num++;

  dst_buf = NULL;
  dst_size = 0;
  packet_size = 0;
  spu_len = -1;
  while (1) {
    if ((spu_len >= 0) && (dst_size >= spu_len))
      return deliver();
    if (sub_file->read(buf, 4) != 4)
      return deliver();
    while (memcmp(buf, wanted, sizeof(wanted)) != 0) {
      c = sub_file->getch();
      if (c < 0)
        return deliver();
      memmove(buf, buf + 1, 3);
      buf[3] = c;
    }
    switch (buf[3]) {
      case 0xb9:                // System End Code
        return deliver();
        break;

      case 0xba:                // Packet start code
        c = sub_file->getch();
        if (c < 0)
          return deliver();
        if ((c & 0xc0) == 0x40)
          version = 4;
        else if ((c & 0xf0) == 0x20)
          version = 2;
        else {
          if (!track->mpeg_version_warning_printed) {
            mxwarn(PFX "Unsupported MPEG version: 0x%02x in packet %lld for "
                   "track %lld for timecode " FMT_TIMECODE ", assuming "
                   "MPEG2. No further warnings will be printed for this "
                   "track.\n", c, track->packet_num, track_id,
                   ARG_TIMECODE_NS(timecode));
            track->mpeg_version_warning_printed = true;
          }
          version = 2;
        }

        if (version == 4) {
          if (!sub_file->setFilePointer2(9, seek_current))
            return deliver();
        } else if (version == 2) {
          if (!sub_file->setFilePointer2(7, seek_current))
            return deliver();
        } else
          abort();
        break;

      case 0xbd:                // packet
        if (sub_file->read(buf, 2) != 2)
          return deliver();
        len = buf[0] << 8 | buf[1];
        idx = sub_file->getFilePointer() - extraction_start_pos;
        c = sub_file->getch();
        if (c < 0)
          return deliver();
        if ((c & 0xC0) == 0x40) { // skip STD scale & size
          if (sub_file->getch() < 0)
            return deliver();
          c = sub_file->getch();
          if (c < 0)
            return deliver();
        }
        if ((c & 0xf0) == 0x20) { // System-1 stream timestamp
          abort();
        } else if ((c & 0xf0) == 0x30) {
          abort();
        } else if ((c & 0xc0) == 0x80) { // System-2 (.VOB) stream
          uint32_t pts_flags, hdrlen, dataidx;
          c = sub_file->getch();
          if (c < 0)
            return deliver();
          pts_flags = c;
          c = sub_file->getch();
          if (c < 0)
            return deliver();
          hdrlen = c;
          dataidx = sub_file->getFilePointer() - extraction_start_pos + hdrlen;
          if (dataidx > idx + len) {
            mxwarn(PFX "Invalid header length: %d (total length: %d, "
                   "idx: %d, dataidx: %d)\n", hdrlen, len, idx, dataidx);
            return deliver();
          }
          if ((pts_flags & 0xc0) == 0x80) {
            if (sub_file->read(buf, 5) != 5)
              return deliver();
            if (!(((buf[0] & 0xf0) == 0x20) && (buf[0] & 1) &&
                  (buf[2] & 1) && (buf[4] & 1))) {
              mxwarn(PFX "PTS error: 0x%02x %02x%02x %02x%02x \n",
                     buf[0], buf[1], buf[2], buf[3], buf[4]);
              pts = 0;
            } else
              pts = ((buf[0] & 0x0e) << 29 | buf[1] << 22 |
                     (buf[2] & 0xfe) << 14 | buf[3] << 7 | (buf[4] >> 1));
          }
          sub_file->setFilePointer2(dataidx + extraction_start_pos,
                                    seek_beginning);
          packet_aid = sub_file->getch();
          if (packet_aid < 0) {
            mxwarn(PFX "Bogus aid %d\n", packet_aid);
            return deliver();
          }
          packet_size = len - ((unsigned int)sub_file->getFilePointer() -
                               extraction_start_pos - idx);
          if (track->aid == -1)
            track->aid = packet_aid;
          else if (track->aid != packet_aid) {
            // The packet does not belong to the current subtitle stream.
            mxverb(3, PFX "skipping sub packet with aid %d (wanted aid: %d) "
                   "with size %d at %lld\n", packet_aid, track->aid,
                   packet_size, sub_file->getFilePointer() -
                   extraction_start_pos);
            sub_file->skip(packet_size);
            idx = len;
            break;
          }
          dst_buf = (unsigned char *)saferealloc(dst_buf, dst_size +
                                                 packet_size);
          mxverb(3, PFX "sub packet data: aid: %d, pts: %.3fs, packet_size: "
                 "%u\n", track->aid, pts, packet_size);
          if (sub_file->read(&dst_buf[dst_size], packet_size) != packet_size) {
            mxwarn(PFX "sub_file->read failure");
            return deliver();
          }
          if (spu_len == -1)
            spu_len = get_uint16_be(dst_buf);
          dst_size += packet_size;
          track->spu_size += packet_size;
          track->overhead += sub_file->getFilePointer() -
            extraction_start_pos - packet_size;

          idx = len;
        }
        break;

      case 0xbe:                // Padding
        if (sub_file->read(buf, 2) != 2)
          return deliver();
        len = buf[0] << 8 | buf[1];
        if ((len > 0) && !sub_file->setFilePointer2(len, seek_current))
          return deliver();
        break;

      default:
        if ((0xc0 <= buf[3]) && (buf[3] < 0xf0)) {
          // MPEG audio or video
          if (sub_file->read(buf, 2) != 2)
            return deliver();
          len = (buf[0] << 8) | buf[1];
          if ((len > 0) && !sub_file->setFilePointer2(len, seek_current))
            return deliver();

        } else {
          mxwarn(PFX "unknown header 0x%02X%02X%02X%02X\n", buf[0], buf[1],
                 buf[2], buf[3]);
          return deliver();
        }
    }
  }

  return deliver();
}

file_status_e
vobsub_reader_c::read(generic_packetizer_c *ptzr,
                      bool force) {
  vobsub_track_c *track;
  uint32_t i, id;

  track = NULL;
  for (i = 0; i < tracks.size(); i++)
    if (PTZR(tracks[i]->ptzr) == ptzr) {
      track = tracks[i];
      break;
    }

  if ((track == NULL) || (track->idx >= track->entries.size()))
    return FILE_STATUS_DONE;

  id = i;
  sub_file->setFilePointer(track->entries[track->idx].position);
  extract_one_spu_packet(track->entries[track->idx].timestamp,
                         track->entries[track->idx].duration, id);
  track->idx++;
  indices_processed++;

  if (track->idx >= track->entries.size()) {
    flush_packetizers();
    return FILE_STATUS_DONE;
  } else
    return FILE_STATUS_MOREDATA;
}

int
vobsub_reader_c::get_progress() {
  return 100 * indices_processed / num_indices;
}

void
vobsub_reader_c::identify() {
  uint32_t i;
  string info;
  const char *language;

  mxinfo("File '%s': container: VobSub\n", ti.fname.c_str());
  for (i = 0; i < tracks.size(); i++) {
    if (identify_verbose) {
      language = map_iso639_1_to_iso639_2(tracks[i]->language);
      if (language != NULL)
        info = " [language:" + string(language) + "]";
      else
        info = "";
    } else
      info = "";
    mxinfo("Track ID %u: subtitles (VobSub)%s\n", i, info.c_str());
  }
}

void
vobsub_reader_c::flush_packetizers() {
  uint32_t i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i]->ptzr != -1)
      PTZR(tracks[i]->ptzr)->flush();
}

void
vobsub_reader_c::add_available_track_ids() {
  int i;

  for (i = 0; i < tracks.size(); i++)
    available_track_ids.push_back(i);
}
