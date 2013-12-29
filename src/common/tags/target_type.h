/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for tag target types

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_TAG_TARGET_TYPE_H
#define MTX_COMMON_TAG_TARGET_TYPE_H

// see http://www.matroska.org/technical/specs/tagging/index.html#targettypes
#define TAG_TARGETTYPE_COLLECTION     70

#define TAG_TARGETTYPE_EDITION        60
#define TAG_TARGETTYPE_ISSUE          60
#define TAG_TARGETTYPE_VOLUME         60
#define TAG_TARGETTYPE_OPUS           60
#define TAG_TARGETTYPE_SEASON         60
#define TAG_TARGETTYPE_SEQUEL         60
#define TAG_TARGETTYPE_VOLUME         60

#define TAG_TARGETTYPE_ALBUM          50
#define TAG_TARGETTYPE_OPERA          50
#define TAG_TARGETTYPE_CONCERT        50
#define TAG_TARGETTYPE_MOVIE          50
#define TAG_TARGETTYPE_EPISODE        50

#define TAG_TARGETTYPE_PART           40
#define TAG_TARGETTYPE_SESSION        40

#define TAG_TARGETTYPE_TRACK          30
#define TAG_TARGETTYPE_SONG           30
#define TAG_TARGETTYPE_CHAPTER        30

#define TAG_TARGETTYPE_SUBTRACK       20
#define TAG_TARGETTYPE_MOVEMENT       20
#define TAG_TARGETTYPE_SCENE          20

#define TAG_TARGETTYPE_SHOT           10

#endif // MTX_COMMON_TAGS_TARGET_TYPE_H
