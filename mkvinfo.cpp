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
    \version \$Id: mkvinfo.cpp,v 1.29 2003/05/03 09:48:29 mosu Exp $
    \brief retrieves and displays information about a Matroska file
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
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

#include "StdIOCallback.h"

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
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "common.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define NAME "MKVInfo"

using namespace LIBMATROSKA_NAMESPACE;

typedef struct track_t {
  unsigned int tnum, tuid;
  char type;
  int64_t size;
} track_t;

track_t **tracks = NULL;
int num_tracks = 0;
StdIOCallback *in = NULL;

void add_track(track_t *s) {
  tracks = (track_t **)realloc(tracks, sizeof(track_t *) * (num_tracks + 1));
  if (tracks == NULL)
    die("realloc");
  tracks[num_tracks] = s;
  num_tracks++;
}

track_t *find_track(int tnum) {
  int i;

  for (i = 0; i < num_tracks; i++)
    if (tracks[i]->tnum == tnum)
      return tracks[i];

  return NULL;
}

track_t *find_track_by_uid(int tuid) {
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
    "  inname         Use 'inname' as the source.\n"
    "  -v, --verbose  Increase verbosity. See the man page for a detailed\n"
    "                 description of what mkvinfo outputs.\n"
    "  -h, --help     Show this help.\n"
    "  -V, --version  Show version information.\n");
}

static void parse_args(int argc, char **argv) {
  int i;
  char *infile = NULL;

  verbose = 0;

  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, "mkvinfo v" VERSION "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-v") || ! strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
             !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    } else if (infile != NULL) {
      fprintf(stderr, "Error: Only one input file is allowed.\n");
      exit(1);
    } else
      infile = argv[i];

  if (infile == NULL) {
    usage();
    exit(0);
  }

  /* open input file */
  try {
    in = new StdIOCallback(infile, MODE_READ);
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Couldn't open input file %s (%s).\n", infile,
            strerror(errno));
    exit(1);
  }
}

int is_ebmlvoid(EbmlElement *l, int level) {
  char buffer[10];

  if (EbmlId(*l) == EbmlVoid::ClassInfos.GlobalId) {
    memset(buffer, ' ', 10);
    buffer[0] = '|';
    buffer[level] = 0;
    fprintf(stdout, "(%s) %s+ EbmlVoid", NAME, buffer);
    if (verbose > 1)
      fprintf(stdout, " at %llu", l->GetElementPosition());
    fprintf(stdout, "\n");

    return 1;
  }

  return 0;
}

void process_file() {
  int upper_lvl_el, exit_loop, i, delete_object;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlElement *l5 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE;

  try {
    delete_object = 1;

    es = new EbmlStream(static_cast<StdIOCallback &>(*in));
    if (es == NULL)
      die("new EbmlStream");
    
    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      fprintf(stdout, "(%s) no head found\n", NAME);
      exit(0);
    }
    // Don't verify its data for now.
    l0->SkipData(static_cast<EbmlStream &>(*es), l0->Generic().Context);
    delete l0;
    fprintf(stdout, "(%s) + EBML head\n", NAME);
    
    // Next element must be a segment
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      fprintf(stdout, "(%s) No segment/level 0 element found.\n", NAME);
      exit(0);
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      fprintf(stdout, "(%s) Next level 0 element is not a segment but %s\n",
              NAME, typeid(*l0).name());
      exit(0);
    }
    fprintf(stdout, "(%s) + segment", NAME);
    if (verbose > 1)
      fprintf(stdout, " at %llu", l0->GetElementPosition());
    fprintf(stdout, "\n");
    
    upper_lvl_el = 0;
    exit_loop = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while (l1 != NULL) {
      if (upper_lvl_el != 0)
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        fprintf(stdout, "(%s) |+ segment information", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            fprintf(stdout, "(%s) | + timecode scale: %llu", NAME, tc_scale);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

          } else if (EbmlId(*l2) == KaxDuration::ClassInfos.GlobalId) {
            KaxDuration &duration = *static_cast<KaxDuration *>(l2);
            duration.ReadData(es->I_O());
            fprintf(stdout, "(%s) | + Duration: %.3fs", NAME,
                    float(duration) * tc_scale / 1000000000.0);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

          } else if (!is_ebmlvoid(l2, 2)) {
            fprintf(stdout, "(%s) | + unknown element, level2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
          }

          if (upper_lvl_el > 0) {	// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        fprintf(stdout, "(%s) |+ segment tracks", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");
        
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;
          
          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            fprintf(stdout, "(%s) | + a track", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;
              
              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Track number: %u", NAME,
                        uint32(tnum));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
                if (find_track(uint32(tnum)) != NULL)
                  fprintf(stdout, "(%s) |  + Warning: There's more than one "
                          "track with the number %u.\n", NAME, uint32(tnum));

              } else if (EbmlId(*l3) == KaxTrackUID::ClassInfos.GlobalId) {
                KaxTrackUID &tuid = *static_cast<KaxTrackUID *>(l3);
                tuid.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Track UID: %u", NAME, uint32(tuid));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
                if (find_track_by_uid(uint32(tuid)) != NULL)
                  fprintf(stdout, "(%s) |  + Warning: There's more than one "
                          "track with the UID %u.\n", NAME, uint32(tuid));

              } else if (EbmlId(*l3) == KaxTrackType::ClassInfos.GlobalId) {
                KaxTrackType &ttype = *static_cast<KaxTrackType *>(l3);
                ttype.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Track type: ", NAME);

                switch (uint8(ttype)) {
                  case track_audio:
                    fprintf(stdout, "Audio");
                    break;
                  case track_video:
                    fprintf(stdout, "Video");
                    break;
                  case track_subtitle:
                    fprintf(stdout, "Subtitles");
                    break;
                  default:
                    fprintf(stdout, "unknown");
                    break;
                }
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                fprintf(stdout, "(%s) |  + Audio track", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
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
                    fprintf(stdout, "(%s) |   + Sampling frequency: %f",
                            NAME, float(freq));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Channels: %u", NAME,
                            uint8(channels));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Bit depth: %u", NAME,
                            uint8(bps));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (!is_ebmlvoid(l4, 4)) {
                    fprintf(stdout, "(%s) |   + unknown element, level 4: %s",
                            NAME, typeid(*l4).name());
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");
                  }

                  if (upper_lvl_el > 0) {
									  assert(1 == 0);	// this should never happen
                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                fprintf(stdout, "(%s) |  + Video track", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) == KaxVideoPixelWidth::ClassInfos.GlobalId) {
                    KaxVideoPixelWidth &width =
                      *static_cast<KaxVideoPixelWidth *>(l4);
                    width.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Pixel width: %u", NAME,
                            uint16(width));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Pixel height: %u", NAME,
                            uint16(height));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Frame rate: %f", NAME,
                            float(framerate));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (!is_ebmlvoid(l4, 4)) {
                    fprintf(stdout, "(%s) |   + unknown element, level 4: %s",
                            NAME, typeid(*l4).name());
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");
                  }

                  if (upper_lvl_el > 0) {
									  assert(1 == 0);	// this should never happen
                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Codec ID: %s", NAME,
                        &binary(codec_id));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + CodecPrivate, length %llu",
                        NAME, c_priv.GetSize());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxTrackMinCache::ClassInfos.GlobalId) {
                KaxTrackMinCache &min_cache =
                  *static_cast<KaxTrackMinCache*>(l3);
                min_cache.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + MinCache: %u", NAME,
                        uint32(min_cache));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxTrackMaxCache::ClassInfos.GlobalId) {
                KaxTrackMaxCache &max_cache =
                  *static_cast<KaxTrackMaxCache*>(l3);
                max_cache.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + MaxCache: %u", NAME,
                        uint32(max_cache));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagLacing::ClassInfos.GlobalId) {
                KaxTrackFlagLacing &f_lacing =
                  *static_cast<KaxTrackFlagLacing *>(l3);
                f_lacing.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Lacing flag: %d", NAME,
                        uint32(f_lacing)); 
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxTrackFlagDefault::ClassInfos.GlobalId) {
                KaxTrackFlagDefault &f_default =
                  *static_cast<KaxTrackFlagDefault *>(l3);
                f_default.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Default flag: %d", NAME,
                        uint32(f_default)); 
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxTrackLanguage::ClassInfos.GlobalId) {
                KaxTrackLanguage &language =
                  *static_cast<KaxTrackLanguage *>(l3);
                language.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Language: %s", NAME,
                        string(language).c_str());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (!is_ebmlvoid(l3, 3)) {
                fprintf(stdout, "(%s) |  + unknown element, level 3: %s",
                        NAME, typeid(*l3).name());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
              }

              if (upper_lvl_el > 0) {	// we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;
              } else {
                l3->SkipData(static_cast<EbmlStream &>(*es),
                             l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2)) {
            fprintf(stdout, "(%s) | + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
          }

          if (upper_lvl_el > 0) {	// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;
          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxSeekHead::ClassInfos.GlobalId) {
        fprintf(stdout, "(%s) |+ found seek head", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxSeek::ClassInfos.GlobalId) {
            fprintf(stdout, "(%s) | + seek entry", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              if (EbmlId(*l3) == KaxSeekID::ClassInfos.GlobalId) {
                binary *b;
                int s;
                KaxSeekID &seek_id = static_cast<KaxSeekID &>(*l3);
                seek_id.ReadData(es->I_O());
                b = seek_id.GetBuffer();
                s = seek_id.GetSize();
                EbmlId id(b, s);
                fprintf(stdout, "(%s) |  + seek ID: ", NAME);
                for (i = 0; i < s; i++)
                  fprintf(stdout, "0x%02x ", ((unsigned char *)b)[i]);
                fprintf(stdout, "(%s)",
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
                        "unknown");
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) == KaxSeekPosition::ClassInfos.GlobalId) {
                KaxSeekPosition &seek_pos =
                  static_cast<KaxSeekPosition &>(*l3);
                seek_pos.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + seek position: %llu", NAME,
                        uint64(seek_pos));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (!is_ebmlvoid(l3, 3)) {
                fprintf(stdout, "(%s) |  + unknown element, level 3: %s", NAME,
                        typeid(*l3).name());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
              }


              l3->SkipData(static_cast<EbmlStream &>(*es),
                           l3->Generic().Context);
              delete l3;
              l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                       0xFFFFFFFFL, true, 1);
            } // while (l3 != NULL)


          } else if (!is_ebmlvoid(l2, 2)) {
            fprintf(stdout, "(%s) |  + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
          }

          if (upper_lvl_el > 0) {		// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        fprintf(stdout, "(%s) |+ found cluster", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");
        if (verbose == 0)
          exit(0);
        cluster = (KaxCluster *)l1;

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            
            fprintf(stdout, "(%s) | + cluster timecode: %.3fs", NAME,
                    (float)cluster_tc * (float)tc_scale / 1000000000.0);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
            cluster->InitTimecode(cluster_tc);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            fprintf(stdout, "(%s) | + block group", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, false, 1);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                KaxBlock &block = *static_cast<KaxBlock *>(l3);
                block.SetParent(*cluster);
                block.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + block (track number %u, %d frame(s)"
                        ", timecode %.3fs)", NAME, block.TrackNum(),
                        block.NumberFrames(),
                        (float)block.GlobalTimecode() / 1000000000.0);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
                for (i = 0; i < (int)block.NumberFrames(); i++) {
                  DataBuffer &data = block.GetBuffer(i);
                  fprintf(stdout, "(%s) |   + frame with size %u\n", NAME,
                          data.Size());
                }

              } else if (EbmlId(*l3) ==
                         KaxBlockDuration::ClassInfos.GlobalId) {
                KaxBlockDuration &duration =
                  *static_cast<KaxBlockDuration *>(l3);
                duration.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + block duration: %.3fms", NAME,
                        ((float)uint64(duration)) * tc_scale / 1000000.0);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxReferenceBlock::ClassInfos.GlobalId) {
                KaxReferenceBlock &reference =
                  *static_cast<KaxReferenceBlock *>(l3);
                reference.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + reference block: %.3fms", NAME,
                        ((float)int64(reference)) * tc_scale /
                        1000000.0);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (!is_ebmlvoid(l3, 3)) {
                 fprintf(stdout, "(%s) |  + unknown element, level 3: %s",
                         NAME, typeid(*l3).name());
                 if (verbose > 1)
                   fprintf(stdout, " at %llu", l3->GetElementPosition());
                 fprintf(stdout, "\n");
              }

              if (upper_lvl_el > 0) {		// we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;

              } else {
                l3->SkipData(static_cast<EbmlStream &>(*es),
                             l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2)) {
            fprintf(stdout, "(%s) |  + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
          }

          if (upper_lvl_el > 0) {		// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCues::ClassInfos.GlobalId) {
        fprintf(stdout, "(%s) |+ found cues", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");

        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxCuePoint::ClassInfos.GlobalId) {
            fprintf(stdout, "(%s) | + found cue point", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");

            l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;

              if (EbmlId(*l3) == KaxCueTime::ClassInfos.GlobalId) {
                KaxCueTime &cue_time = *static_cast<KaxCueTime *>(l3);
                cue_time.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + found cue time: %.3fs", NAME,
                        ((float)uint64(cue_time)) * tc_scale / 1000000000.0);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) ==
                         KaxCueTrackPositions::ClassInfos.GlobalId) {
                fprintf(stdout, "(%s) |  + found cue track positions", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");

                l4 = es->FindNextElement(l3->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  if (EbmlId(*l4) == KaxCueTrack::ClassInfos.GlobalId) {
                    KaxCueTrack &cue_track = *static_cast<KaxCueTrack *>(l4);
                    cue_track.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + found cue track: %u", NAME,
                            uint32(cue_track));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxCueClusterPosition::ClassInfos.GlobalId) {
                    KaxCueClusterPosition &cue_cp =
                      *static_cast<KaxCueClusterPosition *>(l4);
                    cue_cp.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + found cue cluster position: "
                            "%llu", NAME, uint64(cue_cp));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxCueBlockNumber::ClassInfos.GlobalId) {
                    KaxCueBlockNumber &cue_bn =
                      *static_cast<KaxCueBlockNumber *>(l4);
                    cue_bn.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + found cue block number: %llu",
                            NAME, uint64(cue_bn));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxCueCodecState::ClassInfos.GlobalId) {
                    KaxCueCodecState &cue_cs =
                      *static_cast<KaxCueCodecState *>(l4);
                    cue_cs.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + found cue codec state: %llu",
                            NAME, uint64(cue_cs));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxCueReference::ClassInfos.GlobalId) {
                    fprintf(stdout, "(%s) |  + found cue reference", NAME);
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");


                    l5 = es->FindNextElement(l4->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                    while (l5 != NULL) {
                      if (upper_lvl_el != 0)
                        break;

                      if (EbmlId(*l5) == KaxCueRefTime::ClassInfos.GlobalId) {
                        KaxCueRefTime &cue_rt =
                          *static_cast<KaxCueRefTime *>(l5);
                        fprintf(stdout, "(%s) |    + found cue ref time: "
                                "%.3fs", NAME,
                                ((float)uint64(cue_rt)) * tc_scale /
                                1000000000.0);
                        if (verbose > 1)
                          fprintf(stdout, " at %llu",
                                  l5->GetElementPosition());
                        fprintf(stdout, "\n");

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefCluster::ClassInfos.GlobalId) {
                        KaxCueRefCluster &cue_rc =
                          *static_cast<KaxCueRefCluster *>(l5);
                        cue_rc.ReadData(es->I_O());
                        fprintf(stdout, "(%s) |    + found cue ref cluster: "
                                "%llu", NAME, uint64(cue_rc));
                        if (verbose > 1)
                          fprintf(stdout, " at %llu",
                                  l5->GetElementPosition());
                        fprintf(stdout, "\n");

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefNumber::ClassInfos.GlobalId) {
                        KaxCueRefNumber &cue_rn =
                          *static_cast<KaxCueRefNumber *>(l5);
                        cue_rn.ReadData(es->I_O());
                        fprintf(stdout, "(%s) |    + found cue ref number: "
                                "%llu", NAME, uint64(cue_rn));
                        if (verbose > 1)
                          fprintf(stdout, " at %llu",
                                  l5->GetElementPosition());
                        fprintf(stdout, "\n");

                      } else if (EbmlId(*l5) ==
                                 KaxCueRefCodecState::ClassInfos.GlobalId) {
                        KaxCueRefCodecState &cue_rcs =
                          *static_cast<KaxCueRefCodecState *>(l5);
                        cue_rcs.ReadData(es->I_O());
                        fprintf(stdout, "(%s) |    + found cue ref codec state"
                                ": %llu", NAME, uint64(cue_rcs));
                        if (verbose > 1)
                          fprintf(stdout, " at %llu",
                                  l5->GetElementPosition());
                        fprintf(stdout, "\n");

                      } else if (!is_ebmlvoid(l5, 5)) {
                        fprintf(stdout, "(%s) |    + unknown element, "
                                "level 5: %s", NAME, typeid(*l5).name());
                        if (verbose > 1)
                          fprintf(stdout, " at %llu",
                                  l5->GetElementPosition());
                        fprintf(stdout, "\n");
                      }

                      l5->SkipData(static_cast<EbmlStream &>(*es),
                                   l5->Generic().Context);
                      delete l5;
                      l5 = es->FindNextElement(l4->Generic().Context,
                                               upper_lvl_el, 0xFFFFFFFFL,
                                               true, 1);
                    } // while (l5 != NULL)

                  } else if (!is_ebmlvoid(l4, 4)) {
                    fprintf(stdout, "(%s) |  + unknown element, level 4: %s",
                            NAME, typeid(*l4).name());
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", l4->GetElementPosition());
                    fprintf(stdout, "\n");
                  }

                  if (upper_lvl_el > 0) {		// we're coming from l5
                    upper_lvl_el--;
                    delete l4;
                    l4 = l5;
                    if (upper_lvl_el > 0)
                      break;

                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    l4 = es->FindNextElement(l3->Generic().Context,
                                             upper_lvl_el, 0xFFFFFFFFL, true,
                                             1);
                  }
                } // while (l4 != NULL)

              } else if (!is_ebmlvoid(l3, 3)) {
                fprintf(stdout, "(%s) |  + unknown element, level 3: %s", NAME,
                        typeid(*l3).name());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", l3->GetElementPosition());
                fprintf(stdout, "\n");
              }

              if (upper_lvl_el > 0) {		// we're coming from l4
                upper_lvl_el--;
                delete l3;
                l3 = l4;
                if (upper_lvl_el > 0)
                  break;

              } else {
                l3->SkipData(static_cast<EbmlStream &>(*es),
                             l3->Generic().Context);
                delete l3;
                l3 = es->FindNextElement(l2->Generic().Context, upper_lvl_el,
                                         0xFFFFFFFFL, true, 1);
              }
            } // while (l3 != NULL)

          } else if (!is_ebmlvoid(l2, 2)) {
            fprintf(stdout, "(%s) |  + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", l2->GetElementPosition());
            fprintf(stdout, "\n");
          }

          if (upper_lvl_el > 0) {		// we're coming from l3
            upper_lvl_el--;
            delete l2;
            l2 = l3;
            if (upper_lvl_el > 0)
              break;

          } else {
            l2->SkipData(static_cast<EbmlStream &>(*es),
                         l2->Generic().Context);
            delete l2;
            l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                     0xFFFFFFFFL, true, 1);
          }
        } // while (l2 != NULL)
      } else if (!is_ebmlvoid(l1, 1)) {
        fprintf(stdout, "(%s) |+ unknown element, level 1: %s", NAME,
                typeid(*l1).name());
        if (verbose > 1)
          fprintf(stdout, " at %llu", l1->GetElementPosition());
        fprintf(stdout, "\n");
      }

      if (exit_loop)      // we've found the first cluster, so get out
        break;

      if (upper_lvl_el > 0) {		// we're coming from l2
        upper_lvl_el--;
        delete l1;
        l1 = l2;
        if (upper_lvl_el > 0)
          break;
      } else {
        l1->SkipData(static_cast<EbmlStream &>(*es), l1->Generic().Context);
        delete l1;
        l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
      }
    } // while (l1 != NULL)
    
  } catch (std::exception &ex) {
    fprintf(stdout, "(%s) caught exception\n", NAME);
  }
}

int main(int argc, char **argv) {
  nice(2);

  parse_args(argc, argv);
  process_file();

  return 0;
}
