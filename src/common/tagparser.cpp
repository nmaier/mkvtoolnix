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
 * XML tag parser. Functions for the start tags + helper functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <expat.h>

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "mm_io.h"
#include "tagparser.h"
#include "xml_element_mapping.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace std;
using namespace libebml;
using namespace libmatroska;

typedef struct {
  XML_Parser parser;

  const char *file_name;

  int depth;
  bool done_reading, data_allowed;

  string *bin;

  vector<EbmlElement *> *parents;
  vector<int> *parent_idxs;

  KaxTags *tags;

  jmp_buf parse_error_jmp;
  string *parse_error_msg;
} parser_data_t;

#define parent_elt (*((parser_data_t *)pdata)->parents) \
                      [((parser_data_t *)pdata)->parents->size() - 1]
#define parent_name _parent_name(parent_elt)
#define CPDATA (parser_data_t *)pdata

static const char *
_parent_name(EbmlElement *e) {
  int i;

  for (i = 0; tag_elements[i].name != NULL; i++)
    if (tag_elements[i].id == e->Generic().GlobalId)
      return tag_elements[i].name;

  return "(none)";
}

static void
tperror(parser_data_t *pdata,
        const char *fmt,
        ...) {
  va_list ap;
  string new_fmt;
  char *new_string;
  int len;

  fix_format(fmt, new_fmt);
  len = get_arg_len("Error: Tag parser failed for '%s', line %d, "
                    "column %d: ", pdata->file_name,
                    XML_GetCurrentLineNumber(pdata->parser),
                    XML_GetCurrentColumnNumber(pdata->parser));
  va_start(ap, fmt);
  len += get_varg_len(new_fmt.c_str(), ap);
  new_string = (char *)safemalloc(len + 2);
  sprintf(new_string, "Error: Tag parser failed for '%s', line %d, "
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
    tperror(pdata, "Expected an unsigned integer but found '%s'.",
            pdata->bin->c_str());
  if (value < min_value)
    tperror(pdata, "Unsigned integer (%lld) is too small. Mininum value is "
            "%lld.", value, min_value);
  if (is_bool && (value > 0))
    value = 1;

  *(static_cast<EbmlUInteger *>(el)) = value;
}

static void
el_get_string(parser_data_t *pdata,
              EbmlElement *el) {
  strip(*pdata->bin);
  if (pdata->bin->length() == 0)
    tperror(pdata, "Expected a string but found only whitespaces.");

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
        tperror(pdata, "Data is not allowed inside <%s>.", parent_name);
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
    tperror(pdata, "Attributes are not allowed.");

  if (pdata->data_allowed)
    tperror(pdata, "<%s> is not a valid child element of <%s>.", name,
            parent_name);

  pdata->data_allowed = false;

  if (pdata->bin != NULL)
    die("start_element: pdata->bin != NULL");

  if (pdata->depth == 0) {
    if (pdata->done_reading)
      tperror(pdata, "More than one root element found.");
    if (strcmp(name, "Tags"))
      tperror(pdata, "Root element must be <Tags>.");

    pdata->parents->push_back(pdata->tags);
    pdata->parent_idxs->push_back(0);

  } else {
    EbmlElement *e;
    EbmlMaster *m;
    int elt_idx, parent_idx, i;
    bool found;

    parent_idx = (*pdata->parent_idxs)[pdata->parent_idxs->size() - 1];
    elt_idx = parent_idx;
    found = false;
    while (tag_elements[elt_idx].name != NULL) {
      if (!strcmp(tag_elements[elt_idx].name, name)) {
        found = true;
        break;
      }
      elt_idx++;
    }

    if (!found)
      tperror(pdata, "<%s> is not a valid child element of <%s>.", name,
              tag_elements[parent_idx].name);

    const EbmlSemanticContext &context =
      find_ebml_callbacks(KaxTags::ClassInfos,
                          tag_elements[parent_idx].id).Context;
    found = false;
    for (i = 0; i < context.Size; i++)
      if (tag_elements[elt_idx].id ==
          context.MyTable[i].GetCallbacks.GlobalId) {
        found = true;
        break;
      }

    if (!found)
      tperror(pdata, "<%s> is not a valid child element of <%s>.", name,
              tag_elements[parent_idx].name);

    const EbmlSemantic &semantic =
      find_ebml_semantic(KaxTags::ClassInfos,
                         tag_elements[elt_idx].id);
    if (semantic.Unique) {
      m = dynamic_cast<EbmlMaster *>(parent_elt);
      assert(m != NULL);
      for (i = 0; i < m->ListSize(); i++)
        if ((*m)[i]->Generic().GlobalId == tag_elements[elt_idx].id)
          tperror(pdata, "Only one instance of <%s> is allowed beneath <%s>.",
                  name, tag_elements[parent_idx].name);
    }

    e = create_ebml_element(KaxTags::ClassInfos,
                            tag_elements[elt_idx].id);
    assert(e != NULL);
    m = dynamic_cast<EbmlMaster *>(parent_elt);
    assert(m != NULL);
    m->PushElement(*e);

    if (tag_elements[elt_idx].start_hook != NULL)
      tag_elements[elt_idx].start_hook(pdata);

    pdata->parents->push_back(e);
    pdata->parent_idxs->push_back(elt_idx);

    pdata->data_allowed = tag_elements[elt_idx].type != ebmlt_master;
  }

  (pdata->depth)++;
}

static void
end_element(void *user_data,
            const char *name) {
  parser_data_t *pdata;
  EbmlMaster *m;

  pdata = (parser_data_t *)user_data;

  if (pdata->data_allowed && (pdata->bin == NULL))
    tperror(pdata, "Element <%s> does not contain any data.", name);

  if (pdata->depth == 1) {
    m = static_cast<EbmlMaster *>(parent_elt);
    if (m->ListSize() == 0)
      tperror(pdata, "At least one <EditionEntry> element is needed.");

  } else {
    int elt_idx;
    bool found;

    found = false;
    for (elt_idx = 0; tag_elements[elt_idx].name != NULL; elt_idx++)
      if (!strcmp(tag_elements[elt_idx].name, name)) {
        found = true;
        break;
      }
    assert(found);

    switch (tag_elements[elt_idx].type) {
      case ebmlt_master:
        break;
      case ebmlt_uint:
        el_get_uint(pdata, parent_elt, tag_elements[elt_idx].min_value,
                    false);
        break;
      case ebmlt_bool:
        el_get_uint(pdata, parent_elt, 0, true);
        break;
      case ebmlt_string:
        el_get_string(pdata, parent_elt);
        break;
      case ebmlt_ustring:
        el_get_utf8string(pdata, parent_elt);
        break;
      case ebmlt_binary:
        el_get_binary(pdata, parent_elt);
        break;
      default:
        assert(0);
    }

    if (tag_elements[elt_idx].end_hook != NULL)
      tag_elements[elt_idx].end_hook(pdata);
  }

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
end_simple_tag(void *pdata) {
  KaxTagSimple *simple;

  simple = dynamic_cast<KaxTagSimple *>(parent_elt);
  assert(simple != NULL);
  if ((FINDFIRST(simple, KaxTagString) != NULL) &&
      (FINDFIRST(simple, KaxTagBinary) != NULL))
    tperror(CPDATA, "Only one of <String> and <Binary> may be used beneath "
            "<Simple> but not both at the same time.");
}

void
parse_xml_tags(const char *name,
               KaxTags *tags) {
  char buffer[5000];
  int len, done, i;
  parser_data_t *pdata;
  mm_io_c *io;
  XML_Parser parser;
  XML_Error xerror;
  char *emsg;

  io = NULL;
  try {
    io = new mm_io_c(name, MODE_READ);
  } catch(...) {
    mxerror("Could not open '%s' for reading.\n", name);
  }

  done = 0;

  for (i = 0; tag_elements[i].name != NULL; i++) {
    tag_elements[i].start_hook = NULL;
    tag_elements[i].end_hook = NULL;
  }
  tag_elements[tag_element_map_index("Simple")].end_hook = end_simple_tag;

  parser = XML_ParserCreate(NULL);

  pdata = (parser_data_t *)safemalloc(sizeof(parser_data_t));
  memset(pdata, 0, sizeof(parser_data_t));

  pdata->parser = parser;
  pdata->file_name = name;
  pdata->parents = new vector<EbmlElement *>;
  pdata->parent_idxs = new vector<int>;
  pdata->parse_error_msg = new string;
  pdata->tags = tags;

  XML_SetUserData(parser, pdata);
  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, add_data);

  if (setjmp(pdata->parse_error_jmp) == 1)
    mxerror(pdata->parse_error_msg->c_str());
  do {
    len = io->read(buffer, 5000);
    if (len != 5000)
      done = 1;
    if (XML_Parse(parser, buffer, len, done) == 0) {
      xerror = XML_GetErrorCode(parser);
      if (xerror == XML_ERROR_INVALID_TOKEN)
        emsg = " Remember that special characters like &, <, > and \" "
          "must be escaped in the usual HTML way: &amp; for '&', "
          "&lt; for '<', &gt; for '>' and &quot; for '\"'.";
      else
        emsg = "";
      mxerror("XML parser error at  line %d of '%s': %s.%s Aborting.\n",
              XML_GetCurrentLineNumber(parser), name,
              XML_ErrorString(xerror), emsg);
    }

  } while (!done);

  delete io;
  XML_ParserFree(parser);
  delete pdata->parents;
  delete pdata->parent_idxs;
  safefree(pdata);
}

void
fix_mandatory_tag_elements(EbmlElement *e) {
  if (dynamic_cast<KaxTagSimple *>(e) != NULL) {
    KaxTagSimple &s = *static_cast<KaxTagSimple *>(e);
    GetChild<KaxTagLangue>(s);
    GetChild<KaxTagDefault>(s);

  } else if (dynamic_cast<KaxTagTargets *>(e) != NULL) {
    KaxTagTargets &t = *static_cast<KaxTagTargets *>(e);
    GetChild<KaxTagTargetTypeValue>(t);

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_tag_elements((*m)[i]);
  }
}
