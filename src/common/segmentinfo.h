/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   segment info parser/writer functions

   Written by Moritz Bunkus <moritz@bunkus.org> and
   Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __SEGMENTINFO_H
#define __SEGMENTINFO_H

#include <stdio.h>

#include "ebml/EbmlElement.h"

#include "matroska/KaxInfo.h"
#include "matroska/KaxInfoData.h"

#include "common.h"
#include "mm_io.h"

using namespace libebml;
using namespace libmatroska;

KaxInfo *MTX_DLL_API parse_segmentinfo(const string &file_name, bool exception_on_error = false);

KaxInfo *MTX_DLL_API parse_xml_segmentinfo(mm_text_io_c *in, bool exception_on_error = false);

void MTX_DLL_API fix_mandatory_segmentinfo_elements(EbmlElement *e);

#endif // __SEGMENTINFO_H

