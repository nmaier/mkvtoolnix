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

#include <stdio.h>

#include <typeinfo>

#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTagMulti.h>

using namespace libmatroska;
using namespace std;

#include "base64.h"
#include "common.h"
#include "matroska.h"

static FILE *o;

#define UINT uint64(*static_cast<EbmlUInteger *>(e))
#define SINT int64(*static_cast<EbmlSInteger *>(e))
#define FLOAT float(*static_cast<EbmlFloat *>(e))
#define U8STR UTFstring_to_cstrutf8(UTFstring( \
  *static_cast<EbmlUnicodeString *>(e)))
#define STR string(*static_cast<EbmlString *>(e)).c_str()


#define pr(l, n, f, a) \
{ \
  int __idx; \
  for (__idx = 0; __idx < l; __idx++) \
    fprintf(o, " "); \
  fprintf(o, "<%s>", n); \
  fprintf(o, f, a); \
  mxprint(o, "</%s>\n", n); \
}

#define prUI(l, n) pr(l, n, "%llu", UINT)
#define prSI(l, n) pr(l, n, "%lld", SINT)
#define prF(l, n) pr(l, n, "%f", FLOAT)
#define prU8S(l, n) \
{ \
  s = U8STR; \
  pr(l, n, "%s", s); \
  safefree(s); \
}
#define prS(l, n) pr(l, n, "%s", STR)

static void print_binary(int level, const char *name, EbmlElement *e) {
  EbmlBinary *b;
  string s;
  int i, idx, old_idx;

  b = (EbmlBinary *)e;
  s = base64_encode((const unsigned char *)b->GetBuffer(), b->GetSize(), true,
                    72 - level - 2);
  for (i = 0; i < level; i++)
    mxprint(o, " ");
  mxprint(o, "<%s>\n", name);

  for (i = 0; i < (level + 2); i++)
    mxprint(o, " ");

  old_idx = 0;
  for (idx = 0; idx < s.length(); idx++)
    if (s[idx] == '\n') {
      mxprint(o, "%s\n", s.substr(old_idx, idx - old_idx).c_str());
      for (i = 0; i < (level + 2); i++)
        mxprint(o, " ");
      old_idx = idx + 1;
    }

  if (s[s.length() - 1] != '\n')
    mxprint(o, "\n");

  for (i = 0; i < level; i++)
    mxprint(o, " ");
  mxprint(o, "</%s>\n", name);
}

static void handle_level4(EbmlElement *e) {
  int i;
  char *s;

  if (e->Generic().GlobalId == KaxTagTrackUID::ClassInfos.GlobalId)
    prUI(3, "TrackUID");

  else if (e->Generic().GlobalId == KaxTagChapterUID::ClassInfos.GlobalId)
    prUI(3, "ChapterUID");

  else if (e->Generic().GlobalId == KaxTagSubject::ClassInfos.GlobalId)
    prU8S(3, "Subject");

  else if (e->Generic().GlobalId == KaxTagBibliography::ClassInfos.GlobalId)
    prU8S(3, "Bibliography");

  else if (e->Generic().GlobalId == KaxTagLanguage::ClassInfos.GlobalId)
    prS(3, "Language");

  else if (e->Generic().GlobalId == KaxTagRating::ClassInfos.GlobalId)
    print_binary(3, "Rating", e);

  else if (e->Generic().GlobalId == KaxTagEncoder::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <Encoder>%s</Encoder>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId ==
           KaxTagEncodeSettings::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <EncodeSettings>%s</EncodeSettings>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagFile::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <File>%s</File>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId ==
           KaxTagArchivalLocation::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <ArchivalLocation>%s</ArchivalLocation>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagKeywords::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <Keywords>%s</Keywords>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagMood::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <Mood>%s</Mood>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId ==
           KaxTagRecordLocation::ClassInfos.GlobalId)
    mxprint(o, "   <RecordLocation>%s</RecordLocation>\n", STR);

  else if (e->Generic().GlobalId == KaxTagSource::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <Source>%s</Source>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagSourceForm::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <SourceForm>%s</SourceForm>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagProduct::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <Product>%s</Product>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId ==
           KaxTagOriginalMediaType::ClassInfos.GlobalId) {
    s = U8STR;
    mxprint(o, "   <OriginalMediaType>%s</OriginalMediaType>\n", s);
    safefree(s);
  }

  else if (e->Generic().GlobalId == KaxTagPlayCounter::ClassInfos.GlobalId)
    mxprint(o, "   <PlayCounter>%llu</PlayCounter>\n", UINT);

  else if (e->Generic().GlobalId == KaxTagPopularimeter::ClassInfos.GlobalId)
    mxprint(o, "   <Popularimeter>%lld</Popularimeter>\n", SINT);

  else if (e->Generic().GlobalId == KaxTagAudioGenre::ClassInfos.GlobalId)
    mxprint(o, "   <AudioGenre>%s</AudioGenre>\n", STR);

  else if (e->Generic().GlobalId == KaxTagVideoGenre::ClassInfos.GlobalId)
    print_binary(3, "VideoGenre", e);

  else if (e->Generic().GlobalId == KaxTagSubGenre::ClassInfos.GlobalId)
    mxprint(o, "   <SubGenre>%s</SubGenre>\n", STR);

  else if (e->Generic().GlobalId == KaxTagAudioEncryption::ClassInfos.GlobalId)
    print_binary(3, "AudioEncryption", e);

  else if (e->Generic().GlobalId == KaxTagAudioGain::ClassInfos.GlobalId)
    mxprint(o, "   <AudioGain>%f</AudioGain>\n", FLOAT);

  else if (e->Generic().GlobalId == KaxTagAudioPeak::ClassInfos.GlobalId)
    mxprint(o, "   <AudioPeak>%f</AudioPeak>\n", FLOAT);

  else if (e->Generic().GlobalId == KaxTagBPM::ClassInfos.GlobalId)
    mxprint(o, "   <BPM>%f</BPM>\n", FLOAT);

  else if (e->Generic().GlobalId == KaxTagEqualisation::ClassInfos.GlobalId)
    print_binary(3, "Equalisation", e);

  else if (e->Generic().GlobalId == KaxTagDiscTrack::ClassInfos.GlobalId)
    mxprint(o, "   <DiscTrack>%llu</DiscTrack>\n", UINT);

  else if (e->Generic().GlobalId == KaxTagSetPart::ClassInfos.GlobalId)
    mxprint(o, "   <SetPart>%llu</SetPart>\n", UINT);

  else if (e->Generic().GlobalId == KaxTagInitialKey::ClassInfos.GlobalId)
    mxprint(o, "   <InitialKey>%s</InitialKey>\n", STR);

  else if (e->Generic().GlobalId ==
           KaxTagOfficialAudioFileURL::ClassInfos.GlobalId)
    mxprint(o, "   <OfficialFileURL>%s</OfficialFileURL>\n", STR);

  else if (e->Generic().GlobalId ==
           KaxTagOfficialAudioSourceURL::ClassInfos.GlobalId)
    mxprint(o, "   <OfficialSourceURL>%s</OfficialSourceURL>\n", STR);


  

  else
    mxprint(stderr, "   Unknown element: %s\n", typeid(*e).name());
}

static void handle_level3(EbmlElement *e) {
  int i;

  if (e->Generic().GlobalId == KaxTagTargets::ClassInfos.GlobalId) {
    mxprint(o, "  <Targets>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </Targets>\n");

  } else if (e->Generic().GlobalId == KaxTagGeneral::ClassInfos.GlobalId) {
    mxprint(o, "  <General>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </General>\n");

  } else if (e->Generic().GlobalId == KaxTagGenres::ClassInfos.GlobalId) {
    mxprint(o, "  <Genres>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </Genres>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagAudioSpecific::ClassInfos.GlobalId) {
    mxprint(o, "  <AudioSpecific>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </AudioSpecific>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagImageSpecific::ClassInfos.GlobalId) {
    mxprint(o, "  <ImageSpecific>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </ImageSpecific>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiCommercial::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiCommercial>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiCommercial>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiDate::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiDate>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiDate>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiEntity::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiEntity>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiEntity>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiIdentifier::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiIdentifier>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiIdentifier>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiLegal::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiLegal>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiLegal>\n");

  } else if (e->Generic().GlobalId ==
             KaxTagMultiTitle::ClassInfos.GlobalId) {
    mxprint(o, "  <MultiTitle>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </MultiTitle>\n");

  } else
    mxprint(stderr, "  Unknown element: %s\n", typeid(*e).name());
}

static void handle_level2(EbmlElement *e) {
  int i;

  if (e->Generic().GlobalId == KaxTag::ClassInfos.GlobalId) {
    mxprint(o, " <Tag>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, " </Tag>\n");
  } else
    mxprint(stderr, " Unknown element: %s\n", typeid(*e).name());

}

static void dumpsizes(EbmlElement *e, int level) {
  int i;

  for (i = 0; i < level; i++)
    printf(" ");
  printf("%s", typeid(*e).name());

  try {
    EbmlMaster *m = &dynamic_cast<EbmlMaster &>(*e);
    if (m != NULL) {
      printf(" (size: %u)\n", m->ListSize());
      for (i = 0; i < m->ListSize(); i++)
        dumpsizes((*m)[i], level + 1);
    } else
      printf("\n");
  } catch (...) {
      printf("\n");
  }
}

void write_tags_xml(KaxTags &tags, FILE *out) {
  int i;

  o = out;

//   dumpsizes(&tags, 0);

  for (i = 0; i < tags.ListSize(); i++)
    handle_level2(tags[i]);
}
