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
    \version \$Id: mkvinfo.cpp,v 1.51 2003/05/29 18:35:19 mosu Exp $
    \brief retrieves and displays information about a Matroska file
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//SLM
#ifdef WIN32
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>

#ifdef LIBEBML_GCC2
#include <typeinfo>
#endif

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

#include "mkvinfo.h"
#include "common.h"
#include "matroska.h"
#include "mm_io.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

typedef struct {
  unsigned int tnum, tuid;
  char type;
  int64_t size;
} mkv_track_t;

mkv_track_t **tracks = NULL;
int num_tracks = 0;
bool use_gui = false;

void add_track(mkv_track_t *s) {
  tracks = (mkv_track_t **)saferealloc(tracks, sizeof(mkv_track_t *) *
                                   (num_tracks + 1));
  tracks[num_tracks] = s;
  num_tracks++;
}

mkv_track_t *find_track(int tnum) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tnum == tnum)
      return tracks[i];

  return NULL;
}

mkv_track_t *find_track_by_uid(int tuid) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tuid == tuid)
      return tracks[i];

  return NULL;
}

void usage() {
  fprintf(stdout,
    "Usage: mkvinfo [options] inname\n\n"
    " options:\n"
#ifdef HAVE_WXWINDOWS
    "  -g, --gui      Start the GUI. All other options are ignored.\n"
#endif
    "  inname         Use 'inname' as the source.\n"
    "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
    "                 description of what mkvinfo outputs.\n"
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
    fprintf(stdout, "(%s) %s\n", NAME, args_buffer);
}

void show_element(EbmlElement *l, int level, const char *fmt, ...) {
  va_list ap;
  char level_buffer[10];

  if (level > 9)
    die("level > 9: %d", level);

  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, fmt, ap);
  va_end(ap);

  if (!use_gui) {
    memset(&level_buffer[1], ' ', 9);
    level_buffer[0] = '|';
    level_buffer[level] = 0;
    fprintf(stdout, "(%s) %s+ %s", NAME, level_buffer, args_buffer);
    if ((verbose > 1) && (l != NULL))
      fprintf(stdout, " at %llu", l->GetElementPosition());
    fprintf(stdout, "\n");
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
  show_element(e, l, "Unknown element: %s", typeid(*e).name())

void parse_args(int argc, char **argv, char *&file_name, bool &use_gui) {
  int i;

  verbose = 0;
  file_name = NULL;

  use_gui = false;

  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--gui")) {
#ifndef HAVE_WXWINDOWS
      fprintf(stderr, "Error: mkvinfo was compiled without GUI support.\n");
      exit(1);
#else // HAVE_WXWINDOWS
      use_gui = true;
#endif // HAVE_WXWINDOWS
    } else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, "mkvinfo v" VERSION "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-v") || ! strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
             !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    } else if (file_name != NULL) {
      fprintf(stderr, "Error: Only one input file is allowed.\n");
      exit(1);
    } else
      file_name = argv[i];
}

int is_ebmlvoid(EbmlElement *l, int level) {
  if (EbmlId(*l) == EbmlVoid::ClassInfos.GlobalId) {
    show_element(l, level, "EbmlVoid");

    return 1;
  }

  return 0;
}

bool process_file(const char *file_name) {
  int upper_lvl_el, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlElement *l5 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size;
  char mkv_track_type;
  bool ms_compat;
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

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration &duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());
            show_element(l2, 2, "Duration: %.3fs",
                         float(duration) * tc_scale / 1000000000.0);

          } else if (EbmlId(*l2) == KaxMuxingApp::ClassInfos.GlobalId) {
            KaxMuxingApp &muxingapp = *static_cast<KaxMuxingApp *>(l2);
            muxingapp.ReadData(es->I_O());
#ifdef NO_WSTRING
            show_element(l2, 2, "Muxing application: %s",
                         UTFstring(muxingapp).c_str());
#else
            show_element(l2, 2, "Muxing application: %ls",
                         UTFstring(muxingapp).c_str());
#endif

          } else if (EbmlId(*l2) == KaxWritingApp::ClassInfos.GlobalId) {
            KaxWritingApp &writingapp = *static_cast<KaxWritingApp *>(l2);
            writingapp.ReadData(es->I_O());
#ifdef NO_WSTRING
            show_element(l2, 2, "Writing application: %s",
                         UTFstring(writingapp).c_str());
#else
            show_element(l2, 2, "Writing application: %ls",
                         UTFstring(writingapp).c_str());
#endif

          } else if (!is_ebmlvoid(l2, 2))
            show_unknown_element(l2, 2);

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

            mkv_track_type = '?';
            ms_compat = false;

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
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

                  } else if (!is_ebmlvoid(l4, 4))
                    show_unknown_element(l4, 4);

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

                  } else if (!is_ebmlvoid(l4, 4))
                    show_unknown_element(l4, 4);

                  l4->SkipData(*es, l4->Generic().Context);
                  delete l4;
                  l4 = es->FindNextElement(l3->Generic().Context,
                                           upper_lvl_el, 0xFFFFFFFFL, true, 1);
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                show_element(l3, 3, "Codec ID: %s", &binary(codec_id));
                if ((!strcmp((char *)&binary(codec_id), MKV_V_MSCOMP) &&
                     (mkv_track_type == 'v')) ||
                    (!strcmp((char *)&binary(codec_id), MKV_A_ACM) &&
                     (mkv_track_type == 'a')))
                  ms_compat = true;

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                char pbuffer[100];
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                if (ms_compat && (mkv_track_type == 'v') &&
                    (c_priv.GetSize() >= sizeof(BITMAPINFOHEADER))) {
                  BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)&binary(c_priv);
                  unsigned char *fcc = (unsigned char *)&bih->bi_compression;
                  sprintf(pbuffer, " (FourCC: %c%c%c%c, 0x%08x)",
                          fcc[0], fcc[1], fcc[2], fcc[3],
                          get_uint32(&bih->bi_compression));
                } else if (ms_compat && (mkv_track_type == 'a') &&
                           (c_priv.GetSize() >= sizeof(WAVEFORMATEX))) {
                  WAVEFORMATEX *wfe = (WAVEFORMATEX *)&binary(c_priv);
                  sprintf(pbuffer, " (format tag: 0x%04x)",
                          get_uint16(&wfe->w_format_tag));
                } else
                  pbuffer[0] = 0;
                show_element(l3, 3, "CodecPrivate, length %llu%s",
                             c_priv.GetSize(), pbuffer);

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

              } else if (!is_ebmlvoid(l3, 3))
                show_unknown_element(l3, 3);

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

          } else if (!is_ebmlvoid(l2, 2))
            show_unknown_element(l2, 2);

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

      } else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        show_element(l1, 1, "Seek head");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxSeek::ClassInfos.GlobalId) {
            show_element(l2, 2, "Seek entry");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
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
                             (id == KaxAttachements::ClassInfos.GlobalId) ?
                             "KaxAttachements" :
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

              } else if (!is_ebmlvoid(l3, 3))
                show_unknown_element(l3, 3);

              l3->SkipData(*es,
                           l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)


          } else if (!is_ebmlvoid(l2, 2))
            show_unknown_element(l2, 2);

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
        frame->show_progress(100 * cluster->GetElementPosition() /
                             file_size, "Parsing file");
#endif // HAVE_WXWINDOWS

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
            cluster->InitTimecode(cluster_tc);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            show_element(l2, 2, "Block group");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, false, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                KaxBlock &block = *static_cast<KaxBlock *>(l3);
                block.ReadData(es->I_O());
                block.SetParent(*cluster);
                show_element(l3, 3, "Block (track number %u, %d frame(s), "
                             "timecode %.3fs)", block.TrackNum(),
                             block.NumberFrames(),
                             (float)block.GlobalTimecode() / 1000000000.0);
                for (i = 0; i < (int)block.NumberFrames(); i++) {
                  DataBuffer &data = block.GetBuffer(i);
                  show_element(NULL, 4, "Frame with size %u", data.Size());
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

              } else if (!is_ebmlvoid(l3, 3))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) {    // we're coming from l4
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

          } else if (!is_ebmlvoid(l2, 2))
            show_unknown_element(l2, 2);

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

      } else if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) {
        show_element(l1, 1, "Cues");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxCuePoint::ClassInfos.GlobalId) {
            show_element(l2, 2, "Cue point");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              if (EbmlId(*l3) == KaxCueTime::ClassInfos.GlobalId) {
                KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
                cue_time.ReadData(es->I_O());
                show_element(l3, 3, "Cue time: %.3fs", tc_scale * 
                             ((float)uint64(cue_time)) / 1000000000.0);

              } else if (EbmlId(*l3) ==
                         KaxCueTrackPositions::ClassInfos.GlobalId) {
                show_element(l3, 3, "Cue track positions");

                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
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

                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el != 0)
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

                      } else if (!is_ebmlvoid(l5, 5))
                        show_unknown_element(l5, 5);

                      l5->SkipData(*es,
                                   l5->Generic().Context);
                      delete l5;
                      l5 = es->FindNextElement(l4->Generic().Context,
                                               upper_lvl_el, 0xFFFFFFFFL,
                                               true, 1);
                    } // while (l5 != NULL)

                  } else if (!is_ebmlvoid(l4, 4))
                    show_unknown_element(l4, 4);

                  if (upper_lvl_el > 0) {    // we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else {
                    l4->SkipData(*es,
                                 l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (!is_ebmlvoid(l3, 3))
                show_unknown_element(l3, 3);

              if (upper_lvl_el > 0) {    // we're coming from l4
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

          } else if (!is_ebmlvoid(l2, 2))
            show_unknown_element(l2, 2);

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
      } else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId) {
        show_element(l1, 1, "Tags (skipping all subelements!)");

      } else if (!is_ebmlvoid(l1, 1))
        show_unknown_element(l1, 1);

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

    return true;
  } catch (std::exception &ex) {
    show_error("Caught exception: %s", ex.what());
    delete in;

    return false;
  }
}

int console_main(int argc, char **argv) {
  char *file_name;

  nice(2);

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

#ifndef HAVE_WXWINDOWS
int main(int argc, char **argv) {
  return console_main(argc, argv);
}
#endif // HAVE_WXWINDOWS
