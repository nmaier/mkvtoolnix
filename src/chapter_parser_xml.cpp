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

#include "common.h"
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

template <typename Type>Type &GetEmptyChild(EbmlMaster &master);
template <typename Type>Type &GetNextEmptyChild(EbmlMaster &master,
                                                const Type &past_elt);

bool probe_xml_chapters(mm_text_io_c *in) {
  return true;                  // TODO: Implement some kind of checking...
}

static void cperror(parser_data_t *pdata, const char *fmt, ...) {
  va_list ap;
  string new_fmt;

  mxprint(stderr, "Chapter parsing error in '%s', line %d, column %d: ",
          pdata->file_name, XML_GetCurrentLineNumber(pdata->parser),
          XML_GetCurrentColumnNumber(pdata->parser));
  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  vfprintf(stderr, new_fmt.c_str(), ap);
  va_end(ap);
  mxprint(stderr, "\n");

#ifdef DEBUG
  int i;
  mxprint(stderr, "Parent types:\n");
  for (i = 0; i < pdata->parents->size(); i++)
    mxprint(stderr, "  %s\n", (*pdata->parents)[i]->Generic().DebugName);
#endif

  mxexit(2);
}

static void add_data(void *user_data, const XML_Char *s, int len) {
  parser_data_t *pdata;
  int i;

  pdata = (parser_data_t *)user_data;

  if (!pdata->data_allowed) {
    for (i = 0; i < len; i++)
      if (!isblank(s[i]) && (s[i] != '\r') && (s[i] != '\n'))
        cperror(pdata, "Data is not allowed inside <%s>.", parent_name);
    return;
  }

  if (pdata->bin == NULL)
    pdata->bin = new string;

  for (i = 0; i < len; i++)
    (*pdata->bin) += s[i];
}

static void start_next_level(parser_data_t *pdata, const char *name) {
  EbmlMaster *m;

  if (!strcmp(name, "ChapterAtom")) {
    KaxChapterAtom *catom;

    if (strcmp(parent_name, "EditionEntry") &&
        strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    catom = new KaxChapterAtom;
    m->PushElement(*catom);
    pdata->parents->push_back(catom);

  } else if (!strcmp(name, "ChapterUID")) {
    KaxChapterUID *cuid;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cuid = new KaxChapterUID;
    m->PushElement(*cuid);
    pdata->parents->push_back(cuid);

  } else if (!strcmp(name, "ChapterTimeStart")) {
    KaxChapterTimeStart *cts;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cts = new KaxChapterTimeStart;
    m->PushElement(*cts);
    pdata->parents->push_back(cts);

  } else if (!strcmp(name, "ChapterTimeEnd")) {
    KaxChapterTimeEnd *cte;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cte = new KaxChapterTimeEnd;
    m->PushElement(*cte);
    pdata->parents->push_back(cte);

  } else if (!strcmp(name, "ChapterTrack")) {
    KaxChapterTrack *ct;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    ct = new KaxChapterTrack;
    m->PushElement(*ct);
    pdata->parents->push_back(ct);

  } else if (!strcmp(name, "ChapterDisplay")) {
    KaxChapterDisplay *cd;

    if (strcmp(parent_name, "ChapterAtom"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cd = new KaxChapterDisplay;
    m->PushElement(*cd);
    pdata->parents->push_back(cd);

  } else if (!strcmp(name, "ChapterTrackNumber")) {
    KaxChapterTrackNumber *ctn;

    if (strcmp(parent_name, "ChapterTrack"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    ctn = new KaxChapterTrackNumber;
    m->PushElement(*ctn);
    pdata->parents->push_back(ctn);

  } else if (!strcmp(name, "ChapterString")) {
    KaxChapterString *cs;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cs = new KaxChapterString;
    m->PushElement(*cs);
    pdata->parents->push_back(cs);

  } else if (!strcmp(name, "ChapterLanguage")) {
    KaxChapterLanguage *cl;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cl = new KaxChapterLanguage;
    m->PushElement(*cl);
    pdata->parents->push_back(cl);

  } else if (!strcmp(name, "ChapterCountry")) {
    KaxChapterCountry *cc;

    if (strcmp(parent_name, "ChapterDisplay"))
      cperror_nochild();

    m = static_cast<EbmlMaster *>(parent_elt);
    cc = new KaxChapterCountry;
    m->PushElement(*cc);
    pdata->parents->push_back(cc);

  } else
    cperror_nochild();
}

static void start_element(void *user_data, const char *name,
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

static void end_this_level(parser_data_t *pdata, const char *name) {
}

static void end_element(void *user_data, const char *name) {
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

  } else if (pdata->depth == 4)
    end_this_level(pdata, name);

  if (pdata->bin != NULL) {
    delete pdata->bin;
    pdata->bin = NULL;
  }

  pdata->data_allowed = false;
  pdata->depth--;
  pdata->parents->pop_back();
}

KaxChapters *parse_xml_chapters(mm_text_io_c *in, int64_t min_tc,
                                int64_t max_tc, int64_t offset) {
  bool done;
  parser_data_t *pdata;
  XML_Parser parser;
  XML_Error xerror;
  char *emsg;
  string buffer;
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

  done = !in->getline2(buffer);
  while (!done) {
    if (XML_Parse(parser, buffer.c_str(), buffer.length(), done) == 0) {
      xerror = XML_GetErrorCode(parser);
      if (xerror == XML_ERROR_INVALID_TOKEN)
        emsg = " Remember that special characters like &, <, > and \" "
          "must be escaped in the usual HTML way: &amp; for '&', "
          "&lt; for '<', &gt; for '>' and &quot; for '\"'.";
      else
        emsg = "";
      mxerror("XML parser error at  line %d of '%s': %s.%s Aborting.\n",
              XML_GetCurrentLineNumber(parser), pdata->file_name,
              XML_ErrorString(xerror), emsg);
    }

    done = !in->getline2(buffer);
  } while (!done);

  chapters = pdata->chapters;

  XML_ParserFree(parser);
  delete pdata->parents;
  safefree(pdata);

  return chapters;
}

// }}}

