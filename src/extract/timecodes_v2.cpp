/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts timecodes from Matroska files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <cassert>
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

#include "common/common.h"
#include "common/ebml.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "extract/mkvextract.h"
#include "extract/xtr_base.h"

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

static std::string
format_timecode_for_timecode_file(int64_t timecode) {
  std::string output      = (boost::format("%1%") % (int64_t)(timecode / 1000000)).str();
  int64_t fractional_part = timecode % 1000000;

  if (0 != fractional_part) {
    output                       += (boost::format(".%06d") % fractional_part).str();
    std::string::iterator zeroes  = output.end() - 1;

    while (*zeroes == '0')
      --zeroes;

    output.erase(zeroes + 1, output.end());
  }

  return output;
}

static void
close_timecode_files() {
  vector<timecode_extractor_t>::iterator extractor;

  mxforeach(extractor, timecode_extractors) {
    vector<int64_t> &timecodes = extractor->m_timecodes;
    vector<int64_t>::const_iterator timecode;

    sort(timecodes.begin(), timecodes.end());
    mxforeach(timecode, timecodes)
      extractor->m_file->puts(boost::format("%1%\n") % format_timecode_for_timecode_file(*timecode));
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
      timecode_extractors.push_back(timecode_extractor_t(tspec->tid, file, default_duration));
      file->puts(boost::format("# timecode format v%1%\n") % version);

    } catch(...) {
      close_timecode_files();
      mxerror(boost::format(Y("Could not open the timecode file '%1%' for writing (%2%).\n")) % tspec->out_name % strerror(errno));
    }
  }
}

static void
handle_blockgroup(KaxBlockGroup &blockgroup,
                  KaxCluster &cluster,
                  int64_t tc_scale) {
  // Only continue if this block group actually contains a block.
  KaxBlock *block = FINDFIRST(&blockgroup, KaxBlock);
  if (NULL == block)
    return;
  block->SetParent(cluster);

  // Do we need this block group?
  vector<timecode_extractor_t>::iterator extractor;
  mxforeach(extractor, timecode_extractors)
    if (block->TrackNum() == extractor->m_tid)
      break;
  if (timecode_extractors.end() == extractor)
    return;

  // Next find the block duration if there is one.
  KaxBlockDuration *kduration = FINDFIRST(&blockgroup, KaxBlockDuration);
  int64_t duration            = NULL == kduration ? extractor->m_default_duration * block->NumberFrames() : uint64(*kduration) * tc_scale;

  // Pass the block to the extractor.
  int i;
  for (i = 0; block->NumberFrames() > i; ++i)
    extractor->m_timecodes.push_back((int64_t)(block->GlobalTimecode() + i * (double)duration / block->NumberFrames()));
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
  // open input file
  mm_io_c *in;
  try {
    in = new mm_file_io_c(file_name);
  } catch (...) {
    show_error(boost::format(Y("The file '%1%' could not be opened for reading (%2%).\n")) % file_name % strerror(errno));
    return;
  }

  try {
    int64_t file_size = in->get_size();
    EbmlStream *es    = new EbmlStream(*in);

    // Find the EbmlHead element. Must be the first one.
    EbmlElement *l0 = es->FindNextID(EbmlHead::ClassInfos, 0xFFFFFFFFL);
    if (l0 == NULL) {
      show_error(Y("Error: No EBML head found."));
      delete es;

      return;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (NULL == l0) {
        show_error(Y("No segment/level 0 element found."));
        return;
      }
      if (EbmlId(*l0) == KaxSegment::ClassInfos.GlobalId) {
        show_element(l0, 0, Y("Segment"));
        break;
      }

      l0->SkipData(*es, l0->Generic().Context);
      delete l0;
    }

    bool tracks_found = false;
    int upper_lvl_el  = 0;
    uint64_t tc_scale = TIMECODE_SCALE;

    // We've got our segment, so let's find the tracks
    EbmlElement *l1   = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);
    EbmlElement *l2   = NULL;
    EbmlElement *l3   = NULL;

    while ((NULL != l1) && (0 >= upper_lvl_el)) {
      if (EbmlId(*l1) == KaxInfo::ClassInfos.GlobalId) {
        // General info about this Matroska file
        show_element(l1, 1, Y("Segment information"));

        upper_lvl_el = 0;
        l2           = es->FindNextElement(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);
        while ((NULL != l2) && (0 >= upper_lvl_el)) {
          if (EbmlId(*l2) == KaxTimecodeScale::ClassInfos.GlobalId) {
            KaxTimecodeScale &ktc_scale = *static_cast<KaxTimecodeScale *>(l2);
            ktc_scale.ReadData(es->I_O());
            tc_scale = uint64(ktc_scale);
            show_element(l2, 2, boost::format(Y("Timecode scale: %1%")) % tc_scale);
          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (0 > upper_lvl_el) {
            upper_lvl_el++;
            if (0 > upper_lvl_el)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);
        }

      } else if ((EbmlId(*l1) == KaxTracks::ClassInfos.GlobalId) && !tracks_found) {

        // Yep, we've found our KaxTracks element. Now find all tracks
        // contained in this segment.
        show_element(l1, 1, Y("Segment tracks"));

        tracks_found = true;
        l1->Read(*es, KaxTracks::ClassInfos.Context, upper_lvl_el, l2, true);
        create_timecode_files(*dynamic_cast<KaxTracks *>(l1), tspecs, version);

      } else if (EbmlId(*l1) == KaxCluster::ClassInfos.GlobalId) {
        show_element(l1, 1, Y("Cluster"));
        KaxCluster *cluster = (KaxCluster *)l1;
        uint64_t cluster_tc = 0;

        if (0 == verbose)
          mxinfo(boost::format(Y("Progress: %1%%%%2%")) % (int)(in->getFilePointer() * 100 / file_size) % "\r");

        upper_lvl_el = 0;
        l2           = es->FindNextElement(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true, 1);
        while ((NULL != l2) && (0 >= upper_lvl_el)) {

          if (EbmlId(*l2) == KaxClusterTimecode::ClassInfos.GlobalId) {
            KaxClusterTimecode &ctc = *static_cast<KaxClusterTimecode *>(l2);
            ctc.ReadData(es->I_O());
            cluster_tc = uint64(ctc);
            show_element(l2, 2, boost::format(Y("Cluster timecode: %|1$.3f|s")) % ((float)cluster_tc * (float)tc_scale / 1000000000.0));
            cluster->InitTimecode(cluster_tc, tc_scale);

          } else if (EbmlId(*l2) == KaxBlockGroup::ClassInfos.GlobalId) {
            show_element(l2, 2, Y("Block group"));

            l2->Read(*es, KaxBlockGroup::ClassInfos.Context, upper_lvl_el, l3, true);
            handle_blockgroup(*static_cast<KaxBlockGroup *>(l2), *cluster, tc_scale);

          } else if (EbmlId(*l2) == KaxSimpleBlock::ClassInfos.GlobalId) {
            show_element(l2, 2, Y("Simple block"));

            l2->Read(*es, KaxSimpleBlock::ClassInfos.Context, upper_lvl_el, l3, true);
            handle_simpleblock(*static_cast<KaxSimpleBlock *>(l2), *cluster);

          } else
            l2->SkipData(*es, l2->Generic().Context);

          if (!in_parent(l1)) {
            delete l2;
            break;
          }

          if (0 < upper_lvl_el) {
            upper_lvl_el--;
            if (0 < upper_lvl_el)
              break;
            delete l2;
            l2 = l3;
            continue;

          } else if (0 > upper_lvl_el) {
            upper_lvl_el++;
            if (0 > upper_lvl_el)
              break;

          }

          l2->SkipData(*es, l2->Generic().Context);
          delete l2;
          l2 = es->FindNextElement(l1->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

        } // while (l2 != NULL)

      } else
        l1->SkipData(*es, l1->Generic().Context);

      if (!in_parent(l0)) {
        delete l1;
        break;
      }

      if (0 < upper_lvl_el) {
        upper_lvl_el--;
        if (0 < upper_lvl_el)
          break;
        delete l1;
        l1 = l2;
        continue;

      } else if (0 > upper_lvl_el) {
        upper_lvl_el++;
        if (0 > upper_lvl_el)
          break;

      }

      l1->SkipData(*es, l1->Generic().Context);
      delete l1;
      l1 = es->FindNextElement(l0->Generic().Context, upper_lvl_el, 0xFFFFFFFFL, true);

    } // while (l1 != NULL)

    delete l0;
    delete es;
    delete in;

    close_timecode_files();

    if (0 == verbose)
      mxinfo(Y("Progress: 100%\n"));

  } catch (...) {
    show_error(Y("Caught exception"));
    delete in;

    close_timecode_files();
  }
}
