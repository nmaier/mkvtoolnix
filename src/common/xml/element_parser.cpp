/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   XML element parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cctype>

#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlBinary.h>

#include <matroska/KaxSegment.h>

#include "common/base64.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/xml/element_parser.h"

using namespace libebml;
using namespace libmatroska;

parser_data_t::parser_data_t()
  : mapping(NULL)
  , depth(0)
  , skip_depth(0)
  , done_reading(false)
  , data_allowed(false)
  , root_element(NULL)
{
  memset(&parser, 0, sizeof(parser));
}

const char *
xmlp_parent_name(parser_data_t *pdata,
                 EbmlElement *e) {
  int i;

  for (i = 0; NULL != pdata->mapping[i].name; i++)
    if (pdata->mapping[i].id == EbmlId(*e))
      return pdata->mapping[i].name;

  return Y("(none)");
}

void
xmlp_error(parser_data_t *pdata,
           const std::string &message) {
  std::string error_message =
    (boost::format(Y("Error: %1% parser failed for '%2%', line %3%, column %4%: %5%\n"))
     % pdata->parser_name % pdata->file_name % XML_GetCurrentLineNumber(pdata->parser) % XML_GetCurrentColumnNumber(pdata->parser) % message).str();

  throw error_c(error_message);
}

static void
el_get_uint(parser_data_t *pdata,
            EbmlElement *el,
            uint64_t min_value = 0,
            bool is_bool = false) {
  uint64_t value;

  strip(pdata->bin);
  if (!parse_uint(pdata->bin.c_str(), value))
    xmlp_error(pdata, boost::format(Y("Expected an unsigned integer but found '%1%'.")) % pdata->bin);

  if (value < min_value)
    xmlp_error(pdata, boost::format(Y("Unsigned integer (%1%) is too small. Mininum value is %2%.")) % value % min_value);

  if (is_bool && (0 < value))
    value = 1;

  *(static_cast<EbmlUInteger *>(el)) = value;
}

static void
el_get_string(parser_data_t *pdata,
              EbmlElement *el) {
  strip(pdata->bin);
  *(static_cast<EbmlString *>(el)) = pdata->bin.c_str();
}

static void
el_get_utf8string(parser_data_t *pdata,
                  EbmlElement *el) {
  strip(pdata->bin);
  *(static_cast<EbmlUnicodeString *>(el)) = cstrutf8_to_UTFstring(pdata->bin.c_str());
}

static void
el_get_time(parser_data_t *pdata,
            EbmlElement *el) {
  int64_t usec;

  strip(pdata->bin);
  if (!parse_timecode(pdata->bin, usec))
    xmlp_error(pdata,
               boost::format(Y("Expected a time in the following format: HH:MM:SS.nnn "
                               "(HH = hour, MM = minute, SS = second, nnn = millisecond up to nanosecond. "
                               "You may use up to nine digits for 'n' which would mean nanosecond precision). "
                               "You may omit the hour as well. Found '%1%' instead. Additional error message: %2%"))
               % pdata->bin.c_str() % timecode_parser_error.c_str());

  *(static_cast<EbmlUInteger *>(el)) = usec;
}

static void
el_get_binary(parser_data_t *pdata,
              EbmlElement *el,
              int min_length,
              int max_length) {
  int64_t length = 0;
  binary *buffer = NULL;

  strip(pdata->bin, true);

  if (pdata->bin.empty())
    xmlp_error(pdata, Y("Found no encoded data nor '@file' to read binary data from."));

  if (pdata->bin[0] == '@') {
    if (pdata->bin.length() == 1)
      xmlp_error(pdata, Y("No filename found after the '@'."));

    std::string file_name = pdata->bin.substr(1);
    try {
      mm_file_io_c io(file_name);
      length = io.get_size();
      if (0 >= length)
        xmlp_error(pdata, boost::format(Y("The file '%1%' is empty.")) % file_name);

      buffer = new binary[length];
      io.read(buffer, length);
    } catch(...) {
      xmlp_error(pdata, boost::format(Y("Could not open/read the file '%1%'.")) % file_name);
    }

  } else if (pdata->format.empty() || (ba::iequals(pdata->format, "base64"))) {
    buffer = new binary[pdata->bin.length() / 4 * 3 + 1];
    length = base64_decode(pdata->bin, static_cast<unsigned char *>(buffer));
    if (0 > length)
      xmlp_error(pdata, Y("Could not decode the Base64 encoded data - it seems to be malformed."));

  } else if (ba::iequals(pdata->format, "hex")) {
    const char *p = pdata->bin.c_str();
    length        = 0;

    while (*p != 0) {
      if ((0 != p[1]) && ('0' == *p) && (('x' == p[1]) || ('X' == p[1]))) {
        p += 2;
        continue;
      }

      if (isdigit(*p) || ((tolower(*p) >= 'a') && (tolower(*p) <= 'f')))
        ++length;

      else if (!isblanktab(*p) && !iscr(*p) && (*p != '-') && (*p != '{') && (*p != '}'))
        xmlp_error(pdata, boost::format(Y("Invalid hexadecimal data encountered: '%1%' is neither white space nor a hexadecimal number.")) % *p);

      ++p;
    }

    if (((length % 2) != 0) || (length == 0))
      xmlp_error(pdata, Y("Too few hexadecimal digits found. The number of digits must be > 0 and divisable by 2."));

    buffer     = new binary[length / 2];
    p          = pdata->bin.c_str();
    bool upper = true;
    length     = 0;
    while (0 != *p) {
      if ((0 != p[1]) && ('0' == *p) && (('x' == p[1]) || ('X' == p[1]))) {
        p += 2;
        continue;
      }

      if (isblanktab(*p) || iscr(*p) || (*p == '-') || (*p == '{') || (*p == '}')) {
        ++p;
        continue;
      }

      uint8_t value = isdigit(*p) ? *p - '0' : tolower(*p) - 'a' + 10;
      if (upper)
        buffer[length] = value << 4;
      else {
        buffer[length] |= value;
        ++length;
      }

      upper = !upper;
      ++p;
    }

  } else if (ba::iequals(pdata->format, "ascii")) {
    length = pdata->bin.length();
    buffer = new binary[length];
    memcpy(buffer, pdata->bin.c_str(), pdata->bin.length());

  } else
    xmlp_error(pdata, boost::format(Y("Invalid binary data format '%1%' specified. Supported are 'Base64', 'ASCII' and 'hex'.")) % pdata->format);

  if ((0 < min_length) && (min_length == max_length) && (length != min_length))
    xmlp_error(pdata, boost::format(Y("The binary data must be exactly %1% bytes long.")) % min_length);

  else if ((0 < min_length) && (length < min_length))
    xmlp_error(pdata, boost::format(Y("The binary data must be at least %1% bytes long.")) % min_length);

  else if ((0 < max_length) && (length > max_length))
    xmlp_error(pdata, boost::format(Y("The binary data must be at most %1% bytes long.")) % max_length);

  (static_cast<EbmlBinary *>(el))->SetBuffer(buffer, length);
}

static void
add_data(void *user_data,
         const XML_Char *s,
         int len) {
  parser_data_t *pdata = static_cast<parser_data_t *>(user_data);

  if (0 < pdata->skip_depth)
    return;

  int i;
  if (!pdata->data_allowed) {
    for (i = 0; i < len; i++)
      if (!isblanktab(s[i]) && !iscr(s[i]))
        xmlp_error(pdata, boost::format(Y("Data is not allowed inside <%1%>.")) % xmlp_pname);
    return;
  }

  for (i = 0; i < len; i++)
    pdata->bin += s[i];
}

static void end_element(void *user_data, const char *name);

static int
find_element_index(parser_data_t *pdata,
                   const char *name,
                   int parent_idx) {
  int elt_idx = parent_idx;
  while (NULL != pdata->mapping[elt_idx].name) {
    if (!strcmp(pdata->mapping[elt_idx].name, name))
      return elt_idx;
    elt_idx++;
  }

  return -1;
}

static void
add_new_element(parser_data_t *pdata,
                const char *name,
                int parent_idx) {
  int elt_idx = find_element_index(pdata, name, parent_idx);
  if (-1 == elt_idx)
    xmlp_error(pdata, boost::format(Y("<%1%> is not a valid child element of <%2%>.")) % name % pdata->mapping[parent_idx].name);

  if (0 < pdata->depth) {
    const EbmlCallbacks *callbacks = find_ebml_callbacks(EBML_INFO(KaxSegment), pdata->mapping[parent_idx].id);
    bool found                     = false;

    if (NULL != callbacks) {
      const EbmlSemanticContext &context = EBML_INFO_CONTEXT(*callbacks);
      size_t i;
      for (i = 0; i < EBML_CTX_SIZE(context); i++)
        if (pdata->mapping[elt_idx].id == EBML_CTX_IDX_ID(context,i)) {
          found = true;
          break;
        }
    }

    if (!found)
      xmlp_error(pdata, boost::format(Y("<%1%> is not a valid child element of <%2%>.")) % name % pdata->mapping[parent_idx].name);

    const EbmlSemantic *semantic = find_ebml_semantic(EBML_INFO(KaxSegment), pdata->mapping[elt_idx].id);
    if ((NULL != semantic) && EBML_SEM_UNIQUE(*semantic)) {
      EbmlMaster *m = dynamic_cast<EbmlMaster *>(xmlp_pelt);
      assert(NULL != m);

      size_t i;
      for (i = 0; i < m->ListSize(); i++)
        if (EbmlId(*(*m)[i]) == pdata->mapping[elt_idx].id)
          xmlp_error(pdata, boost::format(Y("Only one instance of <%1%> is allowed beneath <%2%>.")) % name % pdata->mapping[parent_idx].name);
    }
  }

  EbmlElement *e = create_ebml_element(EBML_INFO(KaxSegment), pdata->mapping[elt_idx].id);
  assert(NULL != e);

  if (0 == pdata->depth) {
    EbmlMaster *m = dynamic_cast<EbmlMaster *>(e);
    assert(NULL != m);
    pdata->root_element = m;

  } else {
    EbmlMaster *m = dynamic_cast<EbmlMaster *>(xmlp_pelt);
    assert(NULL != m);
    m->PushElement(*e);
  }

  pdata->parents.push_back(e);
  pdata->parent_idxs.push_back(elt_idx);

  if (NULL != pdata->mapping[elt_idx].start_hook)
    pdata->mapping[elt_idx].start_hook(pdata);

  pdata->data_allowed = pdata->mapping[elt_idx].type != EBMLT_MASTER;

  (pdata->depth)++;
}

static void
start_element(void *user_data,
              const char *name,
              const char **atts) {
  parser_data_t *pdata = static_cast<parser_data_t *>(user_data);

  int parent_idx;
  if (0 == pdata->depth) {
    if (pdata->done_reading)
      xmlp_error(pdata, Y("More than one root element found."));
    if (strcmp(name, pdata->mapping[0].name))
      xmlp_error(pdata, boost::format(Y("The root element must be <%1%>.")) % pdata->mapping[0].name);
    parent_idx = 0;

  } else
    parent_idx = pdata->parent_idxs.back();

  int elt_idx = find_element_index(pdata, name, parent_idx);
  if ((0 < pdata->skip_depth) || ((-1 != elt_idx) && (EBMLT_SKIP == pdata->mapping[elt_idx].type))) {
    pdata->skip_depth++;
    return;
  }

  if (pdata->data_allowed)
    xmlp_error(pdata, boost::format(Y("<%1%> is not a valid child element of <%2%>.")) % name % xmlp_pname);

  pdata->data_allowed = false;
  pdata->format.clear();

  add_new_element(pdata, name, parent_idx);

  parent_idx = pdata->parent_idxs.back();

  int i;
  for (i = 0; (atts[i] != NULL) && (atts[i + 1] != NULL); i += 2) {
    if (!strcasecmp(atts[i], "format"))
      pdata->format = atts[i + 1];
    else {
      pdata->bin = std::string(atts[i + 1]);
      add_new_element(pdata, atts[i], parent_idx);
      end_element(pdata, atts[i]);
    }
  }
}

static void
end_element(void *user_data,
            const char *name) {
  parser_data_t *pdata = static_cast<parser_data_t *>(user_data);

  if (0 < pdata->skip_depth) {
    pdata->skip_depth--;
    return;
  }

  if (1 == pdata->depth) {
    EbmlMaster *m = static_cast<EbmlMaster *>(xmlp_pelt);
    if (m->ListSize() == 0)
      xmlp_error(pdata, Y("At least one <EditionEntry> element is needed."));

  } else {
    int elt_idx;
    bool found = false;
    for (elt_idx = 0; NULL != pdata->mapping[elt_idx].name; elt_idx++)
      if (!strcmp(pdata->mapping[elt_idx].name, name)) {
        found = true;
        break;
      }
    assert(found);

    switch (pdata->mapping[elt_idx].type) {
      case EBMLT_MASTER:
        break;
      case EBMLT_UINT:
        el_get_uint(pdata, xmlp_pelt, pdata->mapping[elt_idx].min_value, false);
        break;
      case EBMLT_BOOL:
        el_get_uint(pdata, xmlp_pelt, 0, true);
        break;
      case EBMLT_STRING:
        el_get_string(pdata, xmlp_pelt);
        break;
      case EBMLT_USTRING:
        el_get_utf8string(pdata, xmlp_pelt);
        break;
      case EBMLT_TIME:
        el_get_time(pdata, xmlp_pelt);
        break;
      case EBMLT_BINARY:
        el_get_binary(pdata, xmlp_pelt, pdata->mapping[elt_idx].min_value, pdata->mapping[elt_idx].max_value);
        break;
      default:
        assert(0);
    }

    if (NULL != pdata->mapping[elt_idx].end_hook)
      pdata->mapping[elt_idx].end_hook(pdata);
  }

  pdata->bin.clear();
  pdata->data_allowed = false;
  pdata->depth--;
  pdata->parents.pop_back();
  pdata->parent_idxs.pop_back();
}

EbmlMaster *
parse_xml_elements(const char *parser_name,
                   const parser_element_t *mapping,
                   mm_text_io_c *in) {
  XML_Parser parser    = XML_ParserCreate(NULL);

  parser_data_cptr pdata(new parser_data_t);
  pdata->parser          = parser;
  pdata->file_name       = in->get_file_name();
  pdata->parser_name     = parser_name;
  pdata->mapping         = mapping;

  XML_SetUserData(parser, pdata.get_object());
  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, add_data);

  in->setFilePointer(0);

  std::string error;

  try {
    std::string buffer;
    bool done = !in->getline2(buffer);
    while (!done) {
      buffer += "\n";
      if (XML_Parse(parser, buffer.c_str(), buffer.length(), done) == 0) {
        XML_Error xerror    = XML_GetErrorCode(parser);
        std::string message = (boost::format(Y("XML parser error at line %1% of '%2%': %3%.%4%%5%"))
                               % XML_GetCurrentLineNumber(parser) % pdata->file_name % XML_ErrorString(xerror)
                               % ((xerror == XML_ERROR_INVALID_TOKEN)
                                  ? Y(" Remember that special characters like &, <, > and \" "
                                      "must be escaped in the usual HTML way: &amp; for '&', "
                                      "&lt; for '<', &gt; for '>' and &quot; for '\"'.")
                                  : "")
                               % Y(" Aborting.\n")
                               ).str();
        throw error_c(message);
      }

      done = !in->getline2(buffer);
    }

  } catch (error_c e) {
    error = e.get_error();
  }

  EbmlMaster *root_element = pdata->root_element;
  XML_ParserFree(parser);

  if (!error.empty()) {
    if (NULL != root_element)
      delete root_element;
    throw error_c(error);
  }

  return root_element;
}

// -------------------------------------------------------------------

static void
xml_parser_start_element_cb(void *user_data,
                            const char *name,
                            const char **atts) {
  static_cast<xml_parser_c *>(user_data)->start_element_cb(name, atts);
}

static void
xml_parser_end_element_cb(void *user_data,
                          const char *name) {
  static_cast<xml_parser_c *>(user_data)->end_element_cb(name);
}

static void
xml_parser_add_data_cb(void *user_data,
                       const XML_Char *s,
                       int len) {
  static_cast<xml_parser_c *>(user_data)->add_data_cb(s, len);
}

xml_parser_c::xml_parser_c(mm_text_io_c *xml_source)
  : m_xml_parser_state(XMLP_STATE_INITIAL)
  , m_xml_source(xml_source)
  , m_xml_parser(NULL)
{
}

xml_parser_c::xml_parser_c()
  : m_xml_parser_state(XMLP_STATE_INITIAL)
  , m_xml_parser(NULL)
{
}

void
xml_parser_c::setup_xml_parser() {
  if (NULL != m_xml_parser)
    XML_ParserFree(m_xml_parser);

  m_xml_parser = XML_ParserCreate(NULL);
  XML_SetUserData(m_xml_parser, this);
  XML_SetElementHandler(m_xml_parser, xml_parser_start_element_cb, xml_parser_end_element_cb);
  XML_SetCharacterDataHandler(m_xml_parser, xml_parser_add_data_cb);
}

xml_parser_c::~xml_parser_c() {
  if (NULL != m_xml_parser)
    XML_ParserFree(m_xml_parser);
}

void
xml_parser_c::parse_xml_file() {
  m_xml_source->setFilePointer(0);

  while (parse_one_xml_line())
    ;
}

bool
xml_parser_c::parse_one_xml_line() {
  std::string line;

  if (NULL == m_xml_parser)
    setup_xml_parser();

  if (!m_xml_source->getline2(line))
    return false;

  handle_xml_encoding(line);

  line += "\n";
  if (XML_Parse(m_xml_parser, line.c_str(), line.length(), false) == 0) {
    XML_Error xerror  = XML_GetErrorCode(m_xml_parser);
    std::string error = XML_ErrorString(xerror);

    if (XML_ERROR_INVALID_TOKEN == xerror)
      error += Y("Remember that special characters like &, <, > and \" must be escaped in the usual HTML way: &amp; for '&', &lt; for '<', &gt; for '>' and &quot; for '\"'.");
    throw xml_parser_error_c(error, m_xml_parser);
  }

  return true;
}

void
xml_parser_c::handle_xml_encoding(std::string &line) {
  if ((XMLP_STATE_AFTER_HEADER == m_xml_parser_state) || (BO_NONE == m_xml_source->get_byte_order()))
    return;

  size_t pos = 0;
  std::string new_line;

  if (XMLP_STATE_INITIAL == m_xml_parser_state) {
    pos = line.find("<?xml");
    if (std::string::npos == pos)
      return;
    m_xml_parser_state = XMLP_STATE_ATTRIBUTE_NAME;
    pos += 5;
    new_line = line.substr(0, pos);
  }

  while ((line.length() > pos) &&
         (XMLP_STATE_AFTER_HEADER != m_xml_parser_state)) {
    char cur_char = line[pos];
    ++pos;

    if (XMLP_STATE_ATTRIBUTE_NAME == m_xml_parser_state) {
      if (('?' == cur_char) && (line.length() > pos) && ('>' == line[pos])) {
        new_line += "?>" + line.substr(pos + 1, line.length() - pos - 1);
        m_xml_parser_state = XMLP_STATE_AFTER_HEADER;

      } else if ('"' == cur_char)
        m_xml_parser_state = XMLP_STATE_ATTRIBUTE_VALUE;

      else if ((' ' != cur_char) && ('=' != cur_char))
        m_xml_attribute_name += cur_char;

    } else {
      // XMLP_STATE_ATTRIBUTE_VALUE
      if ('"' == cur_char) {
        m_xml_parser_state = XMLP_STATE_ATTRIBUTE_NAME;
        strip(m_xml_attribute_name);
        strip(m_xml_attribute_value);

        if (m_xml_attribute_name == "encoding") {
          ba::to_lower(m_xml_attribute_value);
          if ((m_xml_source->get_byte_order() == BO_NONE) && ((m_xml_attribute_value == "utf-8") || (m_xml_attribute_value == "utf8")))
            m_xml_source->set_byte_order(BO_UTF8);

          else if (starts_with_case(m_xml_attribute_value, "utf"))
            m_xml_attribute_value = "UTF-8";
        }

        new_line              += " " + m_xml_attribute_name + "=\"" + m_xml_attribute_value + "\"";
        m_xml_attribute_name   = "";
        m_xml_attribute_value  = "";

      } else
        m_xml_attribute_value += cur_char;
    }
  }

  line = new_line;
}

