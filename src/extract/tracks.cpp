/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

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
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"

#include "xtr_base.h"

using namespace libmatroska;
using namespace std;

static vector<xtr_base_c *> extractors;

// ------------------------------------------------------------------------

static void
create_extractors(KaxTracks &kax_tracks) {
  int i;

  for (i = 0; i < kax_tracks.ListSize(); i++) {
    int k;
    int64_t tnum;
    string codec_id;
    xtr_base_c *extractor, *master;
    track_spec_t *tspec;

    if (!is_id(kax_tracks[i], KaxTrackEntry))
      continue;

    KaxTrackEntry &track = *static_cast<KaxTrackEntry *>(kax_tracks[i]);

    tnum = kt_get_number(track);

    // Is the track number present and valid?
    if (0 == tnum)
      continue;

    // Is there more than one track with the same track number?
    extractor = NULL;
    for (k = 0; k < extractors.size(); k++)
      if (extractors[k]->tid == tnum) {
        mxwarn("More than one track with the track number %lld found.\n",
               tnum);
        extractor = extractors[k];
        break;
      }
    if (NULL != extractor)
      continue;

    // Does the user want this track to be extracted?
    tspec = NULL;
    for (k = 0; k < tracks.size(); k++)
      if (tracks[k].tid == tnum) {
        tspec = &tracks[k];
        break;
      }
    if (NULL == tspec)
      continue;

    // Let's find the codec ID and create an extractor for it.
    codec_id = kt_get_codec_id(track);
    if (codec_id.empty())
      mxerror("The track number %lld does not have a valid CodecID.\n", tnum);

    extractor = xtr_base_c::create_extractor(codec_id, tnum, *tspec);
    if (NULL == extractor)
      mxerror("Extraction of track number %lld with the CodecID '%s' is "
              "not supported.\n", tnum, codec_id.c_str());

    // Has there another file been requested with the same name?
    master = NULL;
    for (k = 0; k < extractors.size(); k++)
      if (extractors[k]->file_name == tspec->out_name) {
        master = extractors[k];
        break;
      }

    // Let the extractor create the file.
    extractor->create_file(master, track);

    // We're done.
    extractors.push_back(extractor);

    mxinfo("Extracting track %lld with the CodecID '%s' to the file '%s'.\n",
           tnum, codec_id.c_str(), tspec->out_name);
  }

  // Signal that all headers have been taken care of.
  for (i = 0; i < extractors.size(); i++)
    extractors[i]->headers_done();
}

static void
handle_blockgroup(KaxBlockGroup &blockgroup,
                  KaxCluster &cluster,
                  int64_t tc_scale) {
  KaxBlock *block;
  KaxBlockDuration *kduration;
  KaxReferenceBlock *kreference;
  KaxBlockAdditions *kadditions;
  xtr_base_c *extractor;
  int64_t duration, fref, bref;
  int i;

  // Only continue if this block group actually contains a block.
  block = FINDFIRST(&blockgroup, KaxBlock);
  if (NULL == block)
    return;
  block->SetParent(cluster);

  // Do we need this block group?
  extractor = NULL;
  for (i = 0; i < extractors.size(); i++)
    if (block->TrackNum() == extractors[i]->tid) {
      extractor = extractors[i];
      break;
    }
  if (NULL == extractor)
    return;

  // Next find the block duration if there is one.
  kduration = FINDFIRST(&blockgroup, KaxBlockDuration);
  if (NULL == kduration)
    duration = -1;
  else
    duration = uint64(*kduration) * tc_scale;

  // Now find backward and forward references.
  bref = 0;
  fref = 0;
  for (i = 0; i < 2; i++) {
    if (0 == i)
      kreference = FINDFIRST(&blockgroup, KaxReferenceBlock);
    else
      kreference = FINDNEXT(&blockgroup, KaxReferenceBlock, kreference);

    if (NULL != kreference) {
      if (int64(*kreference) < 0)
        bref = int64(*kreference);
      else
        fref = int64(*kreference);
    }
  }

  // Any block additions present?
  kadditions = FINDFIRST(&blockgroup, KaxBlockAdditions);

  // Pass the block to the extractor.
  extractor->handle_block(*block, kadditions, block->GlobalTimecode(),
                          duration, bref, fref);
}

static void
close_extractors() {
  int i;

  for (i = 0; i < extractors.size(); i++)
    if (NULL != extractors[i]->master)
      extractors[i]->finish_file();
  for (i = 0; i < extractors.size(); i++) {
    if (NULL == extractors[i]->master)
      extractors[i]->finish_file();
    delete extractors[i];
  }
  extractors.clear();
}

static void
write_all_cuesheets(KaxChapters &chapters,
                    KaxTags &tags) {
  int i;
  mm_io_c *out;

  out = NULL;

  for (i = 0; i < tracks.size(); i++) {
    if (tracks[i].extract_cuesheet) {
      string file_name, cue_file_name;
      int pos, pos2, pos3;

      file_name = tracks[i].out_name;
      pos = file_name.rfind('/');
      pos2 = file_name.rfind('\\');
      if (pos2 > pos)
        pos = pos2;
      if (pos >= 0)
        file_name.erase(0, pos2);

      cue_file_name = (string)tracks[i].out_name;
      pos = cue_file_name.rfind('.');
      pos2 = cue_file_name.rfind('/');
      pos3 = cue_file_name.rfind('\\');
      if ((pos >= 0) && (pos > pos2) && (pos > pos3))
        cue_file_name.erase(pos);
      cue_file_name += ".cue";

      try {
        out = new mm_file_io_c(cue_file_name.c_str(), MODE_CREATE);
      } catch(...) {
        mxerror(_("The file '%s' could not be opened for writing (%s).\n"),
                cue_file_name.c_str(), strerror(errno));
      }
      mxinfo(_("The CUE sheet for track %lld will be written to '%s'.\n"),
             tracks[i].tid, cue_file_name.c_str());
      write_cuesheet(file_name.c_str(), chapters, tags, tracks[i].tuid, *out);
      delete out;
    }
  }
}

bool
extract_tracks(const char *file_name) {
  int upper_lvl_el;
  // Elements for different levels
  EbmlElement *l0 = NULL, *l1 = NULL, *l2 = NULL, *l3 = NULL;
  EbmlStream *es;
  KaxCluster *cluster;
  KaxBlock *block;
  uint64_t cluster_tc, tc_scale = TIMECODE_SCALE, file_size;
  bool tracks_found;
  mm_io_c *in;
  KaxChapters all_chapters;
  KaxTags all_tags;

  block = NULL;
  // open input file
  try {
    in = new mm_file_io_c(file_name);
  } catch (std::exception &ex) {
    show_error(_("The file '%s' could not be opened for reading (%s).\n"),
               file_name, strerror(errno));
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
      show_error(_("Error: No EBML head found."));
      delete es;

      return false;
    }

    // Don't verify its data for now.
    l0->SkipData(*es, l0->Generic().Context);
    delete l0;

    while (1) {
      // Next element must be a segment
      l0 = es->FindNextID(KaxSegment::ClassInfos, 0xFFFFFFFFFFFFFFFFLL);
      if (l0 == NULL) {
        show_error(_("No segment/level 0 element found."));
        return false;
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
            show_element(l2, 2, _("Timecode scale: %llu"), tc_scale);
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
        create_extractors(*dynamic_cast<KaxTracks *>(l1));

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

      } else if (EbmlId(*l1) == KaxChapters::ClassInfos.GlobalId) {
        KaxChapters &chapters = *static_cast<KaxChapters *>(l1);
        chapters.Read(*es, KaxChapters::ClassInfos.Context, upper_lvl_el, l2,
                      true);
        while (chapters.ListSize() > 0) {
          if (EbmlId(*chapters[0]) == KaxEditionEntry::ClassInfos.GlobalId) {
            KaxEditionEntry &entry =
              *static_cast<KaxEditionEntry *>(chapters[0]);
            while (entry.ListSize() > 0) {
              if (EbmlId(*entry[0]) == KaxChapterAtom::ClassInfos.GlobalId)
                all_chapters.PushElement(*entry[0]);
              entry.Remove(0);
            }
          }
          chapters.Remove(0);
        }

      } else if (EbmlId(*l1) == KaxTags::ClassInfos.GlobalId) {
        KaxTags &tags = *static_cast<KaxTags *>(l1);
        tags.Read(*es, KaxTags::ClassInfos.Context, upper_lvl_el, l2, true);

        while (tags.ListSize() > 0) {
          all_tags.PushElement(*tags[0]);
          tags.Remove(0);
        }

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

    write_all_cuesheets(all_chapters, all_tags);

    // Now just close the files and go to sleep. Mummy will sing you a
    // lullaby. Just close your eyes, listen to her sweet voice, singing,
    // singing, fading... fad... ing...
    close_extractors();

    return true;
  } catch (exception &ex) {
    show_error(_("Caught exception: %s"), ex.what());
    delete in;

    return false;
  }
}
