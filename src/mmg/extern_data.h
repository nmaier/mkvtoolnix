/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  extern_data.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declarations for ComboBox data
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __EXTERN_DATA_H
#define __EXTERN_DATA_H

typedef struct {
  const char *name, *extensions;
} mime_type_t;

extern const char *sub_charsets[];
extern const mime_type_t mime_types[];          
extern const char *cctlds[];

#endif // __EXTERN_DATA_H
