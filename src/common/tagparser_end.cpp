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
 * XML tag parser. Functions for the end tags + value parsers
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <string>
#include <vector>

#include <expat.h>

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "iso639.h"
#include "mm_io.h"
#include "tagparser.h"

using namespace std;
using namespace libebml;
using namespace libmatroska;

static void
el_get_uint(parser_data_t *pdata,
            EbmlElement *el,
            uint64_t min_value = 0,
            uint64_t max_value = 9223372036854775807ULL) {
  int64 value;

  strip(*pdata->bin);
  if (!parse_int(pdata->bin->c_str(), value))
    tperror(pdata, "Expected an unsigned integer but found '%s'.",
            pdata->bin->c_str());
  if (value < min_value)
    tperror(pdata, "Unsigned integer (%lld) is too small. Mininum value is "
            "%lld.", value, min_value);
  if (value > max_value)
    tperror(pdata, "Unsigned integer (%lld) is too big. Maximum value is "
            "%lld.", value, max_value);

  *(static_cast<EbmlUInteger *>(el)) = value;
}

static void
el_get_sint(parser_data_t *pdata,
            EbmlElement *el,
            int64_t min_value = -9223372036854775807LL-1) {
  int64 value;

  strip(*pdata->bin);
  if (!parse_int(pdata->bin->c_str(), value))
    tperror(pdata, "Expected a signed integer but found '%s'.",
            pdata->bin->c_str());
  if (value < min_value)
    tperror(pdata, "Signed integer (%lld) is too small. Mininum value is "
            "%lld.\n", value, min_value);

  *(static_cast<EbmlSInteger *>(el)) = value;
}

static void
el_get_float(parser_data_t *pdata,
             EbmlElement *el,
             float min_value = (float)1.40129846432481707e-45) {
  char *endptr;
  float value;

  strip(*pdata->bin);
  value = (float)strtod(pdata->bin->c_str(), &endptr);

  if (((value == 0.0) && (endptr == pdata->bin->c_str())) ||
      (errno == ERANGE))
    tperror(pdata, "Expected a floating point number but found '%s'.",
            pdata->bin->c_str());

  *(static_cast<EbmlFloat *>(el)) = value;
}

static void
el_get_string(parser_data_t *pdata,
              EbmlElement *el,
              bool check_language = false) {
  strip(*pdata->bin);
  if (pdata->bin->length() == 0)
    tperror(pdata, "Expected a string but found only whitespaces.");

  if (check_language && !is_valid_iso639_2_code(pdata->bin->c_str()))
    tperror(pdata, "'%s' is not a valid ISO639-2 language code. See the "
            "output of 'mkvmerge --list-languages' for a list of all "
            "valid language codes.", pdata->bin->c_str());

  *(static_cast<EbmlString *>(el)) = pdata->bin->c_str();
}

static void
el_get_utf8string(parser_data_t *pdata,
                  EbmlElement *el) {
  strip(*pdata->bin);
  if (pdata->bin->length() == 0)
    tperror(pdata, "Expected a string but found only whitespaces.");

  *(static_cast<EbmlUnicodeString *>(el)) =
    cstrutf8_to_UTFstring(pdata->bin->c_str());
}

static void
el_get_binary(parser_data_t *pdata,
              EbmlElement *el) {
  int64_t result;
  binary *buffer;
  mm_io_c *io;

  result = 0;
  buffer = NULL;
  strip(*pdata->bin, true);
  if (pdata->bin->length() == 0)
    tperror(pdata, "Found neither Base64 encoded data nor '@file' to read "
            "binary data from.");

  if ((*pdata->bin)[0] == '@') {
    if (pdata->bin->length() == 1)
      tperror(pdata, "No filename found after the '@'.");
    try {
      io = new mm_io_c(&(pdata->bin->c_str())[1], MODE_READ);
      io->setFilePointer(0, seek_end);
      result = io->getFilePointer();
      io->setFilePointer(0, seek_beginning);
      if (result <= 0)
        tperror(pdata, "The file '%s' is empty.", &(pdata->bin->c_str())[1]);
      buffer = new binary[result];
      io->read(buffer, result);
      delete io;
    } catch(...) {
      tperror(pdata, "Could not open/read the file '%s'.",
              &(pdata->bin->c_str())[1]);
    }

  } else {
    buffer = new binary[pdata->bin->length() / 4 * 3 + 1];
    result = base64_decode(*pdata->bin, (unsigned char *)buffer);
    if (result < 0)
      tperror(pdata, "Could not decode the Base64 encoded data - it seems to "
              "be broken.");
  }

  (static_cast<EbmlBinary *>(el))->SetBuffer(buffer, result);
}

// ISO 8601 format: 2003-07-17T19:50:53+0200
//                  012345678901234567890123
//                            1         2
static void
el_get_date(parser_data_t *pdata,
            EbmlElement *el) {
  const char *errmsg = "Expected a date in ISO 8601 format but found '%s'. "
    "The ISO 8601 date format looks like this: YYYY-MM-DDTHH:MM:SS:-TZTZ, "
    "e.g. 2003-07-17T19:50:52+0200. The time zone (TZ) may also be negative.";
  char *p;
  int year, month, day, hour, minute, second, time_zone, offset;
  char tz_sign;
  struct tm t;
  time_t tme;
#ifdef SYS_UNIX
  struct timezone tz;
#else
  time_t tme_local, tme_gm;
#endif

  strip(*pdata->bin);
  p = safestrdup(pdata->bin->c_str());

  if (pdata->bin->length() != 24)
    tperror(pdata, errmsg, pdata->bin->c_str());

  if ((p[4] != '-') || (p[7] != '-') || (p[10] != 'T') ||
      (p[13] != ':') || (p[16] != ':') || (p[19] != '+'))
    tperror(pdata, errmsg, pdata->bin->c_str());

  p[4] = 0;
  p[7] = 0;
  p[10] = 0;
  p[13] = 0;
  p[16] = 0;
  tz_sign = p[19];
  p[19] = 0;

  if (!parse_int(p, year) || !parse_int(&p[5], month) ||
      !parse_int(&p[8], day) || !parse_int(&p[11], hour) ||
      !parse_int(&p[14], minute) || !parse_int(&p[17], second) ||
      !parse_int(&p[20], time_zone))
    tperror(pdata, errmsg, pdata->bin->c_str());

  if (year < 1900)
    tperror(pdata, "Invalid year given (%d).", year);
  if ((month < 1) || (month > 12))
    tperror(pdata, "Invalid month given (%d).", month);
  if ((day < 1) || (day > 31))
    tperror(pdata, "Invalid day given (%d).", day);
  if ((hour < 0) || (hour > 23))
    tperror(pdata, "Invalid hour given (%d).", hour);
  if ((minute < 0) || (minute > 59))
    tperror(pdata, "Invalid minute given (%d).", minute);
  if ((second < 0) || (second > 59))
    tperror(pdata, "Invalid second given (%d).", second);
  if ((tz_sign != '+') && (tz_sign != '-'))
    tperror(pdata, "Invalid time zone given (%c%s).", tz_sign, &p[20]);
  if ((time_zone < 0) || (time_zone > 1200))
    tperror(pdata, "Invalid time zone given (%c%s).", tz_sign, &p[20]);

  memset(&t, 0, sizeof(struct tm));
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;
  t.tm_isdst = -1;
  tme = mktime(&t);
  if (tme == (time_t)-1)
    tperror(pdata, "Invalid date specified (%s).", pdata->bin->c_str());
  offset = (((time_zone / 100) * 60) + (time_zone % 100)) * 60;
  if (tz_sign == '+')
    offset *= -1;
  tme += offset;
#ifdef SYS_UNIX
  gettimeofday(NULL, &tz);
  tme -= tz.tz_minuteswest * 60;
#else
  tme_local = mktime(localtime(&tme));
  tme_gm = mktime(gmtime(&tme));
  tme -= (tme_gm - tme_local);
#endif
  if (tme < 0)
    tperror(pdata, "Invalid date specified (%s).", pdata->bin->c_str());

  safefree(p);

  (static_cast<EbmlDate *>(el))->SetEpochDate(tme);
}

static bool
is_multicomment(parser_data_t *pdata,
                const char *name) {
  int parent;

  parent = (*pdata->parents)[pdata->parents->size() - 2];

  if (parent != E_MultiComment)
    return false;

  if (!strcmp(name, "Name"))
    el_get_string(pdata, &GetChild<KaxTagMultiCommentName>(*pdata->m_comment));
  else if (!strcmp(name, "Comments"))
    el_get_utf8string(pdata, &GetChild<KaxTagMultiCommentComments>
                      (*pdata->m_comment));
  else if (!strcmp(name, "Language"))
    el_get_string(pdata, &GetChild<KaxTagMultiCommentLanguage>
                  (*pdata->m_comment), true);

  return true;
}

static void
end_level1(parser_data_t *pdata,
           const char *) {
  // Can only be "Tag"
  pdata->targets = NULL;
  pdata->general = NULL;
  pdata->genres = NULL;
  pdata->audio_specific = NULL;
  pdata->image_specific = NULL;
  pdata->m_commercial = NULL;
  pdata->m_date = NULL;
  pdata->m_entity = NULL;
  pdata->m_identifier = NULL;
  pdata->m_legal = NULL;
  pdata->m_title = NULL;
  pdata->m_comment = NULL;
}

static void
end_level2(parser_data_t *pdata,
           const char *name) {
  if (!strcmp(name, "Targets")) {
    GetChild<KaxTagTargetTypeValue>(*pdata->targets);
    pdata->track_uid = NULL;
    pdata->chapter_uid = NULL;
    pdata->edition_uid = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "General")) {
    pdata->keywords = NULL;
    pdata->rec_location = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "Genres")) {
    pdata->audio_genre = NULL;
    pdata->video_genre = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "AudioSpecific")) {
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "ImageSpecific")) {
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiCommercial")) {
    pdata->commercial = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiDate")) {
    pdata->date = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiEntity")) {
    pdata->entity = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiIdentifier")) {
    pdata->identifier = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiLegal")) {
    pdata->legal = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiTitle")) {
    pdata->title = NULL;
    pdata->m_comment = NULL;

  } else if (!strcmp(name, "MultiComment")) {

  }
}

static void
end_level3(parser_data_t *pdata,
           const char *name) {
  string parent_name;
  int parent;

  if (is_multicomment(pdata, name))
    return;

  parent_name = (*pdata->parent_names)[pdata->parent_names->size() - 2];
  parent = (*pdata->parents)[pdata->parents->size() - 2];

  if (parent == E_Targets) {
    if (!strcmp(name, "TrackUID"))
      el_get_uint(pdata, pdata->track_uid);
    else if (!strcmp(name, "ChapterUID"))
      el_get_uint(pdata, pdata->chapter_uid);
    else if (!strcmp(name, "EditionUID"))
      el_get_uint(pdata, pdata->edition_uid);
    else if (!strcmp(name, "TargetType"))
      el_get_string(pdata, &GetChild<KaxTagTargetType>(*pdata->targets));

  } else if (parent == E_General) {
    if (!strcmp(name, "Subject"))
      el_get_utf8string(pdata, &GetChild<KaxTagSubject>(*pdata->general));
    else if (!strcmp(name, "Bibliography"))
      el_get_utf8string(pdata, &GetChild<KaxTagBibliography>(*pdata->general));
    else if (!strcmp(name, "Language"))
      el_get_string(pdata, &GetChild<KaxTagLanguage>(*pdata->general), true);
    else if (!strcmp(name, "Rating"))
      el_get_binary(pdata, &GetChild<KaxTagRating>(*pdata->general));
    else if (!strcmp(name, "Encoder"))
      el_get_utf8string(pdata, &GetChild<KaxTagEncoder>(*pdata->general));
    else if (!strcmp(name, "EncodeSettings"))
      el_get_utf8string(pdata,
                        &GetChild<KaxTagEncodeSettings>(*pdata->general));
    else if (!strcmp(name, "File"))
      el_get_utf8string(pdata, &GetChild<KaxTagFile>(*pdata->general));
    else if (!strcmp(name, "ArchivalLocation"))
      el_get_utf8string(pdata,
                        &GetChild<KaxTagArchivalLocation>(*pdata->general));
    else if (!strcmp(name, "Keywords"))
      el_get_utf8string(pdata, pdata->keywords);
    else if (!strcmp(name, "Mood"))
      el_get_utf8string(pdata, &GetChild<KaxTagMood>(*pdata->general));
    else if (!strcmp(name, "RecordLocation"))
      el_get_string(pdata, pdata->rec_location);
    else if (!strcmp(name, "Source"))
      el_get_utf8string(pdata, &GetChild<KaxTagSource>(*pdata->general));
    else if (!strcmp(name, "SourceForm"))
      el_get_utf8string(pdata, &GetChild<KaxTagSourceForm>(*pdata->general));
    else if (!strcmp(name, "Product"))
      el_get_utf8string(pdata, &GetChild<KaxTagProduct>(*pdata->general));
    else if (!strcmp(name, "OriginalMediaType"))
      el_get_utf8string(pdata,
                        &GetChild<KaxTagOriginalMediaType>(*pdata->general));
    else if (!strcmp(name, "PlayCounter"))
      el_get_uint(pdata, &GetChild<KaxTagPlayCounter>(*pdata->general));
    else if (!strcmp(name, "Popularimeter"))
      el_get_sint(pdata, &GetChild<KaxTagPopularimeter>(*pdata->general));

  } else if (parent == E_Genres) {
    if (!strcmp(name, "AudioGenre"))
      el_get_string(pdata, pdata->audio_genre);
    else if (!strcmp(name, "VideoGenre"))
      el_get_binary(pdata, pdata->video_genre);
    else if (!strcmp(name, "SubGenre"))
      el_get_string(pdata, &GetChild<KaxTagSubGenre>(*pdata->genres));

  } else if (parent == E_AudioSpecific) {
    if (!strcmp(name, "AudioEncryption"))
      el_get_binary(pdata,
                    &GetChild<KaxTagAudioEncryption>(*pdata->audio_specific));
    else if (!strcmp(name, "AudioGain"))
      el_get_float(pdata,
                   &GetChild<KaxTagAudioGain>(*pdata->audio_specific),
                   0.000000001);
    else if (!strcmp(name, "AudioPeak"))
      el_get_float(pdata, &GetChild<KaxTagAudioPeak>(*pdata->audio_specific));
    else if (!strcmp(name, "BPM"))
      el_get_float(pdata, &GetChild<KaxTagBPM>(*pdata->audio_specific));
    else if (!strcmp(name, "Equalisation"))
      el_get_binary(pdata,
                    &GetChild<KaxTagEqualisation>(*pdata->audio_specific));
    else if (!strcmp(name, "DiscTrack"))
      el_get_uint(pdata, &GetChild<KaxTagDiscTrack>(*pdata->audio_specific));
    else if (!strcmp(name, "SetPart"))
      el_get_uint(pdata, &GetChild<KaxTagSetPart>(*pdata->audio_specific));
    else if (!strcmp(name, "InitialKey"))
      el_get_string(pdata,
                    &GetChild<KaxTagInitialKey>(*pdata->audio_specific));
    else if (!strcmp(name, "OfficialAudioFileURL"))
      el_get_string(pdata, &GetChild<KaxTagOfficialAudioFileURL>
                    (*pdata->audio_specific));
    else if (!strcmp(name, "OfficialAudioSourceURL"))
      el_get_string(pdata, &GetChild<KaxTagOfficialAudioSourceURL>
                    (*pdata->audio_specific));

  } else if (parent == E_ImageSpecific) {
    if (!strcmp(name, "CaptureDPI"))
      el_get_uint(pdata, &GetChild<KaxTagCaptureDPI>(*pdata->image_specific));
    else if (!strcmp(name, "CaptureLightness"))
      el_get_binary(pdata, &GetChild<KaxTagCaptureLightness>
                    (*pdata->image_specific));
    else if (!strcmp(name, "CapturePaletteSetting"))
      el_get_uint(pdata, &GetChild<KaxTagCapturePaletteSetting>
                  (*pdata->image_specific));
    else if (!strcmp(name, "CaptureSharpness"))
      el_get_binary(pdata, &GetChild<KaxTagCaptureSharpness>
                    (*pdata->image_specific));
    else if (!strcmp(name, "Cropped"))
      el_get_utf8string(pdata, &GetChild<KaxTagCropped>
                        (*pdata->image_specific));
    else if (!strcmp(name, "OriginalDimensions"))
      el_get_string(pdata, &GetChild<KaxTagOriginalDimensions>
                    (*pdata->image_specific));

  } else if (parent == E_MultiCommercial) {
    pdata->c_url = NULL;
    pdata->c_email = NULL;
    pdata->m_price = NULL;
    pdata->m_comment = NULL;

  } else if (parent == E_MultiDate) {
    pdata->m_comment = NULL;

  } else if (parent == E_MultiEntity) {
    pdata->e_url = NULL;
    pdata->e_email = NULL;
    pdata->m_comment = NULL;

  } else if (parent == E_MultiIdentifier) {
    pdata->m_comment = NULL;

  } else if (parent == E_MultiLegal) {
    pdata->l_url = NULL;
    pdata->m_comment = NULL;

  } else if (parent == E_MultiTitle) {
    pdata->t_url = NULL;
    pdata->t_email = NULL;
    pdata->m_comment = NULL;

  } else
    die("Unknown parent: level 3, %d", parent);
}

static void
end_level4(parser_data_t *pdata,
           const char *name) {
  string parent_name;
  int parent;

  if (is_multicomment(pdata, name))
    return;

  parent_name = (*pdata->parent_names)[pdata->parent_names->size() - 2];
  parent = (*pdata->parents)[pdata->parents->size() - 2];

  if (parent == E_Commercial) {
    if (!strcmp(name, "CommercialType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiCommercialType>
                  (*pdata->commercial), 1);
    else if (!strcmp(name, "Address"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiCommercialAddress>
                        (*pdata->commercial));
    else if (!strcmp(name, "URL"))
      el_get_string(pdata, pdata->c_url);
    else if (!strcmp(name, "Email"))
      el_get_string(pdata, pdata->c_email);
    else if (!strcmp(name, "MultiPrice")) {
      pdata->m_comment = NULL;
    }

  } else if (parent == E_Date) {
    if (!strcmp(name, "DateType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiDateType>
                  (*pdata->date), 1);
    else if (!strcmp(name, "Begin"))
      el_get_date(pdata, &GetChild<KaxTagMultiDateDateBegin>
                  (*pdata->date));
    else if (!strcmp(name, "End"))
      el_get_date(pdata, &GetChild<KaxTagMultiDateDateEnd>
                  (*pdata->date));

  } else if (parent == E_Entity) {
    if (!strcmp(name, "EntityType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiEntityType>
                  (*pdata->entity), 1);
    else if (!strcmp(name, "Name"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiEntityName>
                        (*pdata->entity));
    else if (!strcmp(name, "URL"))
      el_get_string(pdata, pdata->e_url);
    else if (!strcmp(name, "Email"))
      el_get_string(pdata, pdata->e_email);
    else if (!strcmp(name, "Address"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiEntityAddress>
                        (*pdata->entity));

  } else if (parent == E_Identifier) {
    if (!strcmp(name, "IdentifierType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiIdentifierType>
                  (*pdata->identifier), 1);
    else if (!strcmp(name, "Binary"))
      el_get_binary(pdata, &GetChild<KaxTagMultiIdentifierBinary>
                    (*pdata->identifier));
    else if (!strcmp(name, "String"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiIdentifierString>
                        (*pdata->identifier));

  } else if (parent == E_Legal) {
    if (!strcmp(name, "LegalType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiLegalType>
                  (*pdata->legal), 1);
    else if (!strcmp(name, "URL"))
      el_get_string(pdata, pdata->l_url);
    else if (!strcmp(name, "Address"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiLegalAddress>
                        (*pdata->legal));
    else if (!strcmp(name, "Content"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiLegalContent>
                        (*pdata->legal));

  } else if (parent == E_Title) {
    if (!strcmp(name, "TitleType"))
      el_get_uint(pdata, &GetChild<KaxTagMultiTitleType>
                  (*pdata->title), 1);
    else if (!strcmp(name, "Name"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiTitleName>
                        (*pdata->title));
    else if (!strcmp(name, "SubTitle"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiTitleSubTitle>
                        (*pdata->title));
    else if (!strcmp(name, "Edition"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiTitleEdition>
                        (*pdata->title));
    else if (!strcmp(name, "Address"))
      el_get_utf8string(pdata, &GetChild<KaxTagMultiTitleAddress>
                        (*pdata->title));
    else if (!strcmp(name, "URL"))
      el_get_string(pdata, pdata->t_url);
    else if (!strcmp(name, "Email"))
      el_get_string(pdata, pdata->t_email);
    else if (!strcmp(name, "Language"))
      el_get_string(pdata, &GetChild<KaxTagMultiTitleLanguage>
                    (*pdata->title), true);

  } else
    die("Unknown parent: level 4, %d", parent);
}

static void
end_level5(parser_data_t *pdata,
           const char *name) {
  string parent_name;
  int parent;

  if (is_multicomment(pdata, name))
    return;

  parent_name = (*pdata->parent_names)[pdata->parent_names->size() - 2];
  parent = (*pdata->parents)[pdata->parents->size() - 2];

  if (parent == E_MultiPrice) {
    if (!strcmp(name, "Currency"))
      el_get_string(pdata, &GetChild<KaxTagMultiPriceCurrency>
                    (*pdata->m_price));
    else if (!strcmp(name, "Amount"))
      el_get_float(pdata, &GetChild<KaxTagMultiPriceAmount>
                   (*pdata->m_price));
    else if (!strcmp(name, "PriceDate"))
      el_get_date(pdata, &GetChild<KaxTagMultiPricePriceDate>
                  (*pdata->m_price));

  } else
    die("Unknown parent: level 4, %d", parent);
}

static void
end_level6(parser_data_t *pdata,
           const char *name) {
  if (is_multicomment(pdata, name))
    return;

  die("tagparser_end: Unknown element. This should not have happened.");
}

static void
end_simple(parser_data_t *pdata,
           const char *name) {
  
  if (!strcmp(name, "Simple")) {
    pdata->simple_tags->pop_back();
    if (pdata->simple_tags->size() == 0)
      pdata->parsing_simple = false;

  } else {
    KaxTagSimple *simple =
      (*pdata->simple_tags)[pdata->simple_tags->size() - 1];
    if (!strcmp(name, "Name"))
      el_get_utf8string(pdata, &GetChild<KaxTagName>(*simple));
    else if (!strcmp(name, "String"))
      el_get_utf8string(pdata, &GetChild<KaxTagString>(*simple));
    else if (!strcmp(name, "Binary"))
      el_get_binary(pdata, &GetChild<KaxTagBinary>(*simple));
    else if (!strcmp(name, "TagLanguage"))
      el_get_string(pdata, &GetChild<KaxTagLangue>(*simple), true);
    else if (!strcmp(name, "DefaultLanguage"))
      el_get_uint(pdata, &GetChild<KaxTagDefault>(*simple), 0, 1);
  }
}

void
end_xml_tag_element(void *user_data,
                    const char *name) {
  parser_data_t *pdata;

  pdata = (parser_data_t *)user_data;

  if (pdata->data_allowed && (pdata->bin == NULL))
    tperror(pdata, "Element <%s> does not contain any data.", name);

  if (pdata->parsing_simple)
    end_simple(pdata, name);
  else if (pdata->depth == 1)
    ;                           // Nothing to do here!
  else if (pdata->depth == 2)
    end_level1(pdata, name);
  else if (pdata->depth == 3)
    end_level2(pdata, name);
  else if (pdata->depth == 4)
    end_level3(pdata, name);
  else if (pdata->depth == 5)
    end_level4(pdata, name);
  else if (pdata->depth == 6)
    end_level5(pdata, name);
  else if (pdata->depth == 7)
    end_level6(pdata, name);
  else
    die("tagparser_end: depth > 7: %d", pdata->depth);

  if (pdata->bin != NULL) {
    delete pdata->bin;
    pdata->bin = NULL;
  }

  pdata->data_allowed = false;
  pdata->depth--;
  pdata->parent_names->pop_back();
  pdata->parents->pop_back();
}
