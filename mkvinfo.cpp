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
    \version \$Id: mkvinfo.cpp,v 1.5 2003/04/09 13:30:23 mosu Exp $
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

#ifdef GCC2
#include <typeinfo>
#endif

#include "StdIOCallback.h"

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "EbmlStream.h"
#include "FileKax.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"
#include "KaxSegment.h"
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxBlock.h"
#include "KaxBlockAdditional.h"
#include "KaxCues.h"

#include "common.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define NAME "MKVInfo"

using namespace LIBMATROSKA_NAMESPACE;

typedef struct track_t {
  int tnum;
  char type;
  u_int64_t size;
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

void process_file() {
//   track_t *t;
  int upper_lvl_el, exit_loop, i;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL, *l4 = NULL;
  EbmlStream *es;
  u_int64_t last_pos;
  u_int32_t cluster_tc;

  try {
    last_pos = 0;
    es = new EbmlStream(static_cast<StdIOCallback &>(*in));
    if (es == NULL)
      die("new EbmlStream");
    
    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL, false);
    if (l0 == NULL) {
      fprintf(stdout, "(%s) no head found\n", NAME);
      exit(0);
    }
    // Don't verify its data for now.
    l0->SkipData(static_cast<EbmlStream &>(*es), l0->Generic().Context);
    delete l0;
    fprintf(stdout, "(%s) + EBML head\n", NAME);
    
    // Next element must be a segment
    last_pos = in->getFilePointer();
    l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFL, false);
    if (l0 == NULL) {
      fprintf(stdout, "(%s) No segment/level 0 element found.\n", NAME);
      exit(0);
    }
    if (!(EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId)) {
      fprintf(stdout, "(%s) Next level 0 element is not a segment.\n", NAME);
      exit(0);
    }
    fprintf(stdout, "(%s) + segment", NAME);
    if (verbose > 1)
      fprintf(stdout, " at %llu", last_pos);
    fprintf(stdout, "\n");
    
    upper_lvl_el = 0;
    exit_loop = 0;
    // We've got our segment, so let's find the tracks
    last_pos = in->getFilePointer();
    l1 = es->FindNextID(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                        true);
    while (l1 != NULL) {
      if (upper_lvl_el != 0)
        break;

      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        fprintf(stdout, "(%s) |+ segment information", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", last_pos);
        fprintf(stdout, "\n");

        last_pos = in->getFilePointer();
        l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                            true);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &tc_scale = *static_cast<KaxTimecodeScale *>(l2);
            fprintf(stdout, "(%s) | + timecode scale: %llu", NAME,
                    uint64(tc_scale));
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
            fprintf(stdout, "\n");
          } else {
            fprintf(stdout, "(%s) | + unknown element, level2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
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
            last_pos = in->getFilePointer();
            l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el,
                                0xFFFFFFFFL, true);
          }
        }

      } else if (EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) {
        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        fprintf(stdout, "(%s) |+ segment tracks", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", last_pos);
        fprintf(stdout, "\n");
        
        last_pos = in->getFilePointer();
        l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                            true);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;
          
          if (EbmlId(*l2) == KaxTrackEntry::ClassInfos.GlobalId) {
            // We actually found a track entry :) We're happy now.
            fprintf(stdout, "(%s) | + a track", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
            fprintf(stdout, "\n");

            last_pos = in->getFilePointer();
            l3 = es->FindNextID(l2->Generic().Context, upper_lvl_el,
                                0xFFFFFFFFL, true);
            while (l3 != NULL) {
              if (upper_lvl_el != 0)
                break;
              
              // Now evaluate the data belonging to this track
              if (EbmlId(*l3) == KaxTrackNumber::ClassInfos.GlobalId) {
                KaxTrackNumber &tnum = *static_cast<KaxTrackNumber *>(l3);
                tnum.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Track number %d", NAME,
                        uint8(tnum));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");
                if (find_track(uint8(tnum)) != NULL)
                  fprintf(stdout, "(%s) |  + Warning: There's more than one "
                          "track with the number %u.\n", NAME,
                          uint8(tnum));

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
                  default:
                    fprintf(stdout, "unknown");
                    break;
                }
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) == KaxTrackAudio::ClassInfos.GlobalId) {
                fprintf(stdout, "(%s) |  + Audio track", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");
                last_pos = in->getFilePointer();
                l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                    0xFFFFFFFFL, true);
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
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxAudioChannels::ClassInfos.GlobalId) {
                    KaxAudioChannels &channels =
                      *static_cast<KaxAudioChannels*>(l4);
                    channels.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Channels: %u", NAME,
                            uint8(channels));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxAudioBitDepth::ClassInfos.GlobalId) {
                    KaxAudioBitDepth &bps =
                      *static_cast<KaxAudioBitDepth*>(l4);
                    bps.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Bit depth: %u", NAME,
                            uint8(bps));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else {
                    fprintf(stdout, "(%s) |   + unknown element, level 4: %s",
                            NAME, typeid(*l4).name());
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");
                  }

                  if (upper_lvl_el > 0) {
									  assert(1 == 0);	// this should never happen
                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    last_pos = in->getFilePointer();
                    l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                        0xFFFFFFFFL, true);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxTrackVideo::ClassInfos.GlobalId) {
                fprintf(stdout, "(%s) |  + Video track", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");
                last_pos = in->getFilePointer();
                l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                    0xFFFFFFFFL, true);
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
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxVideoPixelHeight::ClassInfos.GlobalId) {
                    KaxVideoPixelHeight &height =
                      *static_cast<KaxVideoPixelHeight *>(l4);
                    height.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Pixel height: %u", NAME,
                            uint16(height));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else if (EbmlId(*l4) ==
                             KaxVideoFrameRate::ClassInfos.GlobalId) {
                    KaxVideoFrameRate &framerate =
                      *static_cast<KaxVideoFrameRate *>(l4);
                    framerate.ReadData(es->I_O());
                    fprintf(stdout, "(%s) |   + Frame rate: %f", NAME,
                            float(framerate));
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");

                  } else {
                    fprintf(stdout, "(%s) |   + unknown element, level 4: %s",
                            NAME, typeid(*l4).name());
                    if (verbose > 1)
                      fprintf(stdout, " at %llu", last_pos);
                    fprintf(stdout, "\n");
                  }

                  if (upper_lvl_el > 0) {
									  assert(1 == 0);	// this should never happen
                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    last_pos = in->getFilePointer();
                    l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                        0xFFFFFFFFL, true);
                  }
                } // while (l4 != NULL)

              } else if (EbmlId(*l3) == KaxCodecID::ClassInfos.GlobalId) {
                KaxCodecID &codec_id = *static_cast<KaxCodecID*>(l3);
                codec_id.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + Codec ID: %s", NAME,
                        &binary(codec_id));
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");

              } else if (EbmlId(*l3) == KaxCodecPrivate::ClassInfos.GlobalId) {
                KaxCodecPrivate &c_priv = *static_cast<KaxCodecPrivate*>(l3);
                c_priv.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + CodecPrivate, length %llu",
                        NAME, c_priv.GetSize());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");

              } else {
                fprintf(stdout, "(%s) |  + unknown element, level 3: %s",
                        NAME, typeid(*l3).name());
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
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
                last_pos = in->getFilePointer();
                l3 = es->FindNextID(l2->Generic().Context, upper_lvl_el,
                                    0xFFFFFFFFL, true);
              }
            } // while (l3 != NULL)

          } else {
            fprintf(stdout, "(%s) | + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
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
            last_pos = in->getFilePointer();
            l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el,
                                0xFFFFFFFFL, true);
          }
        } // while (l2 != NULL)

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        fprintf(stdout, "(%s) |+ found cluster", NAME);
        if (verbose > 1)
          fprintf(stdout, " at %llu", last_pos);
        fprintf(stdout, "\n");
        if (verbose == 0)
          exit(0);

        last_pos = in->getFilePointer();
        l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                            true);
        while (l2 != NULL) {
          if (upper_lvl_el != 0)
            break;

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint32(ctc);
            fprintf(stdout, "(%s) | + cluster timecode: %u", NAME,
                    cluster_tc);
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
            fprintf(stdout, "\n");

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            fprintf(stdout, "(%s) | + block group", NAME);
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
            fprintf(stdout, "\n");

            last_pos = in->getFilePointer();
            l3 = es->FindNextID(l2->Generic().Context, upper_lvl_el,
                                0xFFFFFFFFL, false);
            while (l3 != NULL) {
              if (upper_lvl_el > 0)
                break;

              if (EbmlId(*l3) == KaxBlock::ClassInfos.GlobalId) {
                KaxBlock &block = *static_cast<KaxBlock *>(l3);
                block.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + block (track number %u, %d frame(s)"
                        ", timecode %u)", NAME, block.TrackNum(),
                        block.NumberFrames(), block.Timecod() + cluster_tc);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");
                for (i = 0; i < (int)block.NumberFrames(); i++) {
                  DataBuffer &data = block.GetBuffer(i);
                  fprintf(stdout, "(%s) |   + frame with size %u\n", NAME,
                          data.Size());
                }

              } else if (EbmlId(*l3) ==
                         KaxBlockAdditional::ClassInfos.GlobalId) {
                KaxBlockAdditional &add =
                  *static_cast<KaxBlockAdditional *>(l3);
                add.ReadData(es->I_O());
                fprintf(stdout, "(%s) |  + block additional", NAME);
                if (verbose > 1)
                  fprintf(stdout, " at %llu", last_pos);
                fprintf(stdout, "\n");

                last_pos = in->getFilePointer();
                l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                    0xFFFFFFFFL, true);
                while (l4 != NULL) {
                  if (upper_lvl_el != 0)
                    break;

                  fprintf(stdout, "(%s) |   + unknown element, level 4: %s",
                          NAME, typeid(*l4).name());
                  if (verbose > 1)
                    fprintf(stdout, " at %llu", last_pos);
                  fprintf(stdout, "\n");

                  if (upper_lvl_el > 0) {
									  assert(1 == 0);	// this should never happen
                  } else {
                    l4->SkipData(static_cast<EbmlStream &>(*es),
                                 l4->Generic().Context);
                    delete l4;
                    last_pos = in->getFilePointer();
                    l4 = es->FindNextID(l3->Generic().Context, upper_lvl_el,
                                        0xFFFFFFFFL, true);
                  }
                } // while (l4 != NULL)

              } else {
                 fprintf(stdout, "(%s) |  + unknown element, level 3: %s",
                         NAME, typeid(*l3).name());
                 if (verbose > 1)
                   fprintf(stdout, " at %llu", last_pos);
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
                last_pos = in->getFilePointer();
                l3 = es->FindNextID(l2->Generic().Context, upper_lvl_el,
                                    0xFFFFFFFFL, true);
              }
            } // while (l3 != NULL)
          } else {
            fprintf(stdout, "(%s) |  + unknown element, level 2: %s", NAME,
                    typeid(*l2).name());
            if (verbose > 1)
              fprintf(stdout, " at %llu", last_pos);
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
            last_pos = in->getFilePointer();
            l2 = es->FindNextID(l1->Generic().Context, upper_lvl_el,
                                0xFFFFFFFFL, true);
//             if (l2) printf("[mkv] [fb]? 2 : %s\n", typeid(*l2).name());
          }
        } // while (l2 != NULL)

      } else {
        fprintf(stdout, "(%s) |+ unknown element, level 1: %s", NAME,
                typeid(*l1).name());
        if (verbose > 1)
          fprintf(stdout, " at %llu", last_pos);
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
        last_pos = in->getFilePointer();
        l1 = es->FindNextID(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                            true);
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
