/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts timecodes from Matroska files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <cassert>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlVoid.h>

#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>

#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"

#include "xtr_base.h"

using namespace libmatroska;
using namespace std;

struct timecode_extractor_t {
  int64_t m_tid;
  mm_io_c *m_file;
  vector<int64_t> m_timecodes;
  int64_t m_default_duration;

  timecode_extractor_t(int64_t tid, mm_io_c *file, int64_t default_duration):
    m_tid(tid), m_file(file), m_default_duration(default_duration) {}
};

static vector<timecode_extractor_t> timecode_extractors;

// ------------------------------------------------------------------------

static void
close_timecode_files() {
  vector<timecode_extractor_t>::iterator extractor;

  mxforeach(extractor, timecode_extractors) {
    vector<int64_t> &timecodes = extractor->m_timecodes;
    vector<int64_t>::const_iterator timecode;

    sort(timecodes.begin(), timecodes.end());
    mxforeach(timecode, timecodes)
      extractor->m_file->printf("%f\n", ((double)(*timecode)) / 1000000.0);
    delete extractor->m_file;
  }
  timecode_extractors.clear();
}

static void
create_timecode_files(KaxTracks &kax_tracks,
                      vector<track_spec_t> &tracks,
                      int version) {
  vector<track_spec_t>::iterator tspec;
  int i;
  int64_t default_duration;

  mxforeach(tspec, tracks) {
    default_duration = 0;

    for (i = 0; kax_tracks.ListSize() > i; ++i) {
      if (!is_id(kax_tracks[i], KaxTrackEntry))
        continue;

      KaxTrackEntry &track = *static_cast<KaxTrackEntry *>(kax_tracks[i]);

      if (kt_get_number(track) != tspec->tid)
        continue;

      default_duration = kt_get_default_duration(track);
      if (0 > default_duration)
        default_duration = 0;
    }

    try {
      mm_io_c *file = new mm_file_io_c(tspec->out_name, MODE_CREATE);
      timecode_extractors.push_back(timecode_extractor_t(tspec->tid, file,
                                                         default_duration));
      file->printf("# timecode format v%d\n", version);
    } catch(...) {
      close_timecode_files();
      mxerror("Could not open the timecode file '%s' for writing (%s).\n",
              tspec->out_name, strerror(errno));
    }
  }
}

static void
handle_blockgroup(KaxBlockGroup &blockgroup,
                  KaxCluster &cluster,
                  int64_t tc_scale) {
  KaxBlock *block;
  KaxBlockDuration *kduration;
  int64_t duration;
  vector<timecode_extractor_t>::iterator extractor;
  int i;

  // Only continue if this block group actually contains a block.
  block = FINDFIRST(&blockgroup, KaxBlock);
  if (NULL == block)
    return;
  block->SetParent(cluster);

  // Do we need this block group?
  mxforeach(extractor, timecode_extractors)
    if (block->TrackNum() == extractor->m_tid)
      break;
  if (timecode_extractors.end() == extractor)
    return;

  // Next find the block duration if there is one.
  kduration = FINDFIRST(&blockgroup, KaxBlockDuration);
  if (NULL == kduration)
    duration = extractor->m_default_duration * block->NumberFrames();
  else
    duration = uint64(*kduration) * tc_scale;

  // Pass the block to the extractor.
  for (i = 0; block->NumberFrames() > i; ++i)
    extractor->m_timecodes.push_back((int64_t)(block->GlobalTimecode() + i *
                                               (double)duration /
                                               block->NumberFrames()));
}

static void
handle_simpleblock(KaxSimpleBlock &simpleblock,
                   KaxCluster &cluster) {
  if (0 == simpleblock.NumberFrames())
    return;

  simpleblock.SetParent(cluster);

  vector<timecode_extractor_t>::iterator extractor;
  // Do we need this simple block?
  mxforeach(extractor, timecode_extractors)
    if (simpleblock.TrackNum() == extractor->m_tid)
      break;
  if (timecode_extractors.end() == extractor)
    return;

  // Pass the block to the extractor.
  int i;
  for (i = 0; simpleblock.NumberFrames() > i; ++i)
    extractor->m_timecodes.push_back((int64_t)(simpleblock.GlobalTimecode() + i * (double)extractor->m_default_duration));
}

void
extract_timecodes(const string &file_name,
                  vector<track_spec_t> &tspecs,
                  int version) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size;
  mm_io_c *in;
  bool tracks_found;

  // open input file
  try {
    in = new mm_file_io_c(file_name);
  } catch (...) {
    show_error(_("The file '%s' could not be opened for reading (%s).\n"),
               file_name.c_str(), strerror(errno));
    return;
  }

  in->setFilePointer(0, seek_end);
  file_size = in->getFilePointer();
  in->setFilePointer(0, seek_beginning);

  try {
    es = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error(_("Error: No EBML head found."));
      delete es;

      return;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error(_("No segment/level 0 element found."));
        return;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, _("Segment"));
        break;
      }

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    tracks_found = false;

    upper_lvl_el = 0;
    // We've got our segment, so let's find the tracks
    l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL,
                             true, 1);
    while ((l1 != NULL) && (upper_lvl_el <= 0)) {
      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        show_element(l1, 1, _("Segment information"));

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            show_element(l2, 2, _("Timecode scale: " LLU), tc_scale);
          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        }

      } else if ((EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) &&
                 !tracks_found) {

        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        show_element(l1, 1, _("Segment tracks"));

        tracks_found = true;
        l1->Read(*es, KaxTracks::ClassInfos.Context, upper_lvl_el, l2, true);
        create_timecode_files(*dynamic_cast<KaxTracks *>(l1), tspecs, version);

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        show_element(l1, 1, _("Cluster"));
        cluster = (KaxCluster *)l1;

        if (verbose == 0)
          mxinfo("%s: %d%%\r", _("progress"),
                 (int)(in->getFilePointer() * 100 / file_size));

        upper_lvl_el = 0;
        l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                 0xFFFFFFFFL, true, 1);
        while ((l2 != NULL) && (upper_lvl_el <= 0)) {

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            show_element(l2, 2, _("Cluster timecode: %.3fs"),
                         (float)cluster_tc * (float)tc_scale / 1000000000.0);
            cluster->InitTimecode(cluster_tc, tc_scale);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            show_element(l2, 2, _("Block group"));

            l2->Read(*es, KaxBlockGroup::ClassInfos.Context, upper_lvl_el, l3,
                     true);
            handle_blockgroup(*static_cast<KaxBlockGroup *>(l2), *cluster,
                              tc_scale);

          } else if (EbmlId(*l2) == KaxSimpleBlock::ClassInfos.GlobalId) {
            show_element(l2, 2, _("Simple block"));

            l2->Read(*es, KaxSimpleBlock::ClassInfos.Context, upper_lvl_el, l3, true);
            handle_simpleblock(*static_cast<KaxSimpleBlock *>(l2), *cluster);

          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (upper_lvl_el > 0) {
            upper_lvl_el--;
            if (upper_lvl_el > 0)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (upper_lvl_el < 0) {
            upper_lvl_el++;
            if (upper_lvl_el < 0)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el,
                                   0xFFFFFFFFL, true);

        } // while (l2 != NULL)

      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (upper_lvl_el > 0) {
        upper_lvl_el--;
        if (upper_lvl_el > 0)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (upper_lvl_el < 0) {
        upper_lvl_el++;
        if (upper_lvl_el < 0)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el,
                               0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    close_timecode_files();

    if (0 == verbose)
      mxinfo("progress: 100%%\n");
  } catch (...) {
    show_error("Caught exception");
    delete in;

    close_timecode_files();
  }
}
