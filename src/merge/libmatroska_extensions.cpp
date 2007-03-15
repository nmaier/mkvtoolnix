/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   The cluster helper groups frames into blocks groups and those
   into clusters, sets the durations, renders the clusters etc.

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cassert>

#include "common.h"
#include "libmatroska_extensions.h"

kax_reference_block_c::kax_reference_block_c():
  KaxReferenceBlock(), m_value(-1) {
}

uint64
kax_reference_block_c::UpdateSize(bool bSaveDefault,
                                  bool bForceRender) {
  if (!bTimecodeSet) {
    assert(m_value != -1);

    Value = (m_value - int64(ParentBlock->GlobalTimecode())) /
      int64(ParentBlock->GlobalTimecodeScale());
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
	assert(ParentCluster != NULL);
	block.SetParent(*ParentCluster);
	ParentTrack = &track;
	bool result = block.AddFrame(track, timecode, buffer, lacing);

  if (0 <= past_block) {
    kax_reference_block_c *past_ref = FindChild<kax_reference_block_c>(*this);
    if (NULL == past_ref) {
      past_ref = new kax_reference_block_c;
      PushElement(*past_ref);
    }
    past_ref->set_value(past_block);
    past_ref->SetParentBlock(*this);
  }

  if (0 <= forw_block) {
    kax_reference_block_c *forw_ref = FindChild<kax_reference_block_c>(*this);
    if (NULL == forw_ref) {
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

  if ((SimpleBlockMode == BLOCK_BLOB_ALWAYS_SIMPLE) ||
      ((SimpleBlockMode == BLOCK_BLOB_SIMPLE_AUTO) &&
       (-1 == past_block) && (-1 == forw_block))) {
    assert(bUseSimpleBlock == true);
    if (Block.simpleblock == NULL) {
      Block.simpleblock = new KaxSimpleBlock();
      Block.simpleblock->SetParent(*ParentCluster);
    }

    result = Block.simpleblock->AddFrame(track, timecode, buffer, lacing);
    if ((-1 == past_block) && (-1 == forw_block)) {
      Block.simpleblock->SetKeyframe(true);
      Block.simpleblock->SetDiscardable(false);
    } else {
      Block.simpleblock->SetKeyframe(false);
      if (((-1 == forw_block) || (forw_block <= timecode)) &&
          ((-1 == past_block) || (past_block <= timecode)))
        Block.simpleblock->SetDiscardable(false);
      else
        Block.simpleblock->SetDiscardable(true);
    }
  } else if (replace_simple_by_group()) {
    kax_block_group_c *group =
      static_cast<kax_block_group_c *>(Block.group);
    result = group->add_frame(track, timecode, buffer, past_block, forw_block,
                              lacing);
  }

  return result;
}

bool
kax_block_blob_c::replace_simple_by_group() {
  if (SimpleBlockMode== BLOCK_BLOB_ALWAYS_SIMPLE)
    return false;

  if (!bUseSimpleBlock) {
    if (Block.group == NULL)
      Block.group = new kax_block_group_c();

  } else if (Block.simpleblock != NULL)
    assert(false);
  else
    Block.group = new kax_block_group_c();

  if (ParentCluster != NULL)
    Block.group->SetParent(*ParentCluster);

  bUseSimpleBlock = false;

  return true;
}

void
kax_block_blob_c::set_block_duration(uint64_t time_length) {
	if (replace_simple_by_group())
		Block.group->SetBlockDuration(time_length);
}
