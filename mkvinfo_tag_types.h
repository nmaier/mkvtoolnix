/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mkvinfo_tag_types.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief definition of tag type description lists
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MKVINFO_TAG_TYPES_H
#define __MKVINFO_TAG_TYPES_H

typedef char * cstring;

#define NUM_COMMERCIAL_TYPES  3
#define NUM_DATE_TYPES        6
#define NUM_ENTITY_TYPES     24
#define NUM_IDENTIFIER_TYPES 10
#define NUM_LEGAL_TYPES       3
#define NUM_TITLE_TYPES       4

extern const cstring commercial_types[NUM_COMMERCIAL_TYPES];
extern const cstring commercial_types_descr[NUM_COMMERCIAL_TYPES];
extern const cstring date_types[NUM_DATE_TYPES];
extern const cstring date_types_descr[NUM_DATE_TYPES];
extern const cstring entity_types[NUM_ENTITY_TYPES];
extern const cstring entity_types_descr[NUM_ENTITY_TYPES];
extern const cstring identifier_types[NUM_IDENTIFIER_TYPES];
extern const cstring identifier_types_descr[NUM_IDENTIFIER_TYPES];
extern const cstring legal_types[NUM_LEGAL_TYPES];
extern const cstring legal_types_descr[NUM_LEGAL_TYPES];
extern const cstring title_types[NUM_TITLE_TYPES];
extern const cstring title_types_descr[NUM_TITLE_TYPES];


#endif // __MKVINFO_TAG_TYPES_H
