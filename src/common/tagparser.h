/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  tagparser.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief definition of global variables and functions for the XML tag parser
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TAGPARSER_H
#define __TAGPARSER_H

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTagMulti.h>

#include <string>
#include <vector>

#include <expat.h>

using namespace std;
using namespace libebml;
using namespace libmatroska;

#define E_Tags                            0
#define E_Tag                             1
#define E_Targets                         2
#define E_TrackUID                        3
#define E_ChapterUID                      4
#define E_General                         5
#define E_Subject                         6
#define E_Biography                       7
#define E_Language                        8
#define E_Rating                          9
#define E_Encoder                        10
#define E_EncodeSettings                 11
#define E_File                           12
#define E_ArchivalLocation               13
#define E_Keywords                       14
#define E_Mood                           15
#define E_RecordLocation                 16
#define E_Source                         17
#define E_SourceForm                     18
#define E_Product                        19
#define E_OriginalMediaType              20
#define E_PlayCounter                    21
#define E_Popularimeter                  22
#define E_Genres                         23
#define E_AudioGenre                     24
#define E_VideoGenre                     25
#define E_SubGenre                       26
#define E_AudioSpecific                  27
#define E_AudioEncryption                28
#define E_AudioGain                      29
#define E_AudioPeak                      30
#define E_BPM                            31
#define E_Equalisation                   32
#define E_DiscTrack                      33
#define E_SetPart                        34
#define E_InitialKey                     35
#define E_OfficialAudioFileURL           36
#define E_OfficialAudioSourceURL         37
#define E_ImageSpecific                  38
#define E_CaptureDPI                     39
#define E_CaptureLightness               40
#define E_CapturePaletteSetting          41
#define E_CaptureSharpness               42
#define E_Cropped                        43
#define E_OriginalDimensions             44
#define E_MultiCommercial                45
#define E_Commercial                     46
#define E_CommercialType                 47
#define E_Address                        48
#define E_URL                            49
#define E_Email                          50
#define E_MultiPrice                     51
#define E_Currency                       52
#define E_Amount                         53
#define E_PriceDate                      54
#define E_MultiDate                      55
#define E_Date                           56
#define E_DateType                       57
#define E_DateBegin                      58
#define E_DateEnd                        59
#define E_MultiEntity                    60
#define E_Entity                         61
#define E_EntityType                     62
#define E_Name                           63
#define E_MultiIdentifier                64
#define E_Identifier                     65
#define E_IdentifierType                 66
#define E_IdentifierBinary               67
#define E_IdentifierString               68
#define E_MultiLegal                     69
#define E_Legal                          70
#define E_LegalType                      71
#define E_LegalContent                   80
#define E_MultiTitle                     72
#define E_Title                          73
#define E_TitleType                      74
#define E_SubTitle                       75
#define E_Edition                        76
#define E_MultiComment                   77
#define E_CommentName                    78
#define E_Comments                       79
#define E_CommentLanguage                81

// MAX: 81

typedef struct {
  XML_Parser parser;

  const char *file_name;

  int depth;
  bool done_reading, data_allowed;

  vector<string> *parent_names;
  vector<int> *parents;

  string *bin;

  KaxTags *tags;

  KaxTag *tag;

  KaxTagTargets *targets;
  KaxTagTrackUID *track_uid;
  KaxTagChapterUID *chapter_uid;

  KaxTagGeneral *general;
  KaxTagKeywords *keywords;
  KaxTagRecordLocation *rec_location;

  KaxTagGenres *genres;
  KaxTagAudioGenre *audio_genre;
  KaxTagVideoGenre *video_genre;

  KaxTagAudioSpecific *audio_specific;

  KaxTagImageSpecific *image_specific;

  KaxTagMultiCommercial *m_commercial;
  KaxTagCommercial *commercial;
  KaxTagMultiCommercialURL *c_url;
  KaxTagMultiCommercialEmail *c_email;
  KaxTagMultiPrice *m_price;

  KaxTagMultiDate *m_date;
  KaxTagDate *date;

  KaxTagMultiEntity *m_entity;
  KaxTagEntity *entity;
  KaxTagMultiEntityURL *e_url;
  KaxTagMultiEntityEmail *e_email;

  KaxTagMultiIdentifier *m_identifier;
  KaxTagIdentifier *identifier;

  KaxTagMultiLegal *m_legal;
  KaxTagLegal *legal;
  KaxTagMultiLegalURL *l_url;

  KaxTagMultiTitle *m_title;
  KaxTagTitle *title;
  KaxTagMultiTitleURL *t_url;
  KaxTagMultiTitleEmail *t_email;

  KaxTagMultiComment *m_comment;
} parser_data_t;

void tperror(parser_data_t *pdata, const char *fmt, ...);

#define tperror_unknown() tperror(pdata, "Unknown/unsupported element: %s", \
                                  name)
#define tperror_nochild() tperror(pdata, "<%s> is not a valid child element " \
                                  "of <%s>.", name, parent_name.c_str())
#define tperror_oneinstance() tperror(pdata, "Only one instance of <%s> is " \
                                      "allowed under <%s>.", name, \
                                      parent_name.c_str());
 
void end_xml_tag_element(void *user_data, const char *name);

void parse_xml_tags(const char *name, KaxTags *tags);

#endif // __TAGPARSER_H
