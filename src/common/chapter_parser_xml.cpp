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
 * XML chapter parser functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <expat.h>
#include <ctype.h>
#include <setjmp.h>
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
  vector<int> *parent_idxs;

  KaxChapters *chapters;

  jmp_buf parse_error_jmp;
  string *parse_error_msg;
} parser_data_t;

typedef void (*chapter_element_callback_t)(parser_data_t *pdata);

namespace libmatroska {
  extern EbmlId KaxChapters_TheId;
  extern EbmlId KaxEditionEntry_TheId;
  extern EbmlId KaxEditionUID_TheId;
  extern EbmlId KaxEditionFlagHidden_TheId;
  extern EbmlId KaxEditionFlagDefault_TheId;
  extern EbmlId KaxEditionManaged_TheId;
  extern EbmlId KaxChapterAtom_TheId;
  extern EbmlId KaxChapterUID_TheId;
  extern EbmlId KaxChapterTimeStart_TheId;
  extern EbmlId KaxChapterTimeEnd_TheId;
  extern EbmlId KaxChapterFlagHidden_TheId;
  extern EbmlId KaxChapterFlagEnabled_TheId;
  extern EbmlId KaxChapterPhysicalEquiv_TheId;
  extern EbmlId KaxChapterTrack_TheId;
  extern EbmlId KaxChapterTrackNumber_TheId;
  extern EbmlId KaxChapterDisplay_TheId;
  extern EbmlId KaxChapterString_TheId;
  extern EbmlId KaxChapterLanguage_TheId;
  extern EbmlId KaxChapterCountry_TheId;
  extern EbmlId KaxChapterProcess_TheId;
  extern EbmlId KaxChapterProcessTime_TheId;
}

enum ebml_type_t {ebmlt_master, ebmlt_int, ebmlt_uint, ebmlt_bool,
                  ebmlt_string, ebmlt_ustring, ebmlt_time};

#define NO_MIN_VALUE -9223372036854775807ll-1
#define NO_MAX_VALUE 9223372036854775807ll

typedef struct {
  const char *name;
  ebml_type_t type;
  int level;
  int64_t min_value;
  int64_t max_value;
  const EbmlId id;
  chapter_element_callback_t start_hook;
  chapter_element_callback_t end_hook;
} parser_element_t;

static void end_edition_entry(parser_data_t *pdata);
static void end_edition_uid(parser_data_t *pdata);
static void end_chapter_uid(parser_data_t *pdata);
static void end_chapter_atom(parser_data_t *pdata);
static void end_chapter_track(parser_data_t *pdata);
static void end_chapter_display(parser_data_t *pdata);
static void end_chapter_language(parser_data_t *pdata);
static void end_chapter_country(parser_data_t *pdata);

const parser_element_t chapter_elements[] = {
  {"Chapters", ebmlt_master, 0, 0, 0, KaxChapters_TheId, NULL, NULL},

  {"EditionEntry", ebmlt_master, 1, 0, 0, KaxEditionEntry_TheId, NULL,
   end_edition_entry},
  {"EditionUID", ebmlt_uint, 2, 0, NO_MAX_VALUE, KaxEditionUID_TheId, NULL,
   end_edition_uid},
  {"EditionFlagHidden", ebmlt_bool, 2, 0, 0, KaxEditionFlagHidden_TheId,
   NULL, NULL},
  {"EditionManaged", ebmlt_uint, 2, 0, NO_MAX_VALUE, KaxEditionManaged_TheId,
   NULL, NULL},
  {"EditionFlagDefault", ebmlt_bool, 2, 0, 0, KaxEditionFlagDefault_TheId,
   NULL, NULL},

  {"ChapterAtom", ebmlt_master, 2, 0, 0, KaxChapterAtom_TheId, NULL,
   end_chapter_atom},
  {"ChapterUID", ebmlt_uint, 3, 0, NO_MAX_VALUE, KaxChapterUID_TheId, NULL,
   end_chapter_uid},
  {"ChapterTimeStart", ebmlt_time, 3, 0, 0, KaxChapterTimeStart_TheId, NULL,
   NULL},
  {"ChapterTimeEnd", ebmlt_time, 3, 0, 0, KaxChapterTimeEnd_TheId, NULL,
   NULL},
  {"ChapterFlagHidden", ebmlt_bool, 3, 0, 0, KaxChapterFlagHidden_TheId,
   NULL, NULL},
  {"ChapterFlagEnabled", ebmlt_bool, 3, 0, 0, KaxChapterFlagEnabled_TheId,
   NULL, NULL},

  {"ChapterTrack", ebmlt_master, 3, 0, 0, KaxChapterTrack_TheId,
   NULL, end_chapter_track},
  {"ChapterTrackNumber", ebmlt_uint, 4, 0, NO_MAX_VALUE,
   KaxChapterTrackNumber_TheId, NULL, NULL},

  {"ChapterDisplay", ebmlt_master, 3, 0, 0, KaxChapterDisplay_TheId,
   NULL, end_chapter_display},
  {"ChapterString", ebmlt_ustring, 4, 0, 0, KaxChapterString_TheId,
   NULL, NULL},
  {"ChapterLanguage", ebmlt_string, 4, 0, 0, KaxChapterLanguage_TheId,
   NULL, end_chapter_language},
  {"ChapterCountry", ebmlt_string, 4, 0, 0, KaxChapterCountry_TheId,
   NULL, end_chapter_country},

  {NULL, ebmlt_master, 0, 0, 0, EbmlId((uint32_t)0, 0), NULL, NULL}
};

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
  *pdata->parse_error_msg = new_string;
  safefree(new_string);
  longjmp(pdata->parse_error_jmp, 1);
}

static void
el_get_uint(parser_data_t *pdata,
            EbmlElement *el,
            uint64_t min_value = 0,
            bool is_bool = false) {
  int64 value;

  strip(*pdata->bin);
  if (!parse_int(pdata->bin->c_str(), value))
    cperror(pdata, "Expected an unsigned integer but found '%s'.",
            pdata->bin->c_str());
  if (value < min_value)
    cperror(pdata, "Unsigned integer (%lld) is too small. Mininum value is "
            "%lld.", value, min_value);
  if (is_bool && (value > 0))
    value = 1;

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
start_element(void *user_data,
              const char *name,
              const char **atts) {
  parser_data_t *pdata;

  pdata = (parser_data_t *)user_data;

  if (atts[0] != NULL)
    cperror(pdata, "Attributes are not allowed.");

  if (pdata->data_allowed)
    cperror_unknown();

  pdata->data_allowed = false;

  if (pdata->bin != NULL)
    die("start_element: pdata->bin != NULL");

  if (pdata->depth == 0) {
    if (pdata->done_reading)
      cperror(pdata, "More than one root element found.");
    if (strcmp(name, "Chapters"))
      cperror(pdata, "Root element must be <Chapters>.");

    pdata->chapters = new KaxChapters;
    pdata->parents->push_back(pdata->chapters);
    pdata->parent_idxs->push_back(0);

  } else {
    EbmlElement *e;
    EbmlMaster *m;
    int elt_idx, parent_idx, i;
    bool found;

    parent_idx = (*pdata->parent_idxs)[pdata->parent_idxs->size() - 1];
    elt_idx = parent_idx;
    found = false;
    while (chapter_elements[elt_idx].name != NULL) {
      if (!strcmp(chapter_elements[elt_idx].name, name)) {
        found = true;
        break;
      }
      elt_idx++;
    }

    if (!found)
      cperror(pdata, "<%s> is not a valid child element of <%s>.", name,
              chapter_elements[parent_idx].name);

    const EbmlSemanticContext &context =
      find_ebml_callbacks(KaxChapters::ClassInfos,
                          chapter_elements[parent_idx].id).Context;
    found = false;
    for (i = 0; i < context.Size; i++)
      if (chapter_elements[elt_idx].id ==
          context.MyTable[i].GetCallbacks.GlobalId) {
        found = true;
        break;
      }

    if (!found)
      cperror(pdata, "<%s> is not a valid child element of <%s>.", name,
              chapter_elements[parent_idx].name);

    const EbmlSemantic &semantic =
      find_ebml_semantic(KaxChapters::ClassInfos,
                         chapter_elements[elt_idx].id);
    if (semantic.Unique) {
      m = dynamic_cast<EbmlMaster *>(parent_elt);
      assert(m != NULL);
      for (i = 0; i < m->ListSize(); i++)
        if ((*m)[i]->Generic().GlobalId == chapter_elements[elt_idx].id)
          cperror(pdata, "Only one instance of <%s> is allowed beneath <%s>.",
                  name, chapter_elements[parent_idx].name);
    }

    e = create_ebml_element(KaxChapters::ClassInfos,
                            chapter_elements[elt_idx].id);
    assert(e != NULL);
    m = dynamic_cast<EbmlMaster *>(parent_elt);
    assert(m != NULL);
    m->PushElement(*e);

    if (chapter_elements[elt_idx].start_hook != NULL)
      chapter_elements[elt_idx].start_hook(pdata);

    pdata->parents->push_back(e);
    pdata->parent_idxs->push_back(elt_idx);

    pdata->data_allowed = chapter_elements[elt_idx].type != ebmlt_master;
  }

  (pdata->depth)++;
}

static void
end_edition_entry(parser_data_t *pdata) {
  EbmlMaster *m;
  KaxEditionUID *euid;
  int i, num;

  m = static_cast<EbmlMaster *>(parent_elt);
  num = 0;
  euid = NULL;
  for (i = 0; i < m->ListSize(); i++) {
    if (is_id((*m)[i], KaxEditionUID))
      euid = dynamic_cast<KaxEditionUID *>((*m)[i]);
    else if (is_id((*m)[i], KaxChapterAtom))
      num++;
  }
  if (num == 0)
    cperror(pdata, "At least one <ChapterAtom> element is needed.");
  if (euid == NULL) {
    euid = new KaxEditionUID;
    *static_cast<EbmlUInteger *>(euid) =
      create_unique_uint32(UNIQUE_EDITION_IDS);
    m->PushElement(*euid);
  }
}

static void
end_edition_uid(parser_data_t *pdata) {
  KaxEditionUID *euid;

  euid = static_cast<KaxEditionUID *>(parent_elt);
  if (!is_unique_uint32(uint32(*euid), UNIQUE_EDITION_IDS)) {
    mxwarn("Chapter parser: The EditionUID %u is not unique and could "
           "not be reused. A new one will be created.\n", uint32(*euid));
    *static_cast<EbmlUInteger *>(euid) =
      create_unique_uint32(UNIQUE_EDITION_IDS);
  }
}

static void
end_chapter_uid(parser_data_t *pdata) {
  KaxChapterUID *cuid;

  cuid = static_cast<KaxChapterUID *>(parent_elt);
  if (!is_unique_uint32(uint32(*cuid), UNIQUE_CHAPTER_IDS)) {
    mxwarn("Chapter parser: The ChapterUID %u is not unique and could "
           "not be reused. A new one will be created.\n", uint32(*cuid));
    *static_cast<EbmlUInteger *>(cuid) =
      create_unique_uint32(UNIQUE_CHAPTER_IDS);
  }
}

static void
end_chapter_atom(parser_data_t *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(parent_elt);
  if (m->FindFirstElt(KaxChapterTimeStart::ClassInfos, false) == NULL)
    cperror(pdata, "<ChapterAtom> is missing the <ChapterTimeStart> child.");

  if (m->FindFirstElt(KaxChapterUID::ClassInfos, false) == NULL) {
    KaxChapterUID *cuid;

    cuid = new KaxChapterUID;
    *static_cast<EbmlUInteger *>(cuid) =
      create_unique_uint32(UNIQUE_CHAPTER_IDS);
    m->PushElement(*cuid);
  }
}

static void
end_chapter_track(parser_data_t *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(parent_elt);
  if (m->FindFirstElt(KaxChapterTrackNumber::ClassInfos, false) == NULL)
    cperror(pdata, "<ChapterTrack> is missing the <ChapterTrackNumber> "
            "child.");
}

static void
end_chapter_display(parser_data_t *pdata) {
  EbmlMaster *m;

  m = static_cast<EbmlMaster *>(parent_elt);
  if (m->FindFirstElt(KaxChapterString::ClassInfos, false) == NULL)
    cperror(pdata, "<ChapterDisplay> is missing the <ChapterString> "
            "child.");
  if (m->FindFirstElt(KaxChapterLanguage::ClassInfos, false) == NULL) {
    KaxChapterLanguage *cl;

    cl = new KaxChapterLanguage;
    *static_cast<EbmlString *>(cl) = "und";
    m->PushElement(*cl);
  }
}

static void
end_chapter_language(parser_data_t *pdata) {
  EbmlString *s;

  s = static_cast<EbmlString *>(parent_elt);
  if (!is_valid_iso639_2_code(string(*s).c_str()))
    cperror(pdata, "'%s' is not a valid ISO639-2 language code.",
            string(*s).c_str());
}

static void
end_chapter_country(parser_data_t *pdata) {
  EbmlString *s;

  s = static_cast<EbmlString *>(parent_elt);
  if (!is_valid_iso639_1_code(string(*s).c_str()))
    cperror(pdata, "'%s' is not a valid ISO639-1 country code.",
            string(*s).c_str());
}

static void
end_element(void *user_data,
            const char *name) {
  parser_data_t *pdata;
  EbmlMaster *m;

  pdata = (parser_data_t *)user_data;

  if (pdata->data_allowed && (pdata->bin == NULL))
    cperror(pdata, "Element <%s> does not contain any data.", name);

  if (pdata->depth == 1) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->ListSize() == 0)
      cperror(pdata, "At least one <EditionEntry> element is needed.");

  } else {
    int elt_idx;
    bool found;

    found = false;
    for (elt_idx = 0; chapter_elements[elt_idx].name != NULL; elt_idx++)
      if (!strcmp(chapter_elements[elt_idx].name, name)) {
        found = true;
        break;
      }
    assert(found);

    switch (chapter_elements[elt_idx].type) {
      case ebmlt_master:
        break;
      case ebmlt_uint:
        el_get_uint(pdata, parent_elt, chapter_elements[elt_idx].min_value,
                    false);
        break;
      case ebmlt_bool:
        el_get_uint(pdata, parent_elt, 0, true);
        break;
      case ebmlt_string:
        el_get_string(pdata, parent_elt, false);
        break;
      case ebmlt_ustring:
        el_get_utf8string(pdata, parent_elt);
        break;
      case ebmlt_time:
        el_get_time(pdata, parent_elt);
        break;
      default:
        assert(0);
    }

    if (chapter_elements[elt_idx].end_hook != NULL)
      chapter_elements[elt_idx].end_hook(pdata);
  }


//   } else
//     end_this_level(pdata, name);

  if (pdata->bin != NULL) {
    delete pdata->bin;
    pdata->bin = NULL;
  }

  pdata->data_allowed = false;
  pdata->depth--;
  pdata->parents->pop_back();
  pdata->parent_idxs->pop_back();
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
  pdata->parent_idxs = new vector<int>;
  pdata->parse_error_msg = new string;

  XML_SetUserData(parser, pdata);
  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, add_data);

  in->setFilePointer(0);

  error = "";

  try {
    if (setjmp(pdata->parse_error_jmp) == 1)
      throw error_c(*pdata->parse_error_msg);
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
        error += "Aborting.\n";
        throw error_c(error);
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
  delete pdata->parent_idxs;
  delete pdata->parse_error_msg;
  safefree(pdata);

  if ((chapters != NULL) && (verbose > 1))
    debug_dump_elements(chapters, 0);

  if (error.length() > 0)
    throw error_c(error);

  return chapters;
}

// }}}
