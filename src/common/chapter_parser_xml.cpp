/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapter_parser_xml.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief XML chapter parser functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <expat.h>
#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "commonebml.h"
#include "error.h"
#include "iso639.h"
#include "mm_io.h"

using namespace std;
using namespace libmatroska;

typedef struct {
  XML_Parser parser;

  const char *file_name;

  int depth;
  bool done_reading, data_allowed;

  string *bin;

  vector<EbmlElement *> *parents;

  KaxChapters *chapters;
} parser_data_t;

// {{{ XML chapters

#define parent_elt (*pdata->parents)[pdata->parents->size() - 1]
#define parent_name parent_elt->Generic().DebugName
#define cperror_unknown() \
  cperror(pdata, "Unknown/unsupported element: %s", name)
#define cperror_nochild() \
  cperror(pdata, "<%s> is not a valid child element of <%s>.", name, \
          parent_name)
#define cperror_oneinstance() \
  cperror(pdata, "Only one instance of <%s> is allowed under <%s>.", name, \
          parent_name)

static void
cperror(parser_data_t *pdata,
        const char *fmt,
        ...) {
  va_list ap;
  string new_fmt;
  char *new_string;
  int len;

  fix_format(fmt, new_fmt);
  len = get_arg_len("Error: Chapter parser failed for '%s', line %d, "
                    "column %d: ", pdata->file_name,
                    XML_GetCurrentLineNumber(pdata->parser),
                    XML_GetCurrentColumnNumber(pdata->parser));
  va_start(ap, fmt);
  len += get_varg_len(new_fmt.c_str(), ap);
  new_string = (char *)safemalloc(len + 2);
  sprintf(new_string, "Error: Chapter parser failed for '%s', line %d, "
          "column %d: ", pdata->file_name,
          XML_GetCurrentLineNumber(pdata->parser),
          XML_GetCurrentColumnNumber(pdata->parser));
  vsprintf(&new_string[strlen(new_string)], new_fmt.c_str(), ap);
  va_end(ap);
  strcat(new_string, "\n");
  throw error_c(new_string, true);
}

static void
el_get_uint(parser_data_t *pdata,
            EbmlElement *el,
            uint64_t min_value = 0) {
  int64 value;

  strip(*pdata->bin);
  if (!parse_int(pdata->bin->c_str(), value))
    cperror(pdata, "Expected an unsigned integer but found '%s'.",
            pdata->bin->c_str());
  if (value < min_value)
    cperror(pdata, "Unsigned integer (%lld) is too small. Mininum value is "
            "%lld.", value, min_value);

  *(static_cast<EbmlUInteger *>(el)) = value;
}

static void
el_get_string(parser_data_t *pdata,
              EbmlElement *el,
              bool check_language = false) {
  strip(*pdata->bin);
  if (pdata->bin->length() == 0)
    cperror(pdata, "Expected a string but found only whitespaces.");

  if (check_language && !is_valid_iso639_2_code(pdata->bin->c_str()))
    cperror(pdata, "'%s' is not a valid ISO639-2 language code. See the "
            "output of 'mkvmerge --list-languages' for a list of all "
            "valid language codes.", pdata->bin->c_str());

  *(static_cast<EbmlString *>(el)) = pdata->bin->c_str();
}

static void
el_get_utf8string(parser_data_t *pdata,
                  EbmlElement *el) {
  strip(*pdata->bin);
  if (pdata->bin->length() == 0)
    cperror(pdata, "Expected a string but found only whitespaces.");

  *(static_cast<EbmlUnicodeString *>(el)) =
    cstrutf8_to_UTFstring(pdata->bin->c_str());
}

static void
el_get_time(parser_data_t *pdata,
            EbmlElement *el) {
  const char *errmsg = "Expected a time in the following format: HH:MM:SS.nnn"
    " (HH = hour, MM = minute, SS = second, nnn = millisecond up to "
    "nanosecond. You may use up to nine digits for 'n' which would mean "
    "nanosecond precision). Found '%s' instead. Additional error message: %s";
  int64_t usec;

  strip(*pdata->bin);
  if (!parse_timecode(pdata->bin->c_str(), &usec))
    cperror(pdata, errmsg, pdata->bin->c_str(), timecode_parser_error);

  *(static_cast<EbmlUInteger *>(el)) = usec;
}

static void
add_data(void *user_data,
         const XML_Char *s,
         int len) {
  parser_data_t *pdata;
  int i;

  pdata = (parser_data_t *)user_data;

  if (!pdata->data_allowed) {
    for (i = 0; i < len; i++)
      if (!isblanktab(s[i]) && !iscr(s[i]))
        cperror(pdata, "Data is not allowed inside <%s>.", parent_name);
    return;
  }

  if (pdata->bin == NULL)
    pdata->bin = new string;

  for (i = 0; i < len; i++)
    (*pdata->bin) += s[i];
}

static void
start_next_level(parser_data_t *pdata,
                 const char *name) {
  EbmlMaster *m;

  if (!strcmp(name, "ChapterAtom")) {
    KaxChapterAtom *catom;
    KaxChapterUID *cuid;

    if (strcmp(parent_name, "EditionEntry") &&
        strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    catom = &AddEmptyChild<KaxChapterAtom>(*m);
    pdata->parents->push_back(catom);

    cuid = &GetChild<KaxChapterUID>(*catom);
    *(static_cast<EbmlUInteger *>(cuid)) = create_unique_uint32();

  } else if (!strcmp(name, "ChapterUID")) {
    cperror(pdata, "ChapterUID values are generated automatically.");
//     KaxChapterUID *cuid;

//     if (strcmp(parent_name, "ChapterAtom"))
//       cperror_nochild();

//     m = static_cast<EbmlMaster *>(parent_elt);
//     if (m->FindFirstElt(KaxChapterAtom::ClassInfos, false) != NULL)
//       cperror_oneinstance();

//     cuid = new KaxChapterUID;
//     m->PushElement(*cuid);
//     pdata->parents->push_back(cuid);

  } else if (!strcmp(name, "ChapterTimeStart")) {
    KaxChapterTimeStart *cts;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTimeStart::ClassInfos, false) != NULL)
      cperror_oneinstance();

    cts = new KaxChapterTimeStart;
    m->PushElement(*cts);
    pdata->parents->push_back(cts);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterTimeEnd")) {
    KaxChapterTimeEnd *cte;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTimeEnd::ClassInfos, false) != NULL)
      cperror_oneinstance();

    cte = new KaxChapterTimeEnd;
    m->PushElement(*cte);
    pdata->parents->push_back(cte);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterFlagHidden")) {
    KaxChapterFlagHidden *cfh;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterFlagHidden::ClassInfos, false) != NULL)
      cperror_oneinstance();

    cfh = new KaxChapterFlagHidden;
    m->PushElement(*cfh);
    pdata->parents->push_back(cfh);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterFlagEnabled")) {
    KaxChapterFlagEnabled *cfe;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterFlagEnabled::ClassInfos, false) != NULL)
      cperror_oneinstance();

    cfe = new KaxChapterFlagEnabled;
    m->PushElement(*cfe);
    pdata->parents->push_back(cfe);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterTrack")) {
    KaxChapterTrack *ct;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTrack::ClassInfos, false) != NULL)
      cperror_oneinstance();

    ct = &GetEmptyChild<KaxChapterTrack>(*m);
    pdata->parents->push_back(ct);

  } else if (!strcmp(name, "ChapterDisplay")) {
    KaxChapterDisplay *cd;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cd = &AddEmptyChild<KaxChapterDisplay>(*m);
    pdata->parents->push_back(cd);

  } else if (!strcmp(name, "ChapterTrackNumber")) {
    KaxChapterTrackNumber *ctn;

    if (strcmp(parent_name, "ChapterTrack"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTrackNumber::ClassInfos, false) != NULL)
      cperror_oneinstance();

    ctn = new KaxChapterTrackNumber;
    m->PushElement(*ctn);
    pdata->parents->push_back(ctn);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterString")) {
    KaxChapterString *cs;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterString::ClassInfos, false) != NULL)
      cperror_oneinstance();

    cs = new KaxChapterString;
    m->PushElement(*cs);
    pdata->parents->push_back(cs);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterLanguage")) {
    KaxChapterLanguage *cl;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cl = new KaxChapterLanguage;
    m->PushElement(*cl);
    pdata->parents->push_back(cl);
    pdata->data_allowed = true;

  } else if (!strcmp(name, "ChapterCountry")) {
    KaxChapterCountry *cc;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cc = new KaxChapterCountry;
    m->PushElement(*cc);
    pdata->parents->push_back(cc);
    pdata->data_allowed = true;

  } else
    cperror_nochild();
}

static void
start_element(void *user_data,
              const char *name,
              const char **atts) {
  parser_data_t *pdata;
  KaxChapters *chapters;
  KaxEditionEntry *eentry;

  pdata = (parser_data_t *)user_data;

  if (atts[0] != NULL)
    cperror(pdata, "Attributes are not allowed.");

  if (pdata->data_allowed)
    cperror_unknown();

  pdata->data_allowed = false;

  if (pdata->bin != NULL)
    assert("start_element: pdata->bin != NULL");

  if (pdata->depth == 0) {
    if (pdata->done_reading)
      cperror(pdata, "More than one root element found.");
    if (strcmp(name, "Chapters"))
      cperror(pdata, "Root element must be <Chapters>.");

    pdata->chapters = new KaxChapters;
    pdata->parents->push_back(pdata->chapters);

  } else if (pdata->depth == 1) {
    if (strcmp(name, "EditionEntry"))
      cperror_nochild();

    chapters = static_cast<KaxChapters *>(parent_elt);
    eentry = new KaxEditionEntry;
    chapters->PushElement(*eentry);
    pdata->parents->push_back(eentry);

  } else
    start_next_level(pdata, name);

  (pdata->depth)++;
}

static void
end_this_level(parser_data_t *pdata,
               const char *name) {
  EbmlMaster *m;

  if (!strcmp(name, "ChapterAtom")) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTimeStart::ClassInfos, false) == NULL)
      cperror(pdata, "<ChapterAtom> is missing the <ChapterTimeStart> "
              "child.");

  } else if (!strcmp(name, "ChapterTimeStart"))
    el_get_time(pdata, parent_elt);

  else if (!strcmp(name, "ChapterTimeEnd"))
    el_get_time(pdata, parent_elt);

  else if (!strcmp(name, "ChapterFlagHidden"))
    el_get_uint(pdata, parent_elt);

  else if (!strcmp(name, "ChapterFlagEnabled"))
    el_get_uint(pdata, parent_elt);

  else if (!strcmp(name, "ChapterTrack")) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterTrackNumber::ClassInfos, false) == NULL)
      cperror(pdata, "<ChapterTrack> is missing the <ChapterTrackNumber> "
              "child.");

  } else if (!strcmp(name, "ChapterTrackNumber"))
    el_get_uint(pdata, parent_elt, 1);

  else if (!strcmp(name, "ChapterDisplay")) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->FindFirstElt(KaxChapterString::ClassInfos, false) == NULL)
      cperror(pdata, "<ChapterDisplay> is missing the <ChapterString> "
              "child.");
    if (m->FindFirstElt(KaxChapterLanguage::ClassInfos, false) == NULL)
      cperror(pdata, "<ChapterDisplay> is missing the <ChapterLanguage> "
              "child.");

  } else if (!strcmp(name, "ChapterString"))
    el_get_utf8string(pdata, parent_elt);

  else if (!strcmp(name, "ChapterLanguage"))
    el_get_string(pdata, parent_elt, true);

  else if (!strcmp(name, "ChapterCountry"))
    el_get_string(pdata, parent_elt);

  else
    die("chapter_parser_xml/end_this_level(): Unknown name '%s'.", name);
}

static void
end_element(void *user_data,
            const char *name) {
  parser_data_t *pdata;
  EbmlMaster *m;

  pdata = (parser_data_t *)user_data;

  if (pdata->data_allowed && (pdata->bin == NULL))
    cperror(pdata, "Element <%s> does not contain any data.", name);

  if (pdata->depth == 1)
    ;                           // Nothing to do here!
  else if (pdata->depth == 2) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->ListSize() == 0)
      cperror(pdata, "At least one <EditionEntry> element is needed.");

  } else if (pdata->depth == 3) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->ListSize() == 0)
      cperror(pdata, "At least one <ChapterAtom> element is needed.");

  } else
    end_this_level(pdata, name);

  if (pdata->bin != NULL) {
    delete pdata->bin;
    pdata->bin = NULL;
  }

  pdata->data_allowed = false;
  pdata->depth--;
  pdata->parents->pop_back();
}

static void
remove_entries(int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               EbmlMaster &m) {
  int i;
  bool remove;
  KaxChapterAtom *atom;
  KaxChapterTimeStart *cts;
  KaxChapterTimeEnd *cte;
  int64_t start_tc, end_tc;

  i = 0;
  while (i < m.ListSize()) {
    if (EbmlId(*m[i]) == KaxChapterAtom::ClassInfos.GlobalId) {
      atom = static_cast<KaxChapterAtom *>(m[i]);
      cts = static_cast<KaxChapterTimeStart *>
        (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));

      remove = false;

      start_tc = uint64(*static_cast<EbmlUInteger *>(cts));
      if (start_tc < min_tc)
        remove = true;
      else if ((max_tc >= 0) && (start_tc > max_tc))
        remove = true;

      if (remove) {
        m.Remove(i);
        delete atom;
      } else
        i++;

    } else
      i++;
  }

  for (i = 0; i < m.ListSize(); i++) {
    if (EbmlId(*m[i]) == KaxChapterAtom::ClassInfos.GlobalId) {
      atom = static_cast<KaxChapterAtom *>(m[i]);
      cts = static_cast<KaxChapterTimeStart *>
        (atom->FindFirstElt(KaxChapterTimeStart::ClassInfos, false));
      cte = static_cast<KaxChapterTimeEnd *>
        (atom->FindFirstElt(KaxChapterTimeEnd::ClassInfos, false));

      *static_cast<EbmlUInteger *>(cts) =
        uint64(*static_cast<EbmlUInteger *>(cts)) - offset;
      if (cte != NULL) {
        end_tc =  uint64(*static_cast<EbmlUInteger *>(cte));
        if ((max_tc >= 0) && (end_tc > max_tc))
          end_tc = max_tc;
        end_tc -= offset;
        *static_cast<EbmlUInteger *>(cte) = end_tc;
      }

      remove_entries(min_tc, max_tc, offset, *atom);
    }
  }    
}

bool
probe_xml_chapters(mm_text_io_c *in) {
  string s;

  in->setFilePointer(0);

  while (in->getline2(s)) {
    // I assume that if it looks like XML then it is a XML chapter file :)
    strip(s);
    if (!strncasecmp(s.c_str(), "<?xml", 5))
      return true;
    else if (s.length() > 0)
      return false;
  }

  return false;
}

KaxChapters *
select_chapters_in_timeframe(KaxChapters *chapters,
                             int64_t min_tc,
                             int64_t max_tc,
                             int64_t offset) {
  uint32_t i;
  KaxEditionEntry *eentry;

  for (i = 0; i < chapters->ListSize(); i++) {
    remove_entries(min_tc, max_tc, offset,
                   *static_cast<EbmlMaster *>((*chapters)[i]));
  }

  i = 0;
  while (i < chapters->ListSize()) {
    eentry = static_cast<KaxEditionEntry *>((*chapters)[i]);
    if (eentry->ListSize() == 0) {
      chapters->Remove(i);
      delete eentry;

    } else
      i++;
  }

  if (chapters->ListSize() == 0) {
    delete chapters;
    chapters = NULL;
  }

  return chapters;
}

KaxChapters *
parse_xml_chapters(mm_text_io_c *in,
                   int64_t min_tc,
                   int64_t max_tc,
                   int64_t offset,
                   bool exception_on_error = false) {
  bool done;
  parser_data_t *pdata;
  XML_Parser parser;
  XML_Error xerror;
  string buffer, error;
  KaxChapters *chapters;

  done = 0;

  parser = XML_ParserCreate(NULL);

  pdata = (parser_data_t *)safemalloc(sizeof(parser_data_t));
  memset(pdata, 0, sizeof(parser_data_t));
  pdata->parser = parser;
  pdata->file_name = in->get_file_name();
  pdata->parents = new vector<EbmlElement *>;

  XML_SetUserData(parser, pdata);
  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, add_data);

  in->setFilePointer(0);

  error = "";

  try {
    done = !in->getline2(buffer);
    while (!done) {
      buffer += "\n";
      if (XML_Parse(parser, buffer.c_str(), buffer.length(), done) == 0) {
        xerror = XML_GetErrorCode(parser);
        error = mxsprintf("XML parser error at line %d of '%s': %s. ",
                          XML_GetCurrentLineNumber(parser), pdata->file_name,
                          XML_ErrorString(xerror));
        if (xerror == XML_ERROR_INVALID_TOKEN)
          error += "Remember that special characters like &, <, > and \" "
            "must be escaped in the usual HTML way: &amp; for '&', "
            "&lt; for '<', &gt; for '>' and &quot; for '\"'. ";
        error += "Aborting.";
        throw error_c(error.c_str());
      }

      done = !in->getline2(buffer);
    }

    chapters = pdata->chapters;
    if (chapters != NULL) {
      chapters = select_chapters_in_timeframe(chapters, min_tc, max_tc,
                                              offset);
      if ((chapters != NULL) && !chapters->CheckMandatory())
        die("chapters->CheckMandatory() failed. Should not have happened!");
    }
  } catch (error_c e) {
    if (!exception_on_error)
      mxerror("%s", e.get_error());
    error = (const char *)e;
    chapters = NULL;
  }

  XML_ParserFree(parser);
  delete pdata->parents;
  safefree(pdata);

  if ((chapters != NULL) && (verbose > 1))
    debug_dump_elements(chapters, 0);

  if (error.length() > 0)
    throw error_c(error);

  return chapters;
}

// }}}
