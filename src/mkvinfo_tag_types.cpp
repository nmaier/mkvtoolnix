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
 * tag type description lists for the deprecated tags
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "mkvinfo_tag_types.h"

const cstring commercial_types[NUM_COMMERCIAL_TYPES] = {
  "File purchase",
  "Item purchase",
  "Owner"
};

const cstring commercial_types_descr[NUM_COMMERCIAL_TYPES] = {
  "Information on where to purchase this file. This is akin to the WPAY tag "
  "in ID3.",
  "Information on where to purchase this album. This is akin to the WCOM tag "
  "in ID3.",
  "nformation on the purchase that occurred for this file. This is akin to "
  "the OWNE tag in ID3."
};

const cstring date_types[NUM_DATE_TYPES] = {
  "Encoding date",
  "Recording date",
  "Release date",
  "Original release date",
  "Tagging date",
  "Digitizing date"
};

const cstring date_types_descr[NUM_DATE_TYPES] = {
  "The time that the encoding of this item was completed. This is akin to the "
  "TDEN tag in ID3.",
  "The time that the recording began, and finished. This is akin to the TDRC "
  "tag in ID3.",
  "The time that the item was originaly released. This is akin to the TDRL "
  "tag in ID3.",
  "The time that the item was originaly released if it is a remake. This is "
  "akin to the TDOR tag in ID3.",
  "The time that the tags were done for this item. This is akin to the TDTG "
  "tag in ID3.",
  "The time that the item was tranfered to a digital medium. This is akin to "
  "the IDIT tag in RIFF."
};

const cstring entity_types[NUM_ENTITY_TYPES] = {
  "Lyricist / text writer",
  "Composer",
  "Lead performer(s) / Solist(s)",
  "Band / orchestra / accompaniment",
  "Original lyricist(s) / writer(s)",
  "Original artist(s) / performer(s)",
  "Original album / movie / show title",
  "Conductor / performer refinement",
  "Interpreted, remixed, or otherwise modified by",
  "Director",
  "Produced by",
  "Cinematographer",
  "Production designer",
  "Costume designer",
  "Production studio",
  "Distributed by",
  "Commissioned by",
  "Engineer",
  "Edited by",
  "Encoded by",
  "Ripped by",
  "Involved people list",
  "Internet radio station name",
  "Publisher"
};

const cstring entity_types_descr[NUM_ENTITY_TYPES] = {
  "The person that wrote the words/script for this item. This is akin to the "
  "TEXT tag in ID3.",
  "The name of the composer of this item. This is akin to the TCOM tag in "
  "ID3.",
  "This is akin to the TPE1 tag in ID3.",
  "This is akin to the TPE2 tag in ID3.",
  "This is akin to the TOLY tag in ID3.",
  "This is akin to the TOPE tag in ID3.",
  "This is akin to the TOAL tag in ID3.",
  "This is akin to the TPE3 tag in ID3.",
  "This is akin to the TPE4 tag in ID3.",
  "This is akin to the IART tag in RIFF.",
  "This is akin to the IPRO tag in Extended RIFF",
  "This is akin to the ICNM tag in Extended RIFF",
  "This is akin to the IPDS tag in Extended RIFF",
  "This is akin to the ICDS tag in Extended RIFF",
  "This is akin to the ISTD tag in Extended RIFF",
  "This is akin to the IDST tag in Extended RIFF",
  "This is akin to the ICMS tag in RIFF.",
  "This is akin to the IENG tag in RIFF.",
  "This is akin to the IEDT tag in Extended RIFF",
  "This is akin to the TENC tag in ID3.",
  "This is akin to the IRIP tag in Extended RIFF",
  "A very general tag for everyone else that wants to be listed. This is "
  "akin to the TMCL tag in ID3v2.4.",
  "This is akin to the TSRN tag in ID3v2.4.",
  "This is akin to the TPUB tag in ID3."
};

const cstring identifier_types[NUM_IDENTIFIER_TYPES] = {
  "ISRC",
  "Music CD identifier",
  "ISBN",
  "Catalog",
  "EAN",
  "UPC",
  "Label code",
  "LCCN",
  "Unique file identifier",
  "CDROM_CD_TEXT_PACK_TOC_INFO2"
};

const cstring identifier_types_descr[NUM_IDENTIFIER_TYPES] = {
  "The International Standard Recording Code",
  "This is a binary dump of the TOC of the CDROM that this item was taken "
  "from. This holds the same information as the MCDI in ID3.",
  "International Standard Book Number",
  "sometimes the EAN/UPC, often some letters followed by some numbers",
  "EAN-13 bar code identifier ",
  "UPC-A bar code identifier ",
  "Typically printed as ________ (LC) xxxx) ~~~~~~~~ or _________ (LC) 0xxxx) "
  "~~~~~~~~~ on CDs medias or covers, where xxxx is a 4-digit number.",
  "Library of Congress Control Number",
  "This used for a dump of the UFID field in ID3. This field would only be "
  "used if the item was pulled from an MP3.",
  ""
};

const cstring legal_types[NUM_LEGAL_TYPES] = {
  "Copyright",
  "Production copyright",
  "Terms of use"
};

const cstring legal_types_descr[NUM_LEGAL_TYPES] = {
  "The copyright information as per the copyright holder. This is akin to the "
  "TCOP tag in ID3.",
  "The copyright information as per the production copyright holder. This is "
  "akin to the TPRO tag in ID3.",
  "The terms of use for this item. This is akin to the USER tag in ID3."
};

const cstring title_types[NUM_TITLE_TYPES] = {
  "Track title",
  "Album / movie / show title",
  "Set title",
  "Series"
};

const cstring title_types_descr[NUM_TITLE_TYPES] = {
  "The title of this item. In the case of a track, the MultiName element "
  "should be identical to the Name element. For example, for music you might "
  "label this \"Canon in D\", or for video's audio track you might use "
  "\"English 5.1\" This is akin to the TIT2 tag in ID3.",
  "This is the name given to a grouping of tracks and/or chapters. For "
  "example, all video, audio, and subtitle tracks for a movie would be "
  "grouped under this and be given the name of the movie. All tracks for a "
  "particular CD would be grouped together under the title of the CD, or if "
  "all tracks for a CD were recorded as a single track, seperated by "
  "chapters, the same would apply. You could use this to label episode 3 of "
  "The Simpsons. This is akin to the TALB tag in ID3.",
  "This would be used to label a set of ID 2. For example, season 13 of The "
  "Simpsons.",
  "This would be used to label a set of ID 3. For example, The Simpsons."
};

