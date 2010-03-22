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

#include "common/common.h"

#include <cassert>

#include "merge/libmatroska_extensions.h"

kax_reference_block_c::kax_reference_block_c():
  KaxReferenceBlock(), m_value(-1) {
}

filepos_t
kax_reference_block_c::UpdateSize(bool bSaveDefault,
                                  bool bForceRender) {
  if (!bTimecodeSet) {
    assert(-1 != m_value);
    *static_cast<EbmlSInteger*>(this) = (m_value - int64(ParentBlock->GlobalTimecode())) / int64(ParentBlock->GlobalTimecodeScale());
  }

  return EbmlSInteger::UpdateSize(bSaveDefault, bForceRender);
}

bool
kax_block_group_c::add_frame(const KaxTrackEntry &track,
                             uint64 timecode,
                             DataBuffer &buffer,
                             int64_t past_block,
                             int64_t forw_block,
                             LacingType lacing) {
  KaxBlock & block = GetChild<KaxBlock>(*this);
  assert(NULL != ParentCluster);
  block.SetParent(*ParentCluster);

  ParentTrack                     = &track;
  bool result                     = block.AddFrame(track, timecode, buffer, lacing);
  kax_reference_block_c *past_ref = NULL;

  if (0 <= past_block) {
    past_ref = FindChild<kax_reference_block_c>(*this);
    if (NULL == past_ref) {
      past_ref = new kax_reference_block_c;
      PushElement(*past_ref);
    }
    past_ref->set_value(past_block);
    past_ref->SetParentBlock(*this);
  }

  if (0 <= forw_block) {
    kax_reference_block_c *forw_ref = FindChild<kax_reference_block_c>(*this);
    if (past_ref == forw_ref) {
      forw_ref = new kax_reference_block_c;
      PushElement(*forw_ref);
    }
    forw_ref->set_value(forw_block);
    forw_ref->SetParentBlock(*this);
  }

  return result;
}

bool
kax_block_blob_c::add_frame_auto(const KaxTrackEntry &track,
                                 uint64 timecode,
                                 DataBuffer &buffer,
                                 LacingType lacing,
                                 int64_t past_block,
                                 int64_t forw_block) {
  bool result = false;

  if (   (BLOCK_BLOB_ALWAYS_SIMPLE == SimpleBlockMode)
      || (   (BLOCK_BLOB_SIMPLE_AUTO == SimpleBlockMode)
          && (-1 == past_block)
          && (-1 == forw_block))) {
    assert(true == bUseSimpleBlock);
    if (NULL == Block.simpleblock) {
      Block.simpleblock = new KaxSimpleBlock();
      Block.simpleblock->SetParent(*ParentCluster);
    }

    result = Block.simpleblock->AddFrame(track, timecode, buffer, lacing);
    if ((-1 == past_block) && (-1 == forw_block)) {
      Block.simpleblock->SetKeyframe(true);
      Block.simpleblock->SetDiscardable(false);

    } else {
      Block.simpleblock->SetKeyframe(false);
      if (   ((-1 == forw_block) || (forw_block <= timecode))
          && ((-1 == past_block) || (past_block <= timecode)))
        Block.simpleblock->SetDiscardable(false);
      else
        Block.simpleblock->SetDiscardable(true);
    }

  } else if (replace_simple_by_group()) {
    kax_block_group_c *group = static_cast<kax_block_group_c *>(Block.group);
    result = group->add_frame(track, timecode, buffer, past_block, forw_block, lacing);
  }

  return result;
}

bool
kax_block_blob_c::replace_simple_by_group() {
  if (BLOCK_BLOB_ALWAYS_SIMPLE == SimpleBlockMode)
    return false;

  if (!bUseSimpleBlock) {
    if (NULL == Block.group)
      Block.group = new kax_block_group_c();

  } else if (NULL != Block.simpleblock)
    assert(false);
  else
    Block.group = new kax_block_group_c();

  if (NULL != ParentCluster)
    Block.group->SetParent(*ParentCluster);

  bUseSimpleBlock = false;

  return true;
}

void
kax_block_blob_c::set_block_duration(uint64_t time_length) {
  if (replace_simple_by_group())
    Block.group->SetBlockDuration(time_length);
}
