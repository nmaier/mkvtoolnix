/*
  mkvextract -- extract tracks from Matroska files into other files

  tagwriter.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief writes tags in XML format
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TAGWRITER_H
#define __TAGWRITER_H

#include <stdio.h>

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace libmatroska;

void MTX_DLL_API write_tags_xml(KaxTags &tags, FILE *out);

#endif // __TAGWRITER_H
