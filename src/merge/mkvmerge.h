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
 * definition of global variables found in mkvmerge.cpp
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "os.h"

#include <string>

#include "mm_io.h"
#include "pr_generic.h"

#include <matroska/KaxChapters.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTag.h>

using namespace std;
using namespace libmatroska;

int64_t create_track_number(generic_reader_c *reader, int64_t tid);

#endif // __MKVMERGE_H
