/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   The cluster helper groups frames into blocks groups and those
   into clusters, sets the durations, renders the clusters etc.

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_LIBMATROSKA_EXTENSIONS
#define MTX_LIBMATROSKA_EXTENSIONS

#include "common/common_pch.h"

#include <ebml/EbmlVersion.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxSeekHead.h>

using namespace libebml;
using namespace libmatroska;

#if LIBEBML_VERSION < 0x000800
typedef uint64 filepos_t;
#endif

class kax_cluster_c: public KaxCluster {
public:
  kax_cluster_c(): KaxCluster() {
    PreviousTimecode = 0;
  }

  void delete_non_blocks();

  void set_min_timecode(int64_t min_timecode) {
    MinTimecode = min_timecode;
  }
  void set_max_timecode(int64_t max_timecode) {
    MaxTimecode = max_timecode;
  }
};

class kax_reference_block_c: public KaxReferenceBlock {
protected:
  int64_t m_value;

public:
  kax_reference_block_c();

  void set_value(int64_t value) {
    m_value     = value;
#if LIBEBML_VERSION < 0x000800
    bValueIsSet = true;
#else
    SetValueIsSet();
#endif
  }

  virtual filepos_t UpdateSize(bool bSaveDefault, bool bForceRender);
};

class kax_block_group_c: public KaxBlockGroup {
public:
  kax_block_group_c(): KaxBlockGroup() {
  }

  bool add_frame(const KaxTrackEntry &track, uint64 timecode, DataBuffer &buffer, int64_t past_block, int64_t forw_block, LacingType lacing);
};

class kax_block_blob_c: public KaxBlockBlob {
public:
  kax_block_blob_c(BlockBlobType type): KaxBlockBlob(type) {
  }

  bool add_frame_auto(const KaxTrackEntry &track, uint64 timecode, DataBuffer &buffer, LacingType lacing, int64_t past_block, int64_t forw_block);
  void set_block_duration(uint64_t time_length);
  bool replace_simple_by_group();
};
typedef std::shared_ptr<kax_block_blob_c> kax_block_blob_cptr;

#endif // MTX_LIBMATROSKA_EXTENSIONS
