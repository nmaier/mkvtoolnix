/*
  mkvinfo -- utility for gathering information about Matroska files

  mkvinfo.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief retrieves and displays information about a Matroska file
    \author Moritz Bunkus <moritz@bunkus.org>
*/

// {{{ includes

#include "os.h"

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
#include <typeinfo>

extern "C" {
#include <avilib.h>
}

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTagMulti.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "mkvinfo.h"
#include "mkvinfo_tag_types.h"
#include "common.h"
#include "matroska.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

// }}}

// {{{ structs, output/helper functions

typedef struct {
  unsigned int tnum, tuid;
  char type;
  int64_t size;
} kax_track_t;

kax_track_t **tracks = NULL;
int num_tracks = 0;
bool use_gui = false;
bool calc_checksums = false;

void add_track(kax_track_t *s) {
  tracks = (kax_track_t **)saferealloc(tracks, sizeof(kax_track_t *) *
                                   (num_tracks + 1));
  tracks[num_tracks] = s;
  num_tracks++;
}

kax_track_t *find_track(int tnum) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tnum == tnum)
      return tracks[i];

  return NULL;
}

kax_track_t *find_track_by_uid(int tuid) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tuid == tuid)
      return tracks[i];

  return NULL;
}

void usage() {
  mxprint(stdout,
    "Usage: mkvinfo [options] inname\n\n"
    " options:\n"
#ifdef HAVE_WXWINDOWS
    "  -g, --gui      Start the GUI (and open inname if it was given).\n"
#endif
    "  inname         Use 'inname' as the source.\n"
    "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
    "                 description of what mkvinfo outputs.\n"
    "  -c, --checksum Calculate and display checksums of frame contents.\n"
    "  -h, --help     Show this help.\n"
    "  -V, --version  Show version information.\n");
}

#define ARGS_BUFFER_LEN (200 * 1024) // Ok let's be ridiculous here :)
static char args_buffer[ARGS_BUFFER_LEN];

void show_error(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, fmt, ap);
  va_end(ap);

#ifdef HAVE_WXWINDOWS
  if (use_gui)
    frame->show_error(args_buffer);
  else
#endif
    mxprint(stdout, "(%s) %s\n", NAME, args_buffer);
}

void show_element(EbmlElement *l, int level, const char *fmt, ...) {
  va_list ap;
  char level_buffer[10];

  if (level > 9)
    die("mkvinfo.cpp/show_element(): level > 9: %d", level);

  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, fmt, ap);
  va_end(ap);

  if (!use_gui) {
    memset(&level_buffer[1], ' ', 9);
    level_buffer[0] = '|';
    level_buffer[level] = 0;
    mxprint(stdout, "%s+ %s", level_buffer, args_buffer);
    if ((verbose > 1) && (l != NULL))
      mxprint(stdout, " at %llu", l->GetElementPosition());
    mxprint(stdout, "\n");
  }
#ifdef HAVE_WXWINDOWS
  else {
    if (l != NULL)
      sprintf(&args_buffer[strlen(args_buffer)], " at %llu",
              l->GetElementPosition());
    frame->add_item(level, args_buffer);
  }
#endif // HAVE_WXWINDOWS
}

#define show_warning(l, f, args...) show_element(NULL, l, f, ## args)
#define show_unknown_element(e, l) \
  show_element(e, l, "Unknown element: %s", e->Generic().DebugName)

// }}}

// {{{ FUNCTION parse_args

void parse_args(int argc, char **argv, char *&file_name, bool &use_gui) {
  int i;

  verbose = 0;
  file_name = NULL;

  use_gui = false;

  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gui")) {
#ifndef HAVE_WXWINDOWS
      mxprint(stderr, "Error: mkvinfo was compiled without GUI support.\n");
      exit(1);
#else // HAVE_WXWINDOWS
      use_gui = true;
#endif // HAVE_WXWINDOWS
    } else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      mxprint(stdout, "mkvinfo v" VERSION "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-v") || ! strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
             !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--checksum"))
      calc_checksums = true;
    else if (file_name != NULL) {
      mxprint(stderr, "Error: Only one input file is allowed.\n");
      exit(1);
    } else
      file_name = argv[i];
}

// }}}

// {{{ parsing helper functions

bool is_ebmlvoid(EbmlElement *l, int level, int &upper_lvl_el) {
  if (upper_lvl_el < 0)
    upper_lvl_el = 0;

  if (EbmlId(*l) == EbmlVoid::ClassInfos.GlobalId) {
    show_element(l, level, "EbmlVoid");

    return true;
  }

  return false;
}

bool parse_multicomment(EbmlStream *es, EbmlElement *l0, int level,
                        int &upper_lvl_el, EbmlElement *&l_upper) {
  EbmlElement *l1;

  if (EbmlId(*l0) == KaxTagMultiComment::ClassInfos.GlobalId) {
    show_element(l0, level, "MultiComment");

    upper_lvl_el = 0;
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                             0xFFFFFFFFL, true, 1);
    while (l1 != NULL) {
      if (upper_lvl_el > 0) {
        upper_lvl_el = level - 3 + upper_lvl_el;
        l_upper = l1;
        return true;
      }

      if (EbmlId(*l1) == KaxTagMultiCommentName::ClassInfos.GlobalId) {
        KaxTagMultiCommentName &c_name =
          *static_cast<KaxTagMultiCommentName *>(l1);
        c_name.ReadData(es->I_O());
        show_element(l1, level + 1, "Name: %s", string(c_name).c_str());

      } else if (EbmlId(*l1) ==
                 KaxTagMultiCommentComments::ClassInfos.GlobalId) {
        char *str;

        KaxTagMultiCommentComments &c_comments =
          *static_cast<KaxTagMultiCommentComments *>(l1);
        c_comments.ReadData(es->I_O());
        str = UTFstring_to_cstr(UTFstring(c_comments));
        show_element(l1, level + 1, "Comments: %s", str);
        safefree(str);

      } else if (EbmlId(*l1) ==
                 KaxTagMultiCommentLanguage::ClassInfos.GlobalId) {
        KaxTagMultiCommentLanguage &c_language =
          *static_cast<KaxTagMultiCommentLanguage *>(l1);
        c_language.ReadData(es->I_O());
        show_element(l1, level + 1, "Language: %s",
                     string(c_language).c_str());

      } else if (!is_ebmlvoid(l1, level + 1, upper_lvl_el))
        show_unknown_element(l1, level + 1);

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      upper_lvl_el = 0;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true, 1);
    } // while (l1 != NULL)

    return true;

  } 

  return false;
}

#if defined(COMP_MSC) || defined(COMP_MINGW)
struct tm *gmtime_r(const time_t *timep, struct tm *result) {
  struct tm *aresult;

  aresult = gmtime(timep);
  memcpy(result, aresult, sizeof(struct tm));

  return result;
}

char *asctime_r(const struct tm *tm, char *buf) {
  char *abuf;

  abuf = asctime(tm);
  strcpy(buf, abuf);

  return abuf;
}
#endif

#define fits_parent(l, p) (l->GetElementPosition() < \
                           (p->GetElementPosition() + p->ElementSize()))

// }}}

#define BASE 65521
#define A0 check += *buffer++; sum2 += check;
#define A1 A0 A0
#define A2 A1 A1
#define A3 A2 A2
#define A4 A3 A3
#define A5 A4 A4
#define A6 A5 A5

uint32_t calc_adler32(const unsigned char *buffer, int size) {
  register uint32_t sum2, check;
  register int k;

  check = 1;
  k = size;

  sum2 = (check >> 16) & 0xffffL;
  check &= 0xffffL;
  while (k >= 64) {
    A6;
    k -= 64;
  }

  if (k)
    do {
      A0;
    } while (--k);

  check %= BASE;
  check |= (sum2 % BASE) << 16;

  return check;
}

bool process_file(const char *file_name) {
  int upper_lvl_el, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlElement *l5 = NULL, *l6 = NULL, *l7 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size, lf_timecode;
  int lf_tnum;
  char kax_track_type;
  bool ms_compat, bref_found, fref_found;
  char *str;
  string strc;
  mm_io_c *in;

  // open input file
  try {
    in = new mm_io_c(file_name, MODE_READ);
  } catch (std::exception &ex) {
    show_error("Error: Couldn't open input file %s (%s).\n", file_name,
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
      show_error("No EBML head found.");
      delete es;

      return false;
    }
    show_element(l0, 0, "EBML head");
      
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
      if (upper_lvl_el > 0)
        break;
      if ((upper_lvl_el < 0) && !fits_parent(l1, l0))
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        show_element(l1, 1, "Segment information");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            show_element(l2, 2, "Timecode scale: %llu", tc_scale);

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration &duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());
            show_element(l2, 2, "Duration: %.3fs",
                         float(duration) * tc_scale / 1000000000.0);

          } else if (EbmlId(*l2) == KaxMuxingApp::ClassInfos.GlobalId) {
            KaxMuxingApp &muxingapp = *static_cast<KaxMuxingApp *>(l2);
            muxingapp.ReadData(es->I_O());
            str = UTFstring_to_cstr(UTFstring(muxingapp));
            show_element(l2, 2, "Muxing application: %s", str);
            safefree(str);

          } else if (EbmlId(*l2) == KaxWritingApp::ClassInfos.GlobalId) {
            KaxWritingApp &writingapp = *static_cast<KaxWritingApp *>(l2);
            writingapp.ReadData(es->I_O());
            str = UTFstring_to_cstr(UTFstring(writingapp));
            show_element(l2, 2, "Writing application: %s", str);
            safefree(str);

          } else if (EbmlId(*l2) == KaxDateUTC::ClassInfos.GlobalId) {
            struct tm tmutc;
            time_t temptime;
            char buffer[40];
            KaxDateUTC &dateutc = *static_cast<KaxDateUTC *>(l2);
            dateutc.ReadData(es->I_O());
            temptime = dateutc.GetEpochDate();
            if ((gmtime_r(&temptime, &tmutc) != NULL) &&
                (asctime_r(&tmutc, buffer) != NULL)) {
              buffer[strlen(buffer) - 1] = 0;
              show_element(l2, 2, "Date: %s UTC", buffer);
            } else
              show_element(l2, 2, "Date (invalid, value: %d)", temptime);

          } else if (EbmlId(*l2) == KaxSegmentUID::ClassInfos.GlobalId) {
            KaxSegmentUID &uid = *static_cast<KaxSegmentUID *>(l2);
            uid.ReadData(es->I_O());
            char buffer[uid.GetSize() * 5 + 1];
            const unsigned char *b = (const unsigned char *)&binary(uid);
            buffer[0] = 0;
            for (i = 0; i < uid.GetSize(); i++)
              sprintf(&buffer[strlen(buffer)], " 0x%02x", b[i]);
            show_element(l2, 2, "Segment UID:%s", buffer);

          } else if (EbmlId(*l2) == KaxPrevUID::ClassInfos.GlobalId) {
            KaxPrevUID &uid = *static_cast<KaxPrevUID *>(l2);
            uid.ReadData(es->I_O());
            char buffer[uid.GetSize() * 5 + 1];
            const unsigned char *b = (const unsigned char *)&binary(uid);
            buffer[0] = 0;
            for (i = 0; i < uid.GetSize(); i++)
              sprintf(&buffer[strlen(buffer)], " 0x%02x", b[i]);
            show_element(l2, 2, "Previous segment UID:%s", buffer);

          } else if (EbmlId(*l2) == KaxNextUID::ClassInfos.GlobalId) {
            KaxNextUID &uid = *static_cast<KaxNextUID *>(l2);
            uid.ReadData(es->I_O());
            char buffer[uid.GetSize() * 5 + 1];
            const unsigned char *b = (const unsigned char *)&binary(uid);
            buffer[0] = 0;
            for (i = 0; i < uid.GetSize(); i++)
              sprintf(&buffer[strlen(buffer)], " 0x%02x", b[i]);
            show_element(l2, 2, "Next segment UID:%s", buffer);

          } else if (EbmlId(*l2) == KaxSegmentFilename::ClassInfos.GlobalId) {
            char *str;
            KaxSegmentFilename &filename =
              *static_cast<KaxSegmentFilename *>(l2);
            filename.ReadData(es->I_O());
            str = UTFstring_to_cstr(UTFstring(filename));
            show_element(l2, 2, "Segment filename: %s", str);
            safefree(str);

          } else if (EbmlId(*l2) == KaxTitle::ClassInfos.GlobalId) {
            char *str;
            KaxTitle &title = *static_cast<KaxTitle *>(l2);
            title.ReadData(es->I_O());
            str = UTFstring_to_cstr(UTFstring(title));
            show_element(l2, 2, "Title: %s", str);
            safefree(str);

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {  // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          } else {
            delete l2;
            l2 = l3;
          }

        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        show_element(l1, 1, "Segment tracks");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            show_element(l2, 2, "A track");

            kax_track_type = '?';
            ms_compat = false;

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                show_element(l3, 3, "Track number: %u", uint32(tnum));
                if (find_track(uint32(tnum)) != NULL)
                  show_warning(3, "Warning: There's more than one "
                               "track with the number %u.", uint32(tnum));

              } else if (EbmlId(*l3) == KaxTrackUID::ClassInfos.GlobalId) {
                KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
                tuid.ReadData(es->I_O());
                show_element(l3, 3, "Track UID: %u", uint32(tuid));
                if (find_track_by_uid(uint32(tuid)) != NULL)
                  show_warning(3, "Warning: There's more than one "
                               "track with the UID %u.", uint32(tuid));

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());

                switch (uint8(ttype)) {
                  case track_audio:
                    kax_track_type = 'a';
                    break;
                  case track_video:
                    kax_track_type = 'v';
                    break;
                  case track_subtitle:
                    kax_track_type = 's';
                    break;
                  default:
                    kax_track_type = '?';
                    break;
                }
                show_element(l3, 3, "Track type: %s",
                             kax_track_type == 'a' ? "audio" :
                             kax_track_type == 'v' ? "video" :
                             kax_track_type == 's' ? "subtitles" :
                             "unknown");

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                show_element(l3, 3, "Audio track");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) ==
                      KaxAudioSamplingFreq::ClassInfos.GlobalId) {
                    KaxAudioSamplingFreq &freq =
                      *static_cast<KaxAudioSamplingFreq*>(l4);
                    freq.ReadData(es->I_O());
                    show_element(l4, 4, "Sampling frequency: %f",
                                 float(freq));

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    show_element(l4, 4, "Channels: %u", uint8(channels));

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    show_element(l4, 4, "Bit depth: %u", uint8(bps));

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true);
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                show_element(l3, 3, "Video track");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel width: %u", uint16(width));

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    show_element(l4, 4, "Pixel height: %u", uint16(height));

                  } else if (EbmlId(*l4) ==
                      KaxVideoDisplayWidth::ClassInfos.GlobalId) {
                    KaxVideoDisplayWidth &width =
                      *static_cast<KaxVideoDisplayWidth *>(l4);
                    width.ReadData(es->I_O());
                    show_element(l4, 4, "Display width: %u", uint16(width));

                  } else if (EbmlId(*l4) ==
                             KaxVideoDisplayHeight::ClassInfos.GlobalId) {
                    KaxVideoDisplayHeight &height =
                      *static_cast<KaxVideoDisplayHeight *>(l4);
                    height.ReadData(es->I_O());
                    show_element(l4, 4, "Display height: %u", uint16(height));

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    show_element(l4, 4, "Frame rate: %f", float(framerate));

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true);
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                show_element(l3, 3, "Codec ID: %s", string(codec_id).c_str());
                if ((!strcmp(string(codec_id).c_str(), MKV_V_MSCOMP) &&
                     (kax_track_type == 'v')) ||
                    (!strcmp(string(codec_id).c_str(), MKV_A_ACM) &&
                     (kax_track_type == 'a')))
                  ms_compat = true;

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                char pbuffer[100];
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                if (ms_compat && (kax_track_type == 'v') &&
                    (c_priv.GetSize() >= sizeof(alBITMAPINFOHEADER))) {
                  alBITMAPINFOHEADER *bih =
                    (alBITMAPINFOHEADER *)&binary(c_priv);
                  unsigned char *fcc = (unsigned char *)&bih->bi_compression;
                  sprintf(pbuffer, " (FourCC: %c%c%c%c, 0x%08x)",
                          fcc[0], fcc[1], fcc[2], fcc[3],
                          get_uint32(&bih->bi_compression));
                } else if (ms_compat && (kax_track_type == 'a') &&
                           (c_priv.GetSize() >= sizeof(alWAVEFORMATEX))) {
                  alWAVEFORMATEX *wfe = (alWAVEFORMATEX *)&binary(c_priv);
                  sprintf(pbuffer, " (format tag: 0x%04x)",
                          get_uint16(&wfe->w_format_tag));
                } else
                  pbuffer[0] = 0;
                show_element(l3, 3, "CodecPrivate, length %d%s",
                             (int)c_priv.GetSize(), pbuffer);

              } else if (EbmlId(*l3) ==
                         KaxTrackMinCache::ClassInfos.GlobalId) {
                KaxTrackMinCache &min_cache =
                  *static_cast<KaxTrackMinCache*>(l3);
                min_cache.ReadData(es->I_O());
                show_element(l3, 3, "MinCache: %u", uint32(min_cache));

              } else if (EbmlId(*l3) ==
                         KaxTrackMaxCache::ClassInfos.GlobalId) {
                KaxTrackMaxCache &max_cache =
                  *static_cast<KaxTrackMaxCache*>(l3);
                max_cache.ReadData(es->I_O());
                show_element(l3, 3, "MaxCache: %u", uint32(max_cache));

              } else if (EbmlId(*l3) ==
                         KaxTrackDefaultDuration::ClassInfos.GlobalId) {
                KaxTrackDefaultDuration &def_duration =
                  *static_cast<KaxTrackDefaultDuration*>(l3);
                def_duration.ReadData(es->I_O());
                show_element(l3, 3, "Default duration: %.3fms (%.3f fps for "
                             "a video track)",
                             (float)uint64(def_duration) / 1000000.0,
                             1000000000.0 / (float)uint64(def_duration));

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagLacing::ClassInfos.GlobalId) {
                KaxTrackFlagLacing &f_lacing =
                  *static_cast<KaxTrackFlagLacing *>(l3);
                f_lacing.ReadData(es->I_O());
                show_element(l3, 3, "Lacing flag: %d", uint32(f_lacing));

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagDefault::ClassInfos.GlobalId) {
                KaxTrackFlagDefault &f_default =
                  *static_cast<KaxTrackFlagDefault *>(l3);
                f_default.ReadData(es->I_O());
                show_element(l3, 3, "Default flag: %d", uint32(f_default));

              } else if (EbmlId(*l3) ==
                         KaxTrackLanguage::ClassInfos.GlobalId) {
                KaxTrackLanguage &language =
                  *static_cast<KaxTrackLanguage *>(l3);
                language.ReadData(es->I_O());
                show_element(l3, 3, "Language: %s", string(language).c_str());

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) {  // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;
              } else if (upper_lvl_el == 0) {
                l3->SkipData(*es,
                             l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              } else {
                delete l3;
                l3 = l4;
              }

            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {  // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es, l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          } else {
            delete l2;
            l2 = l3;
          }


        } // while (l2 != NULL)

      } else if ((EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) &&
                 (verbose < 2) && !use_gui)
        show_element(l1, 1, "Seek head (subentries will be skipped)");
      else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        show_element(l1, 1, "Seek head");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxSeek::ClassInfos.GlobalId) {
            show_element(l2, 2, "Seek entry");

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              if (EbmlId(*l3) == KaxSeekID::ClassInfos.GlobalId) {
                binary *b;
                int s;
                char pbuffer[100];
                KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
                seek_id.ReadData(es->I_O());
                b = seek_id.GetBuffer();
                s = seek_id.GetSize();
                EbmlId id(b, s);
                pbuffer[0] = 0;
                for (i = 0; i < s; i++)
                  sprintf(&pbuffer[strlen(pbuffer)], "0x%02x ",
                          ((unsigned char *)b)[i]);
                show_element(l3, 3, "Seek ID: %s (%s)", pbuffer,
                             (id == KaxInfo::ClassInfos.GlobalId) ?
                             "KaxInfo" :
                             (id == KaxCluster::ClassInfos.GlobalId) ?
                             "KaxCluster" :
                             (id == KaxTracks::ClassInfos.GlobalId) ?
                             "KaxTracks" :
                             (id == KaxCues::ClassInfos.GlobalId) ?
                             "KaxCues" :
                             (id == KaxAttachments::ClassInfos.GlobalId) ?
                             "KaxAttachments" :
                             (id == KaxChapters::ClassInfos.GlobalId) ?
                             "KaxChapters" :
                             (id == KaxTags::ClassInfos.GlobalId) ?
                             "KaxTags" :
                             "unknown");

              } else if (EbmlId(*l3) == KaxSeekPosition::ClassInfos.GlobalId) {
                KaxSeekPosition &seek_pos =
                  static_cast<KaxSeekPosition &>(*l3);
                seek_pos.ReadData(es->I_O());
                show_element(l3, 3, "Seek position: %llu", uint64(seek_pos));

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el))
                show_unknown_element(l3, 3);

              if (upper_lvl_el == 0) {
                l3->SkipData(*es, l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true);
              }

            } // while (l3 != NULL)


          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);

          } else {
            delete l2;
            l2 = l3;
          }

        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        show_element(l1, 1, "Cluster");
        if (verbose == 0) {
          delete l1;
          delete l0;
          delete es;
          delete in;

          return true;
        }
        cluster = (KaxCluster *)l1;

#ifdef HAVE_WXWINDOWS
        if (use_gui)
          frame->show_progress(100 * cluster->GetElementPosition() /
                               file_size, "Parsing file");
#endif // HAVE_WXWINDOWS

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
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

            bref_found = false;
            fref_found = false;

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, false, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                char adler[100];
                KaxBlock &block = *static_cast<KaxBlock *>(l3);
                block.ReadData(es->I_O());
                block.SetParent(*cluster);
                show_element(l3, 3, "Block (track number %u, %d frame(s), "
                             "timecode %.3fs)", block.TrackNum(),
                             block.NumberFrames(),
                             (float)block.GlobalTimecode() / 1000000000.0);
                lf_timecode = block.GlobalTimecode() / 1000000;
                lf_tnum = block.TrackNum();
                for (i = 0; i < (int)block.NumberFrames(); i++) {
                  DataBuffer &data = block.GetBuffer(i);
                  if (calc_checksums)
                    sprintf(adler, " (adler: 0x%08x)",
                            calc_adler32(data.Buffer(), data.Size()));
                  else
                    adler[0] = 0;
                  show_element(NULL, 4, "Frame with size %u%s", data.Size(),
                               adler);
                }

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());
                show_element(l3, 3, "Block duration: %.3fms",
                             ((float)uint64(duration)) * tc_scale / 1000000.0);

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &reference =
                  *static_cast<KaxReferenceBlock *>(l3);
                reference.ReadData(es->I_O());
                show_element(l3, 3, "Reference block: %.3fms", 
                             ((float)int64(reference)) * tc_scale / 1000000.0);
                if (int64(reference) < 0)
                  bref_found = true;
                else if (int64(reference) > 0)
                  fref_found = true;

              } else if (EbmlId(*l3) ==
                         KaxReferencePriority::ClassInfos.GlobalId) {
                KaxReferencePriority &priority =
                  *static_cast<KaxReferencePriority *>(l3);
                priority.ReadData(es->I_O());
                show_element(l3, 3, "Reference priority: %u",
                             uint32(priority));

              } else if (EbmlId(*l3) == KaxSlices::ClassInfos.GlobalId) {
                show_element(l3, 3, "Slices");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, false, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTimeSlice::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Time slice");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, false,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxSliceLaceNumber::ClassInfos.GlobalId) {
                        KaxSliceLaceNumber slace_number =
                          *static_cast<KaxSliceLaceNumber *>(l5);
                        slace_number.ReadData(es->I_O());
                        show_element(l5, 5, "Lace number: %u",
                                     uint32(slace_number));

                      } else if (EbmlId(*l5) ==
                                 KaxSliceFrameNumber::ClassInfos.GlobalId) {
                        KaxSliceFrameNumber sframe_number =
                          *static_cast<KaxSliceFrameNumber *>(l5);
                        sframe_number.ReadData(es->I_O());
                        show_element(l5, 5, "Frame number: %u",
                                     uint32(sframe_number));

                      } else if (EbmlId(*l5) ==
                                 KaxSliceDelay::ClassInfos.GlobalId) {
                        KaxSliceDelay sdelay =
                          *static_cast<KaxSliceDelay *>(l5);
                        sdelay.ReadData(es->I_O());
                        show_element(l5, 5, "Delay: %.3fms",
                                     ((float)uint64(sdelay)) * tc_scale /
                                     1000000.0);

                      } else if (EbmlId(*l5) ==
                                 KaxSliceDuration::ClassInfos.GlobalId) {
                        KaxSliceDuration sduration =
                          *static_cast<KaxSliceDuration *>(l5);
                        sduration.ReadData(es->I_O());
                        show_element(l5, 5, "Duration: %.3fms",
                                     ((float)uint64(sduration)) * tc_scale /
                                     1000000.0);

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true);
                      }

                    } // while (l5 != NULL)

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true);
                  } else {
                    delete l3;
                    l3 = l4;
                  }

                } // while (l4 != NULL)

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) {    // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;

              } else if (upper_lvl_el == 0) {
                l3->SkipData(*es,
                             l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true);
              } else {
                delete l3;
                l3 = l4;
              }

            } // while (l3 != NULL)

            if (verbose > 2)
              show_element(NULL, 2, "[%c frame for track %u, timecode %u]",
                           bref_found && fref_found ? 'B' :
                           bref_found ? 'P' : !fref_found ? 'I' : '?',
                           lf_tnum, lf_timecode);

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          } else {
            delete l2;
            l2 = l3;
          }

        } // while (l2 != NULL)

      } else if ((EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) &&
                 (verbose < 2))
        show_element(l1, 1, "Cues (subentries will be skipped)");
      else if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) {
        show_element(l1, 1, "Cues");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxCuePoint::ClassInfos.GlobalId) {
            show_element(l2, 2, "Cue point");

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              if (EbmlId(*l3) == KaxCueTime::ClassInfos.GlobalId) {
                KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
                cue_time.ReadData(es->I_O());
                show_element(l3, 3, "Cue time: %.3fs", tc_scale * 
                             ((float)uint64(cue_time)) / 1000000000.0);

              } else if (EbmlId(*l3) ==
                         KaxCueTrackPositions::ClassInfos.GlobalId) {
                show_element(l3, 3, "Cue track positions");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxCueTrack::ClassInfos.GlobalId) {
                    KaxCueTrack &cue_track = *static_cast<KaxCueTrack *>(l4);
                    cue_track.ReadData(es->I_O());
                    show_element(l4, 4, "Cue track: %u", uint32(cue_track));

                  } else if (EbmlId(*l4) ==
                             KaxCueClusterPosition::ClassInfos.GlobalId) {
                    KaxCueClusterPosition &cue_cp =
                      *static_cast<KaxCueClusterPosition *>(l4);
                    cue_cp.ReadData(es->I_O());
                    show_element(l4, 4, "Cue cluster position: %llu",
                                 uint64(cue_cp));

                  } else if (EbmlId(*l4) ==
                             KaxCueBlockNumber::ClassInfos.GlobalId) {
                    KaxCueBlockNumber &cue_bn =
                      *static_cast<KaxCueBlockNumber *>(l4);
                    cue_bn.ReadData(es->I_O());
                    show_element(l4, 4, "Cue block number: %llu",
                                 uint64(cue_bn));

                  } else if (EbmlId(*l4) ==
                             KaxCueCodecState::ClassInfos.GlobalId) {
                    KaxCueCodecState &cue_cs =
                      *static_cast<KaxCueCodecState *>(l4);
                    cue_cs.ReadData(es->I_O());
                    show_element(l4, 4, "Cue codec state: %llu",
                                 uint64(cue_cs));

                  } else if (EbmlId(*l4) ==
                             KaxCueReference::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Cue reference");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) == KaxCueRefTime::ClassInfos.GlobalId) {
                        KaxCueRefTime &cue_rt =
                          *static_cast<KaxCueRefTime *>(l5);
                        show_element(l5, 5, "Cue ref time: %.3fs", tc_scale,
                                     ((float)uint64(cue_rt)) / 1000000000.0);

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefCluster::ClassInfos.GlobalId) {
                        KaxCueRefCluster &cue_rc =
                          *static_cast<KaxCueRefCluster *>(l5);
                        cue_rc.ReadData(es->I_O());
                        show_element(l5, 5, "Cue ref cluster: %llu",
                                     uint64(cue_rc));

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefNumber::ClassInfos.GlobalId) {
                        KaxCueRefNumber &cue_rn =
                          *static_cast<KaxCueRefNumber *>(l5);
                        cue_rn.ReadData(es->I_O());
                        show_element(l5, 5, "Cue ref number: %llu",
                                     uint64(cue_rn));

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefCodecState::ClassInfos.GlobalId) {
                        KaxCueRefCodecState &cue_rcs =
                          *static_cast<KaxCueRefCodecState *>(l5);
                        cue_rcs.ReadData(es->I_O());
                        show_element(l5, 5, "Cue ref codec state: %llu",
                                     uint64(cue_rcs));

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el))
                        show_unknown_element(l5, 5);
                      
                      if (upper_lvl_el == 0) {
                        l5->SkipData(*es,l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true);
                      }

                    } // while (l5 != NULL)

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es,
                                 l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) {    // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;

              } else if (upper_lvl_el == 0) {
                l3->SkipData(*es,
                             l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              } else {
                delete l3;
                l3 = l4;
              }

            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          } else {
            delete l2;
            l2 = l3;
          }

        } // while (l2 != NULL)

        // Weee! Attachments!
      } else if (EbmlId(*l1) == KaxAttachments::ClassInfos.GlobalId) {
        show_element(l1, 1, "Attachments");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxAttached::ClassInfos.GlobalId) {
            show_element(l2, 2, "Attached");

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              if (EbmlId(*l3) == KaxFileDescription::ClassInfos.GlobalId) {
                KaxFileDescription &f_desc =
                  *static_cast<KaxFileDescription *>(l3);
                f_desc.ReadData(es->I_O());
                str = UTFstring_to_cstr(UTFstring(f_desc));
                show_element(l3, 3, "File description: %s", str);
                safefree(str);

              } else if (EbmlId(*l3) == KaxFileName::ClassInfos.GlobalId) {
                KaxFileName &f_name =
                  *static_cast<KaxFileName *>(l3);
                f_name.ReadData(es->I_O());
                str = UTFstring_to_cstr(UTFstring(f_name));
                show_element(l3, 3, "File name: %s", str);
                safefree(str);

              } else if (EbmlId(*l3) == KaxMimeType::ClassInfos.GlobalId) {
                KaxMimeType &mime_type =
                  *static_cast<KaxMimeType *>(l3);
                mime_type.ReadData(es->I_O());
                show_element(l3, 3, "Mime type: %s",
                             string(mime_type).c_str());

              } else if (EbmlId(*l3) == KaxFileData::ClassInfos.GlobalId) {
                KaxFileData &f_data =
                  *static_cast<KaxFileData *>(l3);
                f_data.ReadData(es->I_O());
                show_element(l3, 3, "File data, size: %u", f_data.GetSize());

              } else if (EbmlId(*l3) == KaxFileUID::ClassInfos.GlobalId) {
                KaxFileUID &f_uid =
                  *static_cast<KaxFileUID *>(l3);
                f_uid.ReadData(es->I_O());
                show_element(l3, 3, "File UID: %u", uint32(f_uid));

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) { // we're coming from l4
                assert(1 == 0);

              } else if (upper_lvl_el == 0) {
                l3->SkipData(*es, l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true);
              } else {
                delete l3;
                l3 = l4;
              }

            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true);
          } else {
            delete l2;
            l2 = l3;
          }

        } // while (l2 != NULL)

        // Let's handle some TAGS.
      } else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId) {
        show_element(l1, 1, "Tags");

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el > 0)
            break;
          if ((upper_lvl_el < 0) && !fits_parent(l2, l1))
            break;

          if (EbmlId(*l2) == KaxTag::ClassInfos.GlobalId) {
            show_element(l2, 2, "Tag");

            upper_lvl_el = 0;
            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;
              if ((upper_lvl_el < 0) && !fits_parent(l3, l2))
                break;

              if (EbmlId(*l3) == KaxTagTargets::ClassInfos.GlobalId) {
                show_element(l3, 3, "Targets");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagTrackUID::ClassInfos.GlobalId) {
                    KaxTagTrackUID &trackuid =
                      *static_cast<KaxTagTrackUID *>(l4);
                    trackuid.ReadData(es->I_O());
                    show_element(l4, 4, "Track UID: %llu", uint64(trackuid));

                  } else if (EbmlId(*l4) ==
                             KaxTagChapterUID::ClassInfos.GlobalId) {
                    KaxTagChapterUID &chapteruid =
                      *static_cast<KaxTagChapterUID *>(l4);
                    chapteruid.ReadData(es->I_O());
                    show_element(l4, 4, "Chapter UID: %llu",
                                 uint64(chapteruid));

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTagGeneral::ClassInfos.GlobalId) {
                show_element(l3, 3, "General");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagSubject::ClassInfos.GlobalId) {
                    KaxTagSubject &subject =
                      *static_cast<KaxTagSubject *>(l4);
                    subject.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(subject));
                    show_element(l4, 4, "Subject: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagBibliography::ClassInfos.GlobalId) {
                    KaxTagBibliography &bibliography =
                      *static_cast<KaxTagBibliography *>(l4);
                    bibliography.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(bibliography));
                    show_element(l4, 4, "Bibliography: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagLanguage::ClassInfos.GlobalId) {
                    KaxTagLanguage &language =
                      *static_cast<KaxTagLanguage *>(l4);
                    language.ReadData(es->I_O());
                    show_element(l4, 4, "Language: %s",
                                 string(language).c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagRating::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagRating &rating =
                      *static_cast<KaxTagRating *>(l4);
                    rating.ReadData(es->I_O());
                    strc = "Rating: length " + to_string(rating.GetSize()) +
                      ", data: ";
                    b = &binary(rating);
                    for (i = 0; i < rating.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagEncoder::ClassInfos.GlobalId) {
                    KaxTagEncoder &encoder =
                      *static_cast<KaxTagEncoder *>(l4);
                    encoder.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(encoder));
                    show_element(l4, 4, "Encoder: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagEncodeSettings::ClassInfos.GlobalId) {
                    KaxTagEncodeSettings &encode_settings =
                      *static_cast<KaxTagEncodeSettings *>(l4);
                    encode_settings.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(encode_settings));
                    show_element(l4, 4, "Encode settings: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagFile::ClassInfos.GlobalId) {
                    KaxTagFile &file =
                      *static_cast<KaxTagFile *>(l4);
                    file.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(file));
                    show_element(l4, 4, "File: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagArchivalLocation::ClassInfos.GlobalId) {
                    KaxTagArchivalLocation &archival_location =
                      *static_cast<KaxTagArchivalLocation *>(l4);
                    archival_location.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(archival_location));
                    show_element(l4, 4, "Archival location: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagKeywords::ClassInfos.GlobalId) {
                    KaxTagKeywords &keywords =
                      *static_cast<KaxTagKeywords *>(l4);
                    keywords.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(keywords));
                    show_element(l4, 4, "Keywords: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagMood::ClassInfos.GlobalId) {
                    KaxTagMood &mood =
                      *static_cast<KaxTagMood *>(l4);
                    mood.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(mood));
                    show_element(l4, 4, "Mood: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagRecordLocation::ClassInfos.GlobalId) {
                    KaxTagRecordLocation &record_location =
                      *static_cast<KaxTagRecordLocation *>(l4);
                    record_location.ReadData(es->I_O());
                    show_element(l4, 4, "Record location: %s",
                                 string(record_location).c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagSource::ClassInfos.GlobalId) {
                    KaxTagSource &source =
                      *static_cast<KaxTagSource *>(l4);
                    source.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(source));
                    show_element(l4, 4, "Source: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagSourceForm::ClassInfos.GlobalId) {
                    KaxTagSourceForm &source_form =
                      *static_cast<KaxTagSourceForm *>(l4);
                    source_form.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(source_form));
                    show_element(l4, 4, "Source form: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagProduct::ClassInfos.GlobalId) {
                    KaxTagProduct &product =
                      *static_cast<KaxTagProduct *>(l4);
                    product.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(product));
                    show_element(l4, 4, "Product: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagOriginalMediaType::ClassInfos.GlobalId) {
                    KaxTagOriginalMediaType &original_media_type =
                      *static_cast<KaxTagOriginalMediaType *>(l4);
                    original_media_type.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(original_media_type));
                    show_element(l4, 4, "Original media type: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagEncoder::ClassInfos.GlobalId) {
                    KaxTagEncoder &encoder =
                      *static_cast<KaxTagEncoder *>(l4);
                    encoder.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(encoder));
                    show_element(l4, 4, "Encoder: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagPlayCounter::ClassInfos.GlobalId) {
                    KaxTagPlayCounter &play_counter =
                      *static_cast<KaxTagPlayCounter *>(l4);
                    play_counter.ReadData(es->I_O());
                    show_element(l4, 4, "Play counter: %llu", 
                                 uint64(play_counter));

                  } else if (EbmlId(*l4) ==
                             KaxTagPopularimeter::ClassInfos.GlobalId) {
                    KaxTagPopularimeter &popularimeter =
                      *static_cast<KaxTagPopularimeter *>(l4);
                    popularimeter.ReadData(es->I_O());
                    show_element(l4, 4, "Popularimeter: %lld", 
                                 int64(popularimeter));

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTagGenres::ClassInfos.GlobalId) {
                show_element(l3, 3, "Genres");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagAudioGenre::ClassInfos.GlobalId) {
                    KaxTagAudioGenre &audio_genre =
                      *static_cast<KaxTagAudioGenre *>(l4);
                    audio_genre.ReadData(es->I_O());
                    show_element(l4, 4, "Audio genre: %s",
                                 string(audio_genre).c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagVideoGenre::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagVideoGenre &video_genre =
                      *static_cast<KaxTagVideoGenre *>(l4);
                    video_genre.ReadData(es->I_O());
                    strc = "Video genre: length " +
                      to_string(video_genre.GetSize()) + ", data: ";
                    b = &binary(video_genre);
                    for (i = 0; i < video_genre.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagSubGenre::ClassInfos.GlobalId) {
                    KaxTagSubGenre &sub_genre =
                      *static_cast<KaxTagSubGenre *>(l4);
                    sub_genre.ReadData(es->I_O());
                    show_element(l4, 4, "Sub genre: %s",
                                 string(sub_genre).c_str());

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagAudioSpecific::ClassInfos.GlobalId) {
                show_element(l3, 3, "Audio specific");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) ==
                      KaxTagAudioEncryption::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagAudioEncryption &encryption =
                      *static_cast<KaxTagAudioEncryption *>(l4);
                    encryption.ReadData(es->I_O());
                    strc = "Audio encryption: length " +
                      to_string(encryption.GetSize()) + ", data: ";
                    b = &binary(encryption);
                    for (i = 0; i < encryption.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagAudioGain::ClassInfos.GlobalId) {
                    KaxTagAudioGain &audio_gain =
                      *static_cast<KaxTagAudioGain *>(l4);
                    audio_gain.ReadData(es->I_O());
                    show_element(l4, 4, "Audio gain: %.3f", float(audio_gain));

                  } else if (EbmlId(*l4) ==
                             KaxTagAudioPeak::ClassInfos.GlobalId) {
                    KaxTagAudioPeak &audio_peak =
                      *static_cast<KaxTagAudioPeak *>(l4);
                    audio_peak.ReadData(es->I_O());
                    show_element(l4, 4, "Audio peak: %.3f", float(audio_peak));

                  } else if (EbmlId(*l4) ==
                             KaxTagBPM::ClassInfos.GlobalId) {
                    KaxTagBPM &bpm = *static_cast<KaxTagBPM *>(l4);
                    bpm.ReadData(es->I_O());
                    show_element(l4, 4, "BPM: %.3f", float(bpm));

                  } else if (EbmlId(*l4) ==
                             KaxTagEqualisation::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagEqualisation &equalisation =
                      *static_cast<KaxTagEqualisation *>(l4);
                    equalisation.ReadData(es->I_O());
                    strc = "Equalisation: length " +
                      to_string(equalisation.GetSize()) + ", data: ";
                    b = &binary(equalisation);
                    for (i = 0; i < equalisation.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagDiscTrack::ClassInfos.GlobalId) {
                    KaxTagDiscTrack &disc_track =
                      *static_cast<KaxTagDiscTrack *>(l4);
                    disc_track.ReadData(es->I_O());
                    show_element(l4, 4, "Disc track: %u", uint32(disc_track));

                  } else if (EbmlId(*l4) ==
                             KaxTagSetPart::ClassInfos.GlobalId) {
                    KaxTagSetPart &set_part =
                      *static_cast<KaxTagSetPart *>(l4);
                    set_part.ReadData(es->I_O());
                    show_element(l4, 4, "Set part: %u", uint32(set_part));

                  } else if (EbmlId(*l4) ==
                             KaxTagInitialKey::ClassInfos.GlobalId) {
                    KaxTagInitialKey &initial_key =
                      *static_cast<KaxTagInitialKey *>(l4);
                    initial_key.ReadData(es->I_O());
                    show_element(l4, 4, "Initial key: %s",
                                 string(initial_key).c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagOfficialAudioFileURL::ClassInfos.GlobalId) {
                    KaxTagOfficialAudioFileURL &official_file_url =
                      *static_cast<KaxTagOfficialAudioFileURL *>(l4);
                    official_file_url.ReadData(es->I_O());
                    show_element(l4, 4, "Official audio file URL: %s",
                                 string(official_file_url).c_str());

                  } else if (EbmlId(*l4) == KaxTagOfficialAudioSourceURL::
                             ClassInfos.GlobalId) {
                    KaxTagOfficialAudioSourceURL &official_source_url =
                      *static_cast<KaxTagOfficialAudioSourceURL *>(l4);
                    official_source_url.ReadData(es->I_O());
                    show_element(l4, 4, "Official audio source URL: %s",
                                 string(official_source_url).c_str());

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagImageSpecific::ClassInfos.GlobalId) {
                show_element(l3, 3, "Image specific");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagCaptureDPI::ClassInfos.GlobalId) {
                    KaxTagCaptureDPI &capture_dpi =
                      *static_cast<KaxTagCaptureDPI *>(l4);
                    capture_dpi.ReadData(es->I_O());
                    show_element(l4, 4, "Capture DPI: %u",
                                 uint32(capture_dpi));

                  } else if (EbmlId(*l4) ==
                      KaxTagCaptureLightness::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagCaptureLightness &capture_lightness =
                      *static_cast<KaxTagCaptureLightness *>(l4);
                    capture_lightness.ReadData(es->I_O());
                    strc = "Capture lightness: length " +
                      to_string(capture_lightness.GetSize()) + ", data: ";
                    b = &binary(capture_lightness);
                    for (i = 0; i < capture_lightness.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) == KaxTagCapturePaletteSetting::
                             ClassInfos.GlobalId) {
                    KaxTagCapturePaletteSetting &capture_palette_setting =
                      *static_cast<KaxTagCapturePaletteSetting *>(l4);
                    capture_palette_setting.ReadData(es->I_O());
                    show_element(l4, 4, "Capture palette setting: %u",
                                 uint32(capture_palette_setting));

                  } else if (EbmlId(*l4) ==
                      KaxTagCaptureSharpness::ClassInfos.GlobalId) {
                    char buf[8];
                    const binary *b;

                    KaxTagCaptureSharpness &capture_sharpness =
                      *static_cast<KaxTagCaptureSharpness *>(l4);
                    capture_sharpness.ReadData(es->I_O());
                    strc = "Capture sharpness: length " +
                      to_string(capture_sharpness.GetSize()) + ", data: ";
                    b = &binary(capture_sharpness);
                    for (i = 0; i < capture_sharpness.GetSize(); i++) {
                      sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                      strc += buf;
                    }
                    show_element(l4, 4, strc.c_str());

                  } else if (EbmlId(*l4) ==
                             KaxTagCropped::ClassInfos.GlobalId) {
                    KaxTagCropped &cropped =
                      *static_cast<KaxTagCropped *>(l4);
                    cropped.ReadData(es->I_O());
                    str = UTFstring_to_cstr(UTFstring(cropped));
                    show_element(l4, 4, "Cropped: %s", str);
                    safefree(str);

                  } else if (EbmlId(*l4) ==
                             KaxTagOriginalDimensions::ClassInfos.GlobalId) {
                    KaxTagOriginalDimensions &original_dimensions =
                      *static_cast<KaxTagOriginalDimensions *>(l4);
                    original_dimensions.ReadData(es->I_O());
                    show_element(l4, 4, "Original dimensions: %s",
                                 string(original_dimensions).c_str());

                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiCommercial::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi commercial");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagCommercial::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Commercial");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiCommercialType::ClassInfos.GlobalId) {
                        int type;

                        KaxTagMultiCommercialType &c_type =
                          *static_cast<KaxTagMultiCommercialType *>(l5);
                        c_type.ReadData(es->I_O());
                        type = uint32(c_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_COMMERCIAL_TYPES) ?
                                     commercial_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiCommercialAddress::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiCommercialAddress &c_address =
                          *static_cast<KaxTagMultiCommercialAddress *>(l5);
                        c_address.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(c_address));
                        show_element(l5, 5, "Address: %s",
                                     str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiCommercialURL::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiCommercialURL &c_url =
                          *static_cast<KaxTagMultiCommercialURL *>(l5);
                        c_url.ReadData(es->I_O());
                        show_element(l5, 5, "URL: %s",
                                     string(c_url).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiCommercialEmail::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiCommercialEmail &c_email =
                          *static_cast<KaxTagMultiCommercialEmail *>(l5);
                        c_email.ReadData(es->I_O());
                        show_element(l5, 5, "Email: %s",
                                     string(c_email).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiPrice::ClassInfos.GlobalId) {
                        show_element(l5, 5, "Multi price");

                        upper_lvl_el = 0;
                        l6 = es->FindNextElement(l5->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                        while (l6 != NULL) {

                          if (upper_lvl_el > 0)
                            break;
                          if ((upper_lvl_el < 0) && !fits_parent(l6, l5))
                            break;

                          if (EbmlId(*l6) ==
                              KaxTagMultiPriceCurrency::ClassInfos.GlobalId) {
                            KaxTagMultiPriceCurrency &p_currency =
                              *static_cast<KaxTagMultiPriceCurrency *>(l6);
                            p_currency.ReadData(es->I_O());
                            show_element(l6, 6, "Currency: %s",
                                         string(p_currency).c_str());

                          } else if (EbmlId(*l6) ==
                                     KaxTagMultiPriceAmount::
                                     ClassInfos.GlobalId) {
                            KaxTagMultiPriceAmount &p_amount =
                              *static_cast<KaxTagMultiPriceAmount *>(l6);
                            p_amount.ReadData(es->I_O());
                            show_element(l6, 6, "Amount: %.3f",
                                         float(p_amount));

                          } else if (EbmlId(*l6) ==
                                     KaxTagMultiPricePriceDate::
                                     ClassInfos.GlobalId) {
                            struct tm tmutc;
                            time_t temptime;
                            char buffer[40];
                            KaxTagMultiPricePriceDate &p_pdate =
                              *static_cast<KaxTagMultiPricePriceDate *>(l6);
                            p_pdate.ReadData(es->I_O());

                            temptime = p_pdate.GetEpochDate();
                            if ((gmtime_r(&temptime, &tmutc) != NULL) &&
                                (asctime_r(&tmutc, buffer) != NULL)) {
                              buffer[strlen(buffer) - 1] = 0;
                              show_element(l6, 6, "Price date: %s UTC",
                                           buffer);
                            } else
                              show_element(l6, 6, "Price date (invalid, "
                                           "value: %d)", temptime);

                          } else if (!is_ebmlvoid(l6, 6, upper_lvl_el) &&
                                     !parse_multicomment(es, l6, 6,
                                                         upper_lvl_el, l7))
                            show_unknown_element(l6, 6);

                          if (upper_lvl_el > 0) { // we're coming from l7
                            upper_lvl_el--;
                            delete l6;
                            l6 = l7;
                            if (upper_lvl_el > 0)
                              break;

                          } else if (upper_lvl_el == 0) {
                            l6->SkipData(*es, l6->Generic().Context);
                            delete l6;
                            upper_lvl_el = 0;
                            l6 = es->FindNextElement(l5->Generic().Context,
                                                     upper_lvl_el, 0xFFFFFFFFL,
                                                     true, 1);
                          } else {
                            delete l6;
                            l6 = l7;
                          }

                        } // while (l6 != NULL)

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiDate::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi date");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagDate::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Date");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiDateType::ClassInfos.GlobalId) {
                        int type;
                        KaxTagMultiDateType &d_type =
                          *static_cast<KaxTagMultiDateType *>(l5);
                        d_type.ReadData(es->I_O());
                        type = uint32(d_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_DATE_TYPES) ?
                                     date_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiDateDateBegin::
                                 ClassInfos.GlobalId) {
                        struct tm tmutc;
                        time_t temptime;
                        char buffer[40];
                        KaxTagMultiDateDateBegin &d_begin =
                          *static_cast<KaxTagMultiDateDateBegin *>(l5);
                        d_begin.ReadData(es->I_O());

                        temptime = d_begin.GetEpochDate();
                        if ((gmtime_r(&temptime, &tmutc) != NULL) &&
                            (asctime_r(&tmutc, buffer) != NULL)) {
                          buffer[strlen(buffer) - 1] = 0;
                          show_element(l5, 5, "Begin: %s UTC", buffer);
                        } else
                          show_element(l5, 5, "Begin (invalid, value: %d)",
                                       temptime);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiDateDateEnd::ClassInfos.GlobalId) {
                        struct tm tmutc;
                        time_t temptime;
                        char buffer[40];
                        KaxTagMultiDateDateEnd &d_end =
                          *static_cast<KaxTagMultiDateDateEnd *>(l5);
                        d_end.ReadData(es->I_O());

                        temptime = d_end.GetEpochDate();
                        if ((gmtime_r(&temptime, &tmutc) != NULL) &&
                            (asctime_r(&tmutc, buffer) != NULL)) {
                          buffer[strlen(buffer) - 1] = 0;
                          show_element(l5, 5, "End: %s UTC", buffer);
                        } else
                          show_element(l5, 5, "End (invalid, value: %d)",
                                       temptime);

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiEntity::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi entity");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagEntity::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Entity");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiEntityType::ClassInfos.GlobalId) {
                        int type;
                        KaxTagMultiEntityType &e_type =
                          *static_cast<KaxTagMultiEntityType *>(l5);
                        e_type.ReadData(es->I_O());
                        type = uint32(e_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_ENTITY_TYPES) ?
                                     entity_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiEntityName::ClassInfos.GlobalId) {
                        KaxTagMultiEntityName &e_name =
                          *static_cast<KaxTagMultiEntityName *>(l5);
                        e_name.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(e_name));
                        show_element(l5, 5, "Name: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiEntityAddress::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiEntityAddress &e_address =
                          *static_cast<KaxTagMultiEntityAddress *>(l5);
                        e_address.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(e_address));
                        show_element(l5, 5, "Address: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiEntityURL::ClassInfos.GlobalId) {
                        KaxTagMultiEntityURL &e_URL =
                          *static_cast<KaxTagMultiEntityURL *>(l5);
                        e_URL.ReadData(es->I_O());
                        show_element(l5, 5, "URL: %s", string(e_URL).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiEntityEmail::ClassInfos.GlobalId) {
                        KaxTagMultiEntityEmail &e_email =
                          *static_cast<KaxTagMultiEntityEmail *>(l5);
                        e_email.ReadData(es->I_O());
                        show_element(l5, 5, "Email: %s",
                                     string(e_email).c_str());

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l5;
                    l5 = l6;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiIdentifier::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi identifier");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagIdentifier::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Identifier");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiIdentifierType::ClassInfos.GlobalId) {
                        int type;
                        KaxTagMultiIdentifierType &i_type =
                          *static_cast<KaxTagMultiIdentifierType *>(l5);
                        i_type.ReadData(es->I_O());
                        type = uint32(i_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_IDENTIFIER_TYPES) ?
                                     identifier_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiIdentifierBinary::
                                 ClassInfos.GlobalId) {
                        char buf[8];
                        const binary *b;

                        KaxTagMultiIdentifierBinary &i_binary =
                          *static_cast<KaxTagMultiIdentifierBinary *>(l5);
                        i_binary.ReadData(es->I_O());
                        strc = "Binary: length " +
                          to_string(i_binary.GetSize()) + ", data: ";
                        b = &binary(i_binary);
                        for (i = 0; i < i_binary.GetSize(); i++) {
                          sprintf(buf, "0x%02x ", (unsigned char)b[i]);
                          strc += buf;
                        }
                        show_element(l5, 5, strc.c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiIdentifierString::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiIdentifierString &i_string =
                          *static_cast<KaxTagMultiIdentifierString *>(l5);
                        i_string.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(i_string));
                        show_element(l5, 5, "String: %s", str);
                        safefree(str);

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;

                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiLegal::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi legal");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagLegal::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Legal");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiLegalType::ClassInfos.GlobalId) {
                        int type;
                        KaxTagMultiLegalType &l_type =
                          *static_cast<KaxTagMultiLegalType *>(l5);
                        l_type.ReadData(es->I_O());
                        type = uint32(l_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_LEGAL_TYPES) ?
                                     legal_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiLegalAddress::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiLegalAddress &l_address =
                          *static_cast<KaxTagMultiLegalAddress *>(l5);
                        l_address.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(l_address));
                        show_element(l5, 5, "Address: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiLegalURL::ClassInfos.GlobalId) {
                        KaxTagMultiLegalURL &l_URL =
                          *static_cast<KaxTagMultiLegalURL *>(l5);
                        l_URL.ReadData(es->I_O());
                        show_element(l5, 5, "URL: %s", string(l_URL).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiLegalContent::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiLegalContent &l_content =
                          *static_cast<KaxTagMultiLegalContent *>(l5);
                        l_content.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(l_content));
                        show_element(l5, 5, "Content: %s", str);
                        safefree(str);

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (EbmlId(*l3) ==
                         KaxTagMultiTitle::ClassInfos.GlobalId) {
                show_element(l3, 3, "Multi title");

                upper_lvl_el = 0;
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el > 0)
                    break;
                  if ((upper_lvl_el < 0) && !fits_parent(l4, l3))
                    break;

                  if (EbmlId(*l4) == KaxTagTitle::ClassInfos.GlobalId) {
                    show_element(l4, 4, "Title");

                    upper_lvl_el = 0;
                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el > 0)
                        break;
                      if ((upper_lvl_el < 0) && !fits_parent(l5, l4))
                        break;

                      if (EbmlId(*l5) ==
                          KaxTagMultiTitleType::ClassInfos.GlobalId) {
                        int type;
                        KaxTagMultiTitleType &t_type =
                          *static_cast<KaxTagMultiTitleType *>(l5);
                        t_type.ReadData(es->I_O());
                        type = uint32(t_type);
                        show_element(l5, 5, "Type: %u (%s)", type,
                                     (type >= 1) &&
                                     (type <= NUM_TITLE_TYPES) ?
                                     title_types[type - 1] : "unknown");

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleName::ClassInfos.GlobalId) {
                        KaxTagMultiTitleName &t_name =
                          *static_cast<KaxTagMultiTitleName *>(l5);
                        t_name.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(t_name));
                        show_element(l5, 5, "Name: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleSubTitle::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiTitleSubTitle &t_sub_title =
                          *static_cast<KaxTagMultiTitleSubTitle *>(l5);
                        t_sub_title.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(t_sub_title));
                        show_element(l5, 5, "Sub title: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleEdition::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiTitleEdition &t_edition =
                          *static_cast<KaxTagMultiTitleEdition *>(l5);
                        t_edition.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(t_edition));
                        show_element(l5, 5, "Edition: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleAddress::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiTitleAddress &t_address =
                          *static_cast<KaxTagMultiTitleAddress *>(l5);
                        t_address.ReadData(es->I_O());
                        str = UTFstring_to_cstr(UTFstring(t_address));
                        show_element(l5, 5, "Address: %s", str);
                        safefree(str);

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleURL::ClassInfos.GlobalId) {
                        KaxTagMultiTitleURL &t_URL =
                          *static_cast<KaxTagMultiTitleURL *>(l5);
                        t_URL.ReadData(es->I_O());
                        show_element(l5, 5, "URL: %s", string(t_URL).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleEmail::ClassInfos.GlobalId) {
                        KaxTagMultiTitleEmail &t_email =
                          *static_cast<KaxTagMultiTitleEmail *>(l5);
                        t_email.ReadData(es->I_O());
                        show_element(l5, 5, "Email: %s",
                                     string(t_email).c_str());

                      } else if (EbmlId(*l5) ==
                                 KaxTagMultiTitleLanguage::
                                 ClassInfos.GlobalId) {
                        KaxTagMultiTitleLanguage &t_language =
                          *static_cast<KaxTagMultiTitleLanguage *>(l5);
                        t_language.ReadData(es->I_O());
                        show_element(l5, 5, "Language: %s",
                                     string(t_language).c_str());

                      } else if (!is_ebmlvoid(l5, 5, upper_lvl_el) &&
                                 !parse_multicomment(es, l5, 5, upper_lvl_el,
                                                     l6))
                        show_unknown_element(l5, 5);

                      if (upper_lvl_el > 0) {    // we're coming from l6
                        upper_lvl_el--;
                        delete l5;
                        l5 = l6;
                        if (upper_lvl_el > 0)
                          break;

                      } else if (upper_lvl_el == 0) {
                        l5->SkipData(*es, l5->Generic().Context);
                        delete l5;
                        upper_lvl_el = 0;
                        l5 = es->FindNextElement(l4->Generic().Context,
                                                 upper_lvl_el, 0xFFFFFFFFL,
                                                 true, 1);
                      } else {
                        delete l5;
                        l5 = l6;
                      }

                    } // while (l5 != NULL)
                    
                  } else if (!is_ebmlvoid(l4, 4, upper_lvl_el) &&
                             !parse_multicomment(es, l4, 4, upper_lvl_el, l5))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else if (upper_lvl_el == 0) {
                    l4->SkipData(*es, l4->Generic().Context);
                    delete l4;
                    upper_lvl_el = 0;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  } else {
                    delete l4;
                    l4 = l5;
                  }

                } // while (l4 != NULL)

              } else if (!is_ebmlvoid(l3, 3, upper_lvl_el) &&
                         !parse_multicomment(es, l3, 3, upper_lvl_el, l4))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) { // we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;

              } else if (upper_lvl_el == 0) {
                l3->SkipData(*es, l3->Generic().Context);
                delete l3;
                upper_lvl_el = 0;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              } else {
                delete l3;
                l3 = l4;
              }

            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2, upper_lvl_el))
            show_unknown_element(l2, 2);

          if (upper_lvl_el > 0) {    // we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else if (upper_lvl_el == 0) {
            l2->SkipData(*es,
                         l2->Generic().Context);
            delete l2;
            upper_lvl_el = 0;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          } else {
            delete l2;
            l2 = l3;
          }

        } // while (l2 != NULL)

      } else if (!is_ebmlvoid(l1, 1, upper_lvl_el))
        show_unknown_element(l1, 1);

      if (upper_lvl_el > 0) {    // we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;

      } else if (upper_lvl_el == 0) {
        l1->SkipData(*es, l1->Generic().Context);
        delete l1;
        upper_lvl_el = 0;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);

      } else {
        delete l1;
        l1 = l2;
      }

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    return true;
  } catch (std::exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;

    return false;
  }
}

int console_main(int argc, char **argv) {
  char *file_name;

#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  nice(2);
#endif

  parse_args(argc, argv, file_name, use_gui);
  if (file_name == NULL) {
    usage();
    exit(1);
  }
  if (process_file(file_name))
    return 0;
  else
    return 1;
}

void setup() {
#if defined(SYS_UNIX)
  if (setlocale(LC_CTYPE, "") == NULL) {
    mxprint(stderr, "Error: Could not set the locale properly. Check the "
            "LANG, LC_ALL and LC_CTYPE environment variables.\n");
    exit(1);
  }
#endif

  cc_local_utf8 = utf8_init(NULL);
}

void cleanup() {
  utf8_done();
}

#if !defined HAVE_WXWINDOWS
int main(int argc, char **argv) {
  char *initial_file;
  int res;

  setup();
  parse_args(argc, argv, initial_file, use_gui);

  res = console_main(argc, argv);
  cleanup();

  return res;
}

#elif defined(SYS_UNIX)

int main(int argc, char **argv) {
  char *initial_file;
  int res;

  setup();
  parse_args(argc, argv, initial_file, use_gui);

  if (use_gui) {
    wxEntry(argc, argv);
    return 0;
  } else
    res = console_main(argc, argv);

  cleanup();

  return res;
}
#endif // HAVE_WXWINDOWS
