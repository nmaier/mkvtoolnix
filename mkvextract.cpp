/*
  mkvextract -- extract tracks from Matroska files into other files

  mkvextract.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief extracts tracks from Matroska files into other files
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

extern "C" {
#include "avilib/avilib.h"
}

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "EbmlStream.h"
#include "EbmlVoid.h"
#include "FileKax.h"

#include "KaxAttachements.h"
#include "KaxBlock.h"
#include "KaxBlockData.h"
#include "KaxChapters.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxCues.h"
#include "KaxCuesData.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTags.h"
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "common.h"
#include "matroska.h"
#include "mm_io.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

#define NAME "mkvextract"

class ssa_line_c {
public:
  char *line;
  int num;

  bool operator < (const ssa_line_c &cmp) const;
};

bool ssa_line_c::operator < (const ssa_line_c &cmp) const {
  return num < cmp.num;
}

typedef struct {
  char *out_name;

  mm_io_c *mm_io;
  avi_t *avi;
  ogg_stream_state osstate;

  int64_t tid;
  bool in_use;

  char track_type;
  int type;

  char *codec_id;
  void *private_data;
  int private_size;

  float a_sfreq;
  int a_channels, a_bps;

  float v_fps;
  int v_width, v_height;

  int srt_num;
  int conv_handle;
  vector<ssa_line_c> ssa_lines;

  wave_header wh;
  int64_t bytes_written;

  unsigned char *buffered_data;
  int buffered_size;
  int64_t packetno, last_end;
  int header_sizes[3];
  unsigned char *headers[3];
} mkv_track_t;

vector<mkv_track_t> tracks;

mkv_track_t *find_track(int tid) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i].tid == tid)
      return &tracks[i];

  return NULL;
}

char typenames[14][20] = {"unknown", "Ogg" "AVI", "WAV", "SRT", "MP3", "AC3",
                          "chapter", "MicroDVD", "VobSub", "Matroska", "DTS",
                          "AAC", "SSA/ASS"};

void usage() {
  fprintf(stdout,
    "Usage: mkvextract -i inname [TID1:outname1 [TID2:outname2 ...]]\n\n"
    "  -i inname      Use 'inname' as the source.\n"
    "  -c charset     Convert text subtitles to this charset.\n"
    "  TID:outname    Write track with the ID TID to 'outname'.\n"
    "\n"
    "  -v, --verbose  Increase verbosity.\n"
    "  -h, --help     Show this help.\n"
    "  -V, --version  Show version information.\n");
}

void parse_args(int argc, char **argv, char *&file_name) {
  int i, conv_handle;
  char *colon, *copy;
  int64_t tid;
  mkv_track_t track;

  file_name = NULL;
  verbose = 0;

  // Find options that directly end the program.
  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, VERSIONINFO "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
               !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    }

  conv_handle = 0;

  // Now process all the other options.
  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-i")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -i lacks a file name.\n");
        exit(1);
      } else if (file_name != NULL) {
        fprintf(stderr, "Error: Only one input file is allowed.\n");
        exit(1);
      }
      file_name = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "-c")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -c lacks a charset.\n");
        exit(1);
      }
      conv_handle = utf8_init(argv[i + 1]);
      i++;
    } else {
      copy = safestrdup(argv[i]);
      colon = strchr(copy, ':');
      if (colon == NULL) {
        fprintf(stderr, "Error: Missing track ID in argument '%s'.\n",
                argv[i]);
        exit(1);
      }
      *colon = 0;
      if (!parse_int(copy, tid) || (tid < 0)) {
        fprintf(stderr, "Error: Invalid track ID in argument '%s'.\n",
                argv[i]);
        exit(1);
      }
      colon++;
      if (*colon == 0) {
        fprintf(stderr, "Error: Missing output filename in argument '%s'.\n",
                argv[i]);
        exit(1);
      }
      memset(&track, 0, sizeof(mkv_track_t));
      track.tid = tid;
      track.out_name = safestrdup(colon);
      track.conv_handle = conv_handle;
      tracks.push_back(track);
      safefree(copy);
    }

  if (file_name == NULL) {
    fprintf(stderr, "Error: No input file given.\n\n");
    usage();
    exit(0);
  }

  if (tracks.size() == 0) {
    fprintf(stdout, "Nothing to do.\n\n");
    usage();
    exit(0);
  }
}

#define ARGS_BUFFER_LEN (200 * 1024) // Ok let's be ridiculous here :)
static char args_buffer[ARGS_BUFFER_LEN];

void show_element(EbmlElement *l, int level, const char *fmt, ...) {
  va_list ap;
  char level_buffer[10];

  if (level > 9)
    die("mkvextract.cpp/show_element(): level > 9: %d", level);

  if (verbose == 0)
    return;

  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, fmt, ap);
  va_end(ap);

  memset(&level_buffer[1], ' ', 9);
  level_buffer[0] = '|';
  level_buffer[level] = 0;
  fprintf(stdout, "(%s) %s+ %s", NAME, level_buffer, args_buffer);
  if (l != NULL)
    fprintf(stdout, " at %llu", l->GetElementPosition());
  fprintf(stdout, "\n");
}

void show_error(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, fmt, ap);
  va_end(ap);

  fprintf(stdout, "(%s) %s\n", NAME, args_buffer);
}

void flush_ogg_pages(mkv_track_t &track) {
  ogg_page page;

  while (ogg_stream_flush(&track.osstate, &page)) {
    track.mm_io->write(page.header, page.header_len);
    track.mm_io->write(page.body, page.body_len);
  }
}

void write_ogg_pages(mkv_track_t &track) {
  ogg_page page;

  while (ogg_stream_pageout(&track.osstate, &page)) {
    track.mm_io->write(page.header, page.header_len);
    track.mm_io->write(page.body, page.body_len);
  }
}

void create_output_files() {
  int i, k, offset;
  bool something_to_do, is_ok;
  unsigned char *c;
  ogg_packet op;

  something_to_do = false;

  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      tracks[i].in_use = false;

      if (tracks[i].codec_id == NULL) {
        fprintf(stdout, "Warning: Track ID %lld is missing the CodecID. "
                "Skipping track.\n", tracks[i].tid);
        continue;
      }

      // Video tracks: 
      if (tracks[i].track_type == 'v') {
        tracks[i].type = TYPEAVI;
        if ((tracks[i].v_width == 0) || (tracks[i].v_height == 0) ||
            (tracks[i].v_fps == 0.0) || (tracks[i].private_data == NULL) ||
            (tracks[i].private_size < sizeof(alBITMAPINFOHEADER))) {
          fprintf(stdout, "Warning: Track ID %lld is missing some critical "
                  "information. Skipping track.\n", tracks[i].tid);
          continue;
        }

        if (strcmp(tracks[i].codec_id, MKV_V_MSCOMP)) {
          fprintf(stdout, "Warning: Extraction of video tracks with a CodecId "
                  "other than " MKV_V_MSCOMP " is not supported at the "
                  "moment. Skipping track %lld.\n", tracks[i].tid);
          continue;
        }

      } else if (tracks[i].track_type == 'a') {
        if ((tracks[i].a_sfreq == 0.0) || (tracks[i].a_channels == 0)) {
          fprintf(stdout, "Warning: Track ID %lld is missing some critical "
                  "information. Skipping track.\n", tracks[i].tid);
          continue;
        }

        if (!strcmp(tracks[i].codec_id, MKV_A_VORBIS)) {
          tracks[i].type = TYPEOGM; // Yeah, I know, I know...
          if (tracks[i].private_data == NULL) {
            fprintf(stdout, "Warning: Track ID %lld is missing some critical "
                    "information. Skipping track.\n", tracks[i].tid);
            continue;
          }

          c = (unsigned char *)tracks[i].private_data;
          if (c[0] != 2) {
            fprintf(stderr, "Error: Vorbis track ID %lld does not contain "
                    "valid headers. Skipping track.\n", tracks[i].tid);
            continue;
          }

          is_ok = true;
          offset = 1;
          for (k = 0; k < 2; k++) {
            int length = 0;
            while ((c[offset] == (unsigned char)255) &&
                   (length < tracks[i].private_size)) {
              length += 255;
              offset++;
            }
            if (offset >= (tracks[i].private_size - 1)) {
              fprintf(stderr, "Error: Vorbis track ID %lld does not contain "
                      "valid headers. Skipping track.\n", tracks[i].tid);
              is_ok = false;
              break;
            }
            length += c[offset];
            offset++;
            tracks[i].header_sizes[k] = length;
          }

          if (!is_ok)
            continue;

          tracks[i].headers[0] = &c[offset];
          tracks[i].headers[1] = &c[offset + tracks[i].header_sizes[0]];
          tracks[i].headers[2] = &c[offset + tracks[i].header_sizes[0] +
                                    tracks[i].header_sizes[1]];
          tracks[i].header_sizes[2] = tracks[i].private_size -
            offset - tracks[i].header_sizes[0] - tracks[i].header_sizes[1];

        } else if (!strcmp(tracks[i].codec_id, MKV_A_MP3))
          tracks[i].type = TYPEMP3;
        else if (!strcmp(tracks[i].codec_id, MKV_A_AC3))
          tracks[i].type = TYPEAC3;
        else if (!strcmp(tracks[i].codec_id, MKV_A_PCM)) {
          tracks[i].type = TYPEWAV; // Yeah, I know, I know...
          if (tracks[i].a_bps == 0) {
            fprintf(stdout, "Warning: Track ID %lld is missing some critical "
                    "information. Skipping track.\n", tracks[i].tid);
            continue;
          }
        } else if (!strncmp(tracks[i].codec_id, "A_AAC", 5)) {
          fprintf(stdout, "Warning: Extraction of AAC is not supported - yet. "
                  "I promise I'll implement it. Really Soon Now (tm)! "
                  "Skipping track.\n");
          continue;
        } else if (!strcmp(tracks[i].codec_id, MKV_A_DTS)) {
          fprintf(stdout, "Warning: Extraction of DTS is not supported - yet. "
                  "I promise I'll implement it. Really Soon Now (tm)! "
                  "Skipping track.\n");
          continue;
        } else {
          fprintf(stdout, "Warning: Unsupported CodecID '%s' for ID %lld. "
                  "Skipping track.\n", tracks[i].codec_id, tracks[i].tid);
          continue;
        }

      } else if (tracks[i].track_type == 's') {
        if (!strcmp(tracks[i].codec_id, MKV_S_TEXTUTF8))
          tracks[i].type = TYPESRT;
        else if (!strcmp(tracks[i].codec_id, MKV_S_TEXTSSA) ||
                 !strcmp(tracks[i].codec_id, MKV_S_TEXTASS))
          tracks[i].type = TYPESSA;
        else {
          fprintf(stdout, "Warning: Unsupported CodecID '%s' for ID %lld. "
                  "Skipping track.\n", tracks[i].codec_id, tracks[i].tid);
          continue;
        }

      } else {
        fprintf(stdout, "Warning: Unknown track type for ID %lld. Skipping "
                "track.\n", tracks[i].tid);
        continue;
      }

      tracks[i].in_use = true;
      something_to_do = true;
    } else
      fprintf(stdout, "Warning: There is no track with the ID '%lld' in the "
              "input file.\n", tracks[i].tid);
  }

  if (!something_to_do) {
    fprintf(stdout, "Nothing to do. Exiting.\n");
    exit(0);
  }

  // Now finally create some files. Hell, I *WANT* to create something now.
  // RIGHT NOW! Or I'll go insane...
  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      if (tracks[i].type == TYPEAVI) {
        alBITMAPINFOHEADER *bih;
        char ccodec[5];

        tracks[i].avi = AVI_open_output_file(tracks[i].out_name);
        if (tracks[i].avi == NULL) {
          fprintf(stderr, "Error: Could not create '%s'. Reason: %s.\n",
                  tracks[i].out_name, AVI_strerror());
          exit(1);
        }

        bih = (alBITMAPINFOHEADER *)tracks[i].private_data;
        memcpy(ccodec, &bih->bi_compression, 4);
        ccodec[4] = 0;
        AVI_set_video(tracks[i].avi, tracks[i].v_width, tracks[i].v_height,
                      tracks[i].v_fps, ccodec);

        fprintf(stdout, "Extracting track ID %lld to an AVI file '%s'.\n",
                tracks[i].tid, tracks[i].out_name);

      } else {

        try {
          tracks[i].mm_io = new mm_io_c(tracks[i].out_name, MODE_CREATE);
        } catch (exception &ex) {
          fprintf(stderr, "Error: Could not create '%s'. Reason: %d (%s). "
                  "Aborting.\n", tracks[i].out_name, errno, strerror(errno));
          exit(1);
        }

        fprintf(stdout, "Extracting track ID %lld to a %s file '%s'.\n",
                tracks[i].tid, typenames[tracks[i].type - 1],
                tracks[i].out_name);

        if (tracks[i].type == TYPEOGM) {
          ogg_stream_init(&tracks[i].osstate, rand());

          // Handle the three header packets: Headers, comments, codec
          // setup data.
          op.b_o_s = 1;
          op.e_o_s = 0;
          op.packetno = 0;
          op.packet = tracks[i].headers[0];
          op.bytes = tracks[i].header_sizes[0];
          op.granulepos = 0;
          ogg_stream_packetin(&tracks[i].osstate, &op);
          flush_ogg_pages(tracks[i]);
          op.b_o_s = 0;
          op.packetno = 1;
          op.packet = tracks[i].headers[1];
          op.bytes = tracks[i].header_sizes[1];
          ogg_stream_packetin(&tracks[i].osstate, &op);
          op.packetno = 2;
          op.packet = tracks[i].headers[2];
          op.bytes = tracks[i].header_sizes[2];
          ogg_stream_packetin(&tracks[i].osstate, &op);
          flush_ogg_pages(tracks[i]);
          tracks[i].packetno = 3;

        } else if (tracks[i].type == TYPEWAV) {
          wave_header *wh = &tracks[i].wh;

          // Write the WAV header.
          memcpy(&wh->riff.id, "RIFF", 4);
          memcpy(&wh->riff.wave_id, "WAVE", 4);
          memcpy(&wh->format.id, "fmt ", 4);
          put_uint32(&wh->format.len, 16);
          put_uint16(&wh->common.wFormatTag, 1);
          put_uint16(&wh->common.wChannels, tracks[i].a_channels);
          put_uint32(&wh->common.dwSamplesPerSec, (int)tracks[i].a_sfreq);
          put_uint32(&wh->common.dwAvgBytesPerSec,
                     tracks[i].a_channels * (int)tracks[i].a_sfreq *
                     tracks[i].a_bps / 8);
          put_uint16(&wh->common.wBlockAlign, 4);
          put_uint16(&wh->common.wBitsPerSample, tracks[i].a_bps);
          memcpy(&wh->data.id, "data", 4);

          tracks[i].mm_io->write(wh, sizeof(wave_header));

        }  else if (tracks[i].type == TYPESRT)
          tracks[i].srt_num = 1;

        else if (tracks[i].type == TYPESSA) {
          char *s;

          s = (char *)safemalloc(tracks[i].private_size + 1);
          memcpy(s, tracks[i].private_data, tracks[i].private_size);
          s[tracks[i].private_size] = 0;
          tracks[i].mm_io->puts_unl(s);
          tracks[i].mm_io->puts_unl("\n[Events]\nFormat: Marked, Start, End, "
                                    "Style, Name, MarginL, MarginR, MarginV, "
                                    "Effect, Text\n");

          safefree(s);
        }
      }
    }
  }
}

void handle_data(KaxBlock *block, int64_t block_duration, bool has_ref) {
  mkv_track_t *track;
  int i, len, num;
  int64_t start, end;
  char *s, buffer[200], *s2;
  vector<string> fields;
  string line, comma = ",";
  ssa_line_c ssa_line;

  track = find_track(block->TrackNum());
  if ((track == NULL) || !track->in_use){
    delete block;
    return;
  }

  start = block->GlobalTimecode() / 1000000; // in ms
  end = start + block_duration;

  for (i = 0; i < block->NumberFrames(); i++) {
    DataBuffer &data = block->GetBuffer(i);
    switch (track->type) {
      case TYPEAVI:
        AVI_write_frame(track->avi, (char *)data.Buffer(), data.Size(),
                        has_ref ? 0 : 1);
        break;

      case TYPEOGM:
        if (track->buffered_data != NULL) {
          ogg_packet op;

          op.b_o_s = 0;
          op.e_o_s = 0;
          op.packetno = track->packetno;
          op.packet = track->buffered_data;
          op.bytes = track->buffered_size;
          op.granulepos = start * (int64_t)track->a_sfreq / 1000;
          ogg_stream_packetin(&track->osstate, &op);
          safefree(track->buffered_data);

          write_ogg_pages(*track);

          track->packetno++;
        }

        track->buffered_data = (unsigned char *)safememdup(data.Buffer(),
                                                           data.Size());
        track->buffered_size = data.Size();
        track->last_end = end;

        break;

      case TYPESRT:
        // Do the charset conversion.
        s = (char *)safemalloc(data.Size() + 1);
        memcpy(s, data.Buffer(), data.Size());
        s[data.Size()] = 0;
        s2 = from_utf8(tracks[i].conv_handle, s);
        safefree(s);
        len = strlen(s2);
        s = (char *)safemalloc(len + 3);
        strcpy(s, s2);
        safefree(s2);
        s[len] = '\n';
        s[len + 1] = '\n';
        s[len + 2] = 0;

        // Print the entry's number.
        sprintf(buffer, "%d\n", tracks[i].srt_num);
        tracks[i].srt_num++;
        tracks[i].mm_io->write(buffer, strlen(buffer));

        // Print the timestamps.
        sprintf(buffer, "%02lld:%02lld:%02lld,%03lld --> %02lld:%02lld:%02lld,"
                "%03lld\n",
                start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                (start / 1000) % 60, start % 1000,
                end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                (end / 1000) % 60, end % 1000);
        tracks[i].mm_io->write(buffer, strlen(buffer));

        // Print the text itself.
        tracks[i].mm_io->puts_unl(s);
        safefree(s);
        break;

      case TYPESSA:
        s = (char *)safemalloc(data.Size() + 1);
        memcpy(s, data.Buffer(), data.Size());
        s[data.Size()] = 0;
        
        // Split the line into the fields.
        // Specs say that the following fields are to put into the block:
        // 0: ReadOrder, 1: Layer, 2: Style, 3: Name, 4: MarginL, 5: MarginR,
        // 6: MarginV, 7: Effect, 8: Text
        fields = split(s, ",", 9);
        safefree(s);
        if (fields.size() != 9) {
          fprintf(stdout, "Warning: Invalid format for a SSA line ('%s'). "
                  "Ignoring this entry.\n", s);
          continue;
        }

        // Convert the ReadOrder entry so that we can re-order the entries
        // later.
        if (!parse_int(fields[0].c_str(), num)) {
          fprintf(stdout, "Warning: Invalid format for a SSA line ('%s'). "
                  "Ignoring this entry.\n", s);
          continue;
        }

        // Reconstruct the 'original' line. It'll look like this for SSA:
        // Marked, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect,
        // Text

        line = string("Dialogue: Marked=0,");

        // Append the start and end time.
        sprintf(buffer, "%lld:%02lld:%02lld.%02lld",
                start / 1000 / 60 / 60, (start / 1000 / 60) % 60,
                (start / 1000) % 60, (start % 1000) / 10);
        line += string(buffer) + comma;

        sprintf(buffer, "%lld:%02lld:%02lld.%02lld",
                end / 1000 / 60 / 60, (end / 1000 / 60) % 60,
                (end / 1000) % 60, (end % 1000) / 10);
        line += string(buffer) + comma;

        // Append the other fields.
        line += fields[2] + comma + // Style
          fields[3] + comma +   // Name
          fields[4] + comma +   // MarginL
          fields[5] + comma +   // MarginR
          fields[6] + comma +   // MarginV
          fields[7] + comma;    // Effect

        // Do the charset conversion.
        s = from_utf8(tracks[i].conv_handle, fields[8].c_str());
        line += string(s) + "\n";
        safefree(s);

        // Now store that entry.
        ssa_line.num = num;
        ssa_line.line = safestrdup(line.c_str());
        tracks[i].ssa_lines.push_back(ssa_line);

        break;

      default:
        track->mm_io->write(data.Buffer(), data.Size());
        track->bytes_written += data.Size();

    }
  }

  delete block;
}

void close_files() {
  int i, k;
  ogg_packet op;

  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].in_use) {
      switch (tracks[i].type) {
        case TYPEAVI:
          AVI_close(tracks[i].avi);
          break;

        case TYPEOGM:
          // Set the "end of stream" marker on the last packet, handle it
          // and flush all remaining Ogg pages.
          op.b_o_s = 0;
          op.e_o_s = 1;
          op.packetno = tracks[i].packetno;
          op.packet = tracks[i].buffered_data;
          op.bytes = tracks[i].buffered_size;
          op.granulepos = tracks[i].last_end * (int64_t)tracks[i].a_sfreq /
            1000;
          ogg_stream_packetin(&tracks[i].osstate, &op);
          safefree(tracks[i].buffered_data);

          flush_ogg_pages(tracks[i]);

          delete tracks[i].mm_io;

          break;

        case TYPEWAV:
          // Fix the header with the real number of bytes written.
          tracks[i].mm_io->setFilePointer(0);
          tracks[i].wh.riff.len = tracks[i].bytes_written + 36;
          tracks[i].wh.data.len = tracks[i].bytes_written;
          tracks[i].mm_io->write(&tracks[i].wh, sizeof(wave_header));
          delete tracks[i].mm_io;

          break;

        case TYPESSA:
          // Sort the SSA lines according to their ReadOrder number and
          // write them.
          sort(tracks[i].ssa_lines.begin(), tracks[i].ssa_lines.end());
          for (k = 0; k < tracks[i].ssa_lines.size(); k++) {
            tracks[i].mm_io->puts_unl(tracks[i].ssa_lines[k].line);
            safefree(tracks[i].ssa_lines[k].line);
          }
          delete tracks[i].mm_io;

          break;

        default:
          delete tracks[i].mm_io;
      }
    }
  }
}

bool process_file(const char *file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  KaxBlock *block;
  mkv_track_t *track;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size;
  char mkv_track_type;
  bool ms_compat, delete_element, has_reference;
  int64_t block_duration;
  mm_io_c *in;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error("Error: Couldn't open input file %s (%s).", file_name,
               strerror(errno));
    return false;
  }

  in->setFilePointer(0, seek_end);
  file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error("Error: No EBML head found.");
      delete es;

      return false;
    }
      
    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
      if (l0 == NULL) {
        show_error("No segment/level 0 element found.");
        return false;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, "Segment");
        break;
      }

      show_element(l0, 0, "Next level 0 element is not a segment but %s",
                   typeid(*l0).name());

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    upper_lvl_el = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while (l1 != NULL) {
      if (upper_lvl_el != 0)
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        show_element(l1, 1, "Segment information");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            show_element(l2, 2, "Timecode scale: %llu", tc_scale);
          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true, 1);
        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        show_element(l1, 1, "Segment tracks");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            show_element(l2, 2, "A track");

            track = NULL;
            mkv_track_type = '?';
            ms_compat = false;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                char *msg;

                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                if ((track = find_track(uint32(tnum))) == NULL)
                  msg = "extraction not requested";
                else {
                  msg = "will extract this track";
                  track->in_use = true;
                }
                show_element(l3, 3, "Track number: %u (%s)", uint32(tnum),
                             msg);

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());

                switch (uint8(ttype)) {
                  case track_audio:
                    mkv_track_type = 'a';
                    break;
                  case track_video:
                    mkv_track_type = 'v';
                    break;
                  case track_subtitle:
                    mkv_track_type = 's';
                    break;
                  default:
                    mkv_track_type = '?';
                    break;
                }
                show_element(l3, 3, "Track type: %s",
                             mkv_track_type == 'a' ? "audio" :
                             mkv_track_type == 'v' ? "video" :
                             mkv_track_type == 's' ? "subtitles" :
                             "unknown");
                if (track != NULL)
                  track->track_type = mkv_track_type;

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                show_element(l3, 3, "Audio track");

                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    show_element(l4, 4, "Sampling frequency: %f",
                                 float(freq));
                    if (track != NULL)
                      track->a_sfreq = float(freq);

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    show_element(l4, 4, "Channels: %u", uint8(channels));
                    if (track != NULL)
                      track->a_channels = uint8(channels);

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    show_element(l4, 4, "Bit depth: %u", uint8(bps));
                    if (track != NULL)
                      track->a_bps = uint8(bps);
                  }

                  l4->SkipData(*es,
                               l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true, 1);
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                show_element(l3, 3, "Video track");

                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel width: %u", uint16(width));
                    if (track != NULL)
                      track->v_width = uint16(width);

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel height: %u", uint16(height));
                    if (track != NULL)
                      track->v_height = uint16(height);

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    show_element(l4, 4, "Frame rate: %f", float(framerate));
                    if (track != NULL)
                      track->v_fps = float(framerate);

                  }

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true, 1);
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                show_element(l3, 3, "Codec ID: %s", string(codec_id).c_str());
                if ((!strcmp(string(codec_id).c_str(), MKV_V_MSCOMP) &&
                     (mkv_track_type == 'v')) ||
                    (!strcmp(string(codec_id).c_str(), MKV_A_ACM) &&
                     (mkv_track_type == 'a')))
                  ms_compat = true;
                if (track != NULL)
                  track->codec_id = safestrdup(string(codec_id).c_str());

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                char pbuffer[100];
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                if (ms_compat && (mkv_track_type == 'v') &&
                    (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
                  alBITMAPINFOHEADER *bih =
                    (alBITMAPINFOHEADER *)&binary(c_priv);
                  unsigned char *fcc = (unsigned char *)&bih->bi_compression;
                  sprintf(pbuffer, " (FourCC: %c%c%c%c, 0x%08x)",
                          fcc[0], fcc[1], fcc[2], fcc[3],
                          get_uint32(&bih->bi_compression));
                } else if (ms_compat && (mkv_track_type == 'a') &&
                           (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
                  alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)&binary(c_priv);
                  sprintf(pbuffer, " (format tag: 0x%04x)",
                          get_uint16(&wfe->w_format_tag));
                } else
                  pbuffer[0] = 0;
                show_element(l3, 3, "CodecPrivate, length %llu%s",
                             c_priv.GetSize(), pbuffer);
                if (track != NULL) {
                  track->private_data = safememdup(&binary(c_priv),
                                                   c_priv.GetSize());
                  track->private_size = c_priv.GetSize();
                }

              } else if (EbmlId(*l3) ==
                         KaxTrackDefaultDuration::ClassInfos.GlobalId) {
                KaxTrackDefaultDuration &def_duration =
                  *static_cast<KaxTrackDefaultDuration*>(l3);
                def_duration.ReadData(es->I_O());
                show_element(l3, 3, "Default duration: %.3fms (%.3f fps for "
                             "a video track)",
                             (float)uint64(def_duration) / 1000000.0,
                             1000000000.0 / (float)uint64(def_duration));
                if (track != NULL)
                  track->v_fps = 1000000000.0 / (float)uint64(def_duration);

              }

              if (upper_lvl_el > 0) {  // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;
              } else {
                l3->SkipData(*es,
                             l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          }

          if (upper_lvl_el > 0) {  // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

        // Headers have been parsed completely. Now create the output files
        // and stuff.
        create_output_files();

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        show_element(l1, 1, "Cluster");
        cluster = (KaxCluster *)l1;

        if (verbose == 0) {
          fprintf(stdout, "Progress: %d%%\r", (int)(in->getFilePointer() *
                                                    100 / file_size));
          fflush(stdout);
        }

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            show_element(l2, 2, "Cluster timecode: %.3fs", 
                         (float)cluster_tc * (float)tc_scale / 1000000000.0);
            cluster->InitTimecode(cluster_tc, tc_scale);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            show_element(l2, 2, "Block group");

            block_duration = 0;
            has_reference = false;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, false, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;

              delete_element = true;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                block = (KaxBlock *)l3;
                block->ReadData(es->I_O());
                block->SetParent(*cluster);
                show_element(l3, 3, "Block (track number %u, %d frame(s), "
                             "timecode %.3fs)", block->TrackNum(),
                             block->NumberFrames(),
                             (float)block->GlobalTimecode() / 1000000000.0);
                delete_element = false;

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());
                show_element(l3, 3, "Block duration: %.3fms",
                             ((float)uint64(duration)) * tc_scale / 1000000.0);
                block_duration = uint64(duration) * tc_scale / 1000000;

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &reference =
                  *static_cast<KaxReferenceBlock *>(l3);
                reference.ReadData(es->I_O());
                show_element(l3, 3, "Reference block: %.3fms", 
                             ((float)int64(reference)) * tc_scale / 1000000.0);

                has_reference = true;
              }

              l3->SkipData(*es, l3->Generic().Context);
              if (delete_element)
                delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)

            // Now write the stuff to the file. Or not. Or get something to
            // drink. Or read a good book. Dunno, your choice, really.
            handle_data(block, block_duration, has_reference);

          }

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)
      }

      if (upper_lvl_el > 0) {    // we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(*es, l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
      }
    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    // Now just close the files and go to sleep. Mummy will sing you a
    // lullaby. Just close your eyes, listen to her sweet voice, singing,
    // singing, fading... fad... ing...
    close_files();

    return true;
  } catch (exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;

    return false;
  }
}

int main(int argc, char **argv) {
  char *input_file;

#if defined(SYS_UNIX)
  nice(2);
#endif

  utf8_init(NULL);

  parse_args(argc, argv, input_file);
  process_file(input_file);

  if (verbose == 0)
    fprintf(stdout, "Progress: 100%%\n");

  utf8_done();

  return 0;
}
