/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * Declarations of external EBML Ids found in libebml
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __EBML_IDS_H
#define __EBML_IDS_H

#include "ebml/EbmlElement.h"

namespace libmatroska {
  extern EbmlId KaxChapters_TheId;
  extern EbmlId KaxEditionEntry_TheId;
  extern EbmlId KaxEditionUID_TheId;
  extern EbmlId KaxEditionFlagHidden_TheId;
  extern EbmlId KaxEditionFlagDefault_TheId;
  extern EbmlId KaxEditionManaged_TheId;
  extern EbmlId KaxChapterAtom_TheId;
  extern EbmlId KaxChapterUID_TheId;
  extern EbmlId KaxChapterTimeStart_TheId;
  extern EbmlId KaxChapterTimeEnd_TheId;
  extern EbmlId KaxChapterFlagHidden_TheId;
  extern EbmlId KaxChapterFlagEnabled_TheId;
  extern EbmlId KaxChapterPhysicalEquiv_TheId;
  extern EbmlId KaxChapterTrack_TheId;
  extern EbmlId KaxChapterTrackNumber_TheId;
  extern EbmlId KaxChapterDisplay_TheId;
  extern EbmlId KaxChapterString_TheId;
  extern EbmlId KaxChapterLanguage_TheId;
  extern EbmlId KaxChapterCountry_TheId;
  extern EbmlId KaxChapterProcess_TheId;
  extern EbmlId KaxChapterProcessTime_TheId;
}

#endif
