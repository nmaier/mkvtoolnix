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

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <matroska/KaxTag.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTagMulti.h>

using namespace libmatroska;
using namespace std;

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "matroska.h"

static FILE *o;

static void print_tag(int level, const char *name, const char *fmt, ...) {
  int idx;
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  for (idx = 0; idx < level; idx++)
    mxprint(o, "  ");
  mxprint(o, "<%s>", name);
  va_start(ap, fmt);
  vfprintf(o, fmt, ap);
  va_end(ap);
  mxprint(o, "</%s>\n", name);
}

static void print_binary(int level, const char *name, EbmlElement *e) {
  EbmlBinary *b;
  string s;
  int i, idx, old_idx;

  b = (EbmlBinary *)e;
  s = base64_encode((const unsigned char *)b->GetBuffer(), b->GetSize(), true,
                    72 - level - 2);
  if (s[s.length() - 1] == '\n')
    s.erase(s.length() - 1);

  for (i = 0; i < level; i++)
    mxprint(o, "  ");

  if ((level * 2 + 2 * strlen(name) + 2 + 3 + s.length()) <= 78) {
    mxprint(o, "<%s>%s</%s>\n", name, s.c_str(), name);
    return;
  }

  mxprint(o, "<%s>\n", name);

  for (i = 0; i < (level + 2); i++)
    mxprint(o, "  ");

  old_idx = 0;
  for (idx = 0; idx < s.length(); idx++)
    if (s[idx] == '\n') {
      mxprint(o, "%s\n", s.substr(old_idx, idx - old_idx).c_str());
      for (i = 0; i < (level + 2); i++)
        mxprint(o, "  ");
      old_idx = idx + 1;
    }

  if (old_idx < s.length())
    mxprint(o, "%s", s.substr(old_idx).c_str());

  mxprint(o, "\n");

  for (i = 0; i < level; i++)
    mxprint(o, "  ");
  mxprint(o, "</%s>\n", name);
}

static void print_date(int level, const char *name, EbmlElement *e) {
  int idx;
  time_t tme;
  struct tm *tm;
  char buffer[100];

  for (idx = 0; idx < level; idx++)
    mxprint(o, "  ");
  mxprint(o, "<%s>", name);

  tme = ((EbmlDate *)e)->GetEpochDate();
  tm = gmtime(&tme);
  if (tm == NULL)
    mxprint(o, "INVALID: %llu", (uint64_t)tme);
  else {
    buffer[99] = 0;
    strftime(buffer, 99, "%Y-%m-%dT%H:%M:%S+0000", tm);
    mxprint(o, buffer);
  }
  
  mxprint(o, "</%s>\n", name);
}

static void print_string(int level, const char *name, const char *src) {
  int idx;
  string s;

  idx = 0;
  s = "";
  while (src[idx] != 0) {
    if (src[idx] == '&')
      s += "&amp;";
    else if (src[idx] == '<')
      s += "&lt;";
    else if (src[idx] == '>')
      s += "&gt;";
    else if (src[idx] == '"')
      s += "&quot;";
    else
      s += src[idx];
    idx++;
  }
  for (idx = 0; idx < level; idx++)
    mxprint(o, "  ");
  mxprint(o, "<%s>%s</%s>\n", name, s.c_str(), name);
}

static void print_utf_string(int level, const char *name, EbmlElement *e) {
  char *s;

  s = UTFstring_to_cstrutf8(*static_cast<EbmlUnicodeString *>(e));
  print_string(level, name, s);
  safefree(s);
}

static void print_unknown(int level, EbmlElement *e) {
  int idx;

  for (idx = 0; idx < level; idx++)
    mxprint(o, "  ");
  mxprint(o, "<!-- Unknown element: %s -->\n", e->Generic().DebugName);
}

#define pr_ui(n) print_tag(level, n, "%llu", \
                           uint64(*static_cast<EbmlUInteger *>(e)))
#define pr_si(n) print_tag(level, n, "%lld", \
                           int64(*static_cast<EbmlSInteger *>(e)))
#define pr_f(n) print_tag(level, n, "%f", \
                          float(*static_cast<EbmlFloat *>(e)))
#define pr_s(n) print_string(level, n, \
                             string(*static_cast<EbmlString *>(e)).c_str())
#define pr_us(n) print_utf_string(level, n, e)
#define pr_d(n) print_date(level, n, e)

#define pr_b(n) print_binary(level, n, e)

#define is_id(ref) (e->Generic().GlobalId == ref::ClassInfos.GlobalId)

#define pr_unk() print_unknown(level, e)

static void handle_multicomments(EbmlElement *e, int level) {
  if (is_id(KaxTagMultiCommentName))
    pr_s("Name");

  else if (is_id(KaxTagMultiCommentComments))
    pr_us("Comments");

  else if (is_id(KaxTagMultiCommentLanguage))
    pr_s("Language");

  else
    pr_unk();
}

static void handle_simpletags(EbmlElement *e, int level) {
  int i;

  if (is_id(KaxTagName))
    pr_us("Name");

  else if (is_id(KaxTagString))
    pr_us("String");

  else if (is_id(KaxTagBinary))
    pr_b("Binary");

  else if (is_id(KaxTagSimple)) {
    for (i = 0; i < level; i++)
      mxprint(o, "  ");
    mxprint(o, "<Simple>\n");
    for (i = 0; i < (int)((EbmlMaster *)e)->ListSize(); i++)
      handle_simpletags((*((EbmlMaster *)e))[i], level + 1);
    for (i = 0; i < level; i++)
      mxprint(o, "  ");
    mxprint(o, "</Simple>\n");

  } else
    pr_unk();
}

static void handle_level5(EbmlElement *e) {
  int i, level = 5;

  if (is_id(KaxTagMultiPriceCurrency))
    pr_s("Currency");

  else if (is_id(KaxTagMultiPriceAmount))
    pr_f("Amount");

  else if (is_id(KaxTagMultiPricePriceDate))
    pr_d("PriceDate");

  else if (is_id(KaxTagMultiComment)) {
    mxprint(o, "          <MultiComment>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_multicomments((*(EbmlMaster *)e)[i], 6);
    mxprint(o, "          </MultiComment>\n");
  }

  else
    pr_unk();
}

static void handle_level4(EbmlElement *e) {
  int i, level = 4;

  if (is_id(KaxTagMultiCommercialType))
    pr_ui("CommercialType");

  else if (is_id(KaxTagMultiCommercialAddress))
    pr_us("Address");

  else if (is_id(KaxTagMultiCommercialURL))
    pr_s("URL");

  else if (is_id(KaxTagMultiCommercialEmail))
    pr_s("Email");

  else if (is_id(KaxTagMultiPrice)) {
    mxprint(o, "        <MultiPrice>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level5((*(EbmlMaster *)e)[i]);
    mxprint(o, "        </MultiPrice>\n");

  } else if (is_id(KaxTagMultiDateType))
    pr_ui("DateType");

  else if (is_id(KaxTagMultiDateDateBegin))
    pr_d("Begin");

  else if (is_id(KaxTagMultiDateDateEnd))
    pr_d("End");

  else if (is_id(KaxTagMultiEntityType))
    pr_ui("EntityType");

  else if (is_id(KaxTagMultiEntityName))
    pr_us("Name");

  else if (is_id(KaxTagMultiEntityURL))
    pr_s("URL");

  else if (is_id(KaxTagMultiEntityEmail))
    pr_s("Email");
  
  else if (is_id(KaxTagMultiEntityAddress))
    pr_us("Address");

  else if (is_id(KaxTagMultiIdentifierType))
    pr_ui("IdentifierType");

  else if (is_id(KaxTagMultiIdentifierBinary))
    pr_b("Binary");

  else if (is_id(KaxTagMultiIdentifierString))
    pr_us("String");

  else if (is_id(KaxTagMultiLegalType))
    pr_ui("LegalType");

  else if (is_id(KaxTagMultiLegalURL))
    pr_s("URL");

  else if (is_id(KaxTagMultiLegalAddress))
    pr_us("Address");
 
  else if (is_id(KaxTagMultiLegalContent))
    pr_us("Content");

  else if (is_id(KaxTagMultiTitleType))
    pr_ui("TitleType");

  else if (is_id(KaxTagMultiTitleName))
    pr_us("Name");

  else if (is_id(KaxTagMultiTitleSubTitle))
    pr_us("SubTitle");

  else if (is_id(KaxTagMultiTitleEdition))
    pr_us("Edition");

  else if (is_id(KaxTagMultiTitleAddress))
    pr_us("Address");

  else if (is_id(KaxTagMultiTitleURL))
    pr_s("URL");

  else if (is_id(KaxTagMultiTitleEmail))
    pr_s("Email");

  else if (is_id(KaxTagMultiTitleLanguage))
    pr_s("Language");

  else if (is_id(KaxTagMultiComment)) {
    mxprint(o, "        <MultiComment>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_multicomments((*(EbmlMaster *)e)[i], 5);
    mxprint(o, "        </MultiComment>\n");
  }

  else
    pr_unk();
}

static void handle_level3(EbmlElement *e) {
  int i, level = 3;

  if (is_id(KaxTagTrackUID))
    pr_ui("TrackUID");

  else if (is_id(KaxTagChapterUID))
    pr_ui("ChapterUID");

  else if (is_id(KaxTagSubject))
    pr_us("Subject");

  else if (is_id(KaxTagBibliography))
    pr_us("Bibliography");

  else if (is_id(KaxTagLanguage))
    pr_s("Language");

  else if (is_id(KaxTagRating))
    pr_b("Rating");

  else if (is_id(KaxTagEncoder))
    pr_us("Encoder");

  else if (is_id(KaxTagEncodeSettings))
    pr_us("EncodeSettings");

  else if (is_id(KaxTagFile))
    pr_us("File");

  else if (is_id(KaxTagArchivalLocation))
    pr_us("ArchivalLocation");

  else if (is_id(KaxTagKeywords))
    pr_us("Keywords");

  else if (is_id(KaxTagMood))
    pr_us("Mood");

  else if (is_id(KaxTagRecordLocation))
    pr_s("RecordLocation");

  else if (is_id(KaxTagSource))
    pr_us("Source");

  else if (is_id(KaxTagSourceForm))
    pr_us("SourceForm");

  else if (is_id(KaxTagProduct))
    pr_us("Product");

  else if (is_id(KaxTagOriginalMediaType))
    pr_us("OriginalMediaType");

  else if (is_id(KaxTagPlayCounter))
    pr_ui("PlayCounter");

  else if (is_id(KaxTagPopularimeter))
    pr_ui("Popularimeter");

  else if (is_id(KaxTagAudioGenre))
    pr_s("AudioGenre");

  else if (is_id(KaxTagVideoGenre))
    pr_b("VideoGenre");

  else if (is_id(KaxTagSubGenre))
    pr_s("SubGenre");

  else if (is_id(KaxTagAudioEncryption))
    pr_b("AudioEncryption");

  else if (is_id(KaxTagAudioGain))
    pr_f("AudioGain");

  else if (is_id(KaxTagAudioPeak))
    pr_f("AudioPeak");

  else if (is_id(KaxTagBPM))
    pr_f("BPM");

  else if (is_id(KaxTagEqualisation))
    pr_b("Equalisation");

  else if (is_id(KaxTagDiscTrack))
    pr_ui("DiscTrack");

  else if (is_id(KaxTagSetPart))
    pr_ui("SetPart");

  else if (is_id(KaxTagInitialKey))
    pr_s("InitialKey");

  else if (is_id(KaxTagOfficialAudioFileURL))
    pr_s("OfficialAudioFileURL");

  else if (is_id(KaxTagOfficialAudioSourceURL))
    pr_s("OfficialAudioSourceURL");

  else if (is_id(KaxTagCaptureDPI))
    pr_ui("CaptureDPI");

  else if (is_id(KaxTagCaptureLightness))
    pr_b("CaptureLightness");

  else if (is_id(KaxTagCapturePaletteSetting))
    pr_ui("CapturePaletteSetting");

  else if (is_id(KaxTagCaptureSharpness))
    pr_b("CaptureSharpness");

  else if (is_id(KaxTagCropped))
    pr_us("Cropped");

  else if (is_id(KaxTagOriginalDimensions))
    pr_s("OriginalDimensions");

  else if (is_id(KaxTagCommercial)) {
    mxprint(o, "      <Commercial>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Commercial>\n");

  } else if (is_id(KaxTagDate)) {
    mxprint(o, "      <Date>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Date>\n");

  } else if (is_id(KaxTagEntity)) {
    mxprint(o, "      <Entity>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Entity>\n");

  } else if (is_id(KaxTagIdentifier)) {
    mxprint(o, "      <Identifier>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Identifier>\n");

  } else if (is_id(KaxTagLegal)) {
    mxprint(o, "      <Legal>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Legal>\n");

  } else if (is_id(KaxTagTitle)) {
    mxprint(o, "      <Title>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level4((*(EbmlMaster *)e)[i]);
    mxprint(o, "      </Title>\n");

  } else if (is_id(KaxTagMultiComment)) {
    mxprint(o, "      <MultiComment>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_multicomments((*(EbmlMaster *)e)[i], 4);
    mxprint(o, "      </MultiComment>\n");

  } else
    pr_unk();
}

static void handle_level2(EbmlElement *e) {
  int i, level = 2;

  if (is_id(KaxTagTargets)) {
    mxprint(o, "    <Targets>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </Targets>\n");

  } else if (is_id(KaxTagGeneral)) {
    mxprint(o, "    <General>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </General>\n");

  } else if (is_id(KaxTagGenres)) {
    mxprint(o, "    <Genres>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </Genres>\n");

  } else if (is_id(KaxTagAudioSpecific)) {
    mxprint(o, "    <AudioSpecific>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </AudioSpecific>\n");

  } else if (is_id(KaxTagImageSpecific)) {
    mxprint(o, "    <ImageSpecific>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </ImageSpecific>\n");

  } else if (is_id(KaxTagMultiCommercial)) {
    mxprint(o, "    <MultiCommercial>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiCommercial>\n");

  } else if (is_id(KaxTagMultiDate)) {
    mxprint(o, "    <MultiDate>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiDate>\n");

  } else if (is_id(KaxTagMultiEntity)) {
    mxprint(o, "    <MultiEntity>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiEntity>\n");

  } else if (is_id(KaxTagMultiIdentifier)) {
    mxprint(o, "    <MultiIdentifier>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiIdentifier>\n");

  } else if (is_id(KaxTagMultiLegal)) {
    mxprint(o, "    <MultiLegal>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiLegal>\n");

  } else if (is_id(KaxTagMultiTitle)) {
    mxprint(o, "    <MultiTitle>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level3((*(EbmlMaster *)e)[i]);
    mxprint(o, "    </MultiTitle>\n");

  } else if (is_id(KaxTagMultiComment)) {
    mxprint(o, "    <MultiComment>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_multicomments((*(EbmlMaster *)e)[i], 3);
    mxprint(o, "    </MultiComment>\n");

  } else if (is_id(KaxTagSimple)) {
    mxprint(o, "    <Simple>\n");
    for (i = 0; i < (int)((EbmlMaster *)e)->ListSize(); i++)
      handle_simpletags((*((EbmlMaster *)e))[i], level + 1);
    mxprint(o, "    </Simple>\n");

  } else
    pr_unk();
}

static void handle_level1(EbmlElement *e) {
  int i, level = 1;

  if (is_id(KaxTag)) {
    mxprint(o, "  <Tag>\n");
    for (i = 0; i < ((EbmlMaster *)e)->ListSize(); i++)
      handle_level2((*(EbmlMaster *)e)[i]);
    mxprint(o, "  </Tag>\n");
  } else
    pr_unk();

}

void write_tags_xml(KaxTags &tags, FILE *out) {
  int i;

  o = out;

//   dumpsizes(&tags, 0);

  for (i = 0; i < tags.ListSize(); i++)
    handle_level1(tags[i]);
}
