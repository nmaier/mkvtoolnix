/** \brief chapter parser and helper functions

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <cassert>

#include <matroska/KaxChapters.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/error.h"
#include "common/locale.h"
#include "common/mm_io.h"
#include "common/mm_io_x.h"
#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "common/unique_numbers.h"
#include "common/xml/ebml_chapters_converter.h"

using namespace libmatroska;

/** The default language for all chapter entries that don't have their own. */
std::string g_default_chapter_language;
/** The default country for all chapter entries that don't have their own. */
std::string g_default_chapter_country;

#define SIMCHAP_RE_TIMECODE_LINE "^\\s*CHAPTER\\d+\\s*=\\s*(\\d+)\\s*:\\s*(\\d+)\\s*:\\s*(\\d+)\\s*[\\.,]\\s*(\\d+)"
#define SIMCHAP_RE_TIMECODE      "^\\s*CHAPTER\\d+\\s*=(.*)"
#define SIMCHAP_RE_NAME_LINE     "^\\s*CHAPTER\\d+NAME\\s*=(.*)"

/** \brief Throw a special chapter parser exception.

   \param error The error message.
*/
inline void
chapter_error(const std::string &error) {
  throw mtx::chapter_parser_x(boost::format(Y("Simple chapter parser: %1%\n")) % error);
}

inline void
chapter_error(const boost::format &format) {
  chapter_error(format.str());
}

/** \brief Reads the start of a file and checks for OGM style comments.

   The first lines are read. OGM style comments are recognized if the first
   non-empty line contains <tt>CHAPTER01=...</tt> and the first non-empty
   line afterwards contains <tt>CHAPTER01NAME=...</tt>.

   The parameters are checked for validity.

   \param in The file to read from.

   \return \c true if the file contains OGM style comments and \c false
     otherwise.
*/
bool
probe_simple_chapters(mm_text_io_c *in) {
  boost::regex timecode_line_re(SIMCHAP_RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex name_line_re(    SIMCHAP_RE_NAME_LINE,     boost::regex::perl);
  boost::smatch matches;

  std::string line;

  assert(in);

  in->setFilePointer(0);
  while (in->getline2(line)) {
    strip(line);
    if (line.empty())
      continue;

    if (!boost::regex_search(line, timecode_line_re))
      return false;

    while (in->getline2(line)) {
      strip(line);
      if (line.empty())
        continue;

      return boost::regex_search(line, name_line_re);
    }

    return false;
  }

  return false;
}

//           1         2
// 012345678901234567890123
//
// CHAPTER01=00:00:00.000
// CHAPTER01NAME=Hallo Welt

/** \brief Parse simple OGM style comments

   The file \a in is read. The content is assumed to be OGM style comments.

   The parameters are checked for validity.

   \param in The text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for all entries.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c nullptr will be returned.

   \return The chapters parsed from the file or \c nullptr if an error occured.
*/
kax_chapters_cptr
parse_simple_chapters(mm_text_io_c *in,
                      int64_t min_tc,
                      int64_t max_tc,
                      int64_t offset,
                      const std::string &language,
                      const std::string &charset) {
  assert(in);

  in->setFilePointer(0);

  kax_chapters_cptr chaps{new KaxChapters};
  KaxChapterAtom *atom     = nullptr;
  KaxEditionEntry *edition = nullptr;
  int mode                 = 0;
  int num                  = 0;
  int64_t start            = 0;
  charset_converter_cptr cc_utf8;

  // The core now uses ns precision timecodes.
  if (0 < min_tc)
    min_tc /= 1000000;
  if (0 < max_tc)
    max_tc /= 1000000;
  if (0 < offset)
    offset /= 1000000;

  bool do_convert = in->get_byte_order() == BO_NONE;
  if (do_convert)
    cc_utf8 = charset_converter_c::init(charset);

  std::string use_language =
      !language.empty()                   ? language
    : !g_default_chapter_language.empty() ? g_default_chapter_language
    :                                       "eng";

  boost::regex timecode_line_re(SIMCHAP_RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex timecode_re(     SIMCHAP_RE_TIMECODE,      boost::regex::perl);
  boost::regex name_line_re(    SIMCHAP_RE_NAME_LINE,     boost::regex::perl);
  boost::smatch matches;

  std::string line, timecode_as_string;

  while (in->getline2(line)) {
    strip(line);
    if (line.empty())
      continue;

    if (0 == mode) {
      if (!boost::regex_match(line, matches, timecode_line_re))
        chapter_error(boost::format(Y("'%1%' is not a CHAPTERxx=... line.")) % line);

      int64_t hour = 0, minute = 0, second = 0, msecs = 0;
      parse_number(matches[1].str(), hour);
      parse_number(matches[2].str(), minute);
      parse_number(matches[3].str(), second);
      parse_number(matches[4].str(), msecs);

      if (59 < minute)
        chapter_error(boost::format(Y("Invalid minute: %1%")) % minute);
      if (59 < second)
        chapter_error(boost::format(Y("Invalid second: %1%")) % second);

      start = msecs + second * 1000 + minute * 1000 * 60 + hour * 1000 * 60 * 60;
      mode  = 1;

      if (!boost::regex_match(line, matches, timecode_re))
        chapter_error(boost::format(Y("'%1%' is not a CHAPTERxx=... line.")) % line);

      timecode_as_string = matches[1].str();

    } else {
      if (!boost::regex_match(line, matches, name_line_re))
        chapter_error(boost::format(Y("'%1%' is not a CHAPTERxxNAME=... line.")) % line);

      std::string name = matches[1].str();
      if (name.empty())
        name = timecode_as_string;

      mode = 0;

      if ((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))) {
        if (!edition)
          edition = &GetChild<KaxEditionEntry>(*chaps);

        atom = &GetFirstOrNextChild<KaxChapterAtom>(*edition, atom);
        GetChild<KaxChapterUID>(*atom).SetValue(create_unique_number(UNIQUE_CHAPTER_IDS));
        GetChild<KaxChapterTimeStart>(*atom).SetValue((start - offset) * 1000000);

        auto &display = GetChild<KaxChapterDisplay>(*atom);

        GetChild<KaxChapterString>(display).SetValue(cstrutf8_to_UTFstring(do_convert ? cc_utf8->utf8(name) : name));
        GetChild<KaxChapterLanguage>(display).SetValue(use_language);

        if (!g_default_chapter_country.empty())
          GetChild<KaxChapterCountry>(display).SetValue(g_default_chapter_country);

        ++num;
      }
    }
  }

  return 0 == num ? kax_chapters_cptr{} : chaps;
}

/** \brief Probe a file for different chapter formats and parse the file.

   The file \a file_name is opened and checked for supported chapter formats.
   These include simple OGM style chapters, CUE sheets and mkvtoolnix' own
   XML chapter format.

   Its parameters don't have to be checked for validity.

   \param file_name The name of the text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for entries that don't specifiy it.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary. This parameter is ignored for XML
     chapter files.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c nullptr will be returned.
   \param is_simple_format This boolean will be set to \c true if the chapter
     format is either the OGM style format or a CUE sheet. May be \c nullptr if
     the caller is not interested in the result.
   \param tags When parsing a CUE sheet tags will be created along with the
     chapter entries. These tags will be stored in this parameter.

   \return The chapters parsed from the file or \c nullptr if an error occured.

   \see ::parse_chapters(mm_text_io_c *in,int64_t min_tc,int64_t max_tc, int64_t offset,const std::string &language,const std::string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
*/
kax_chapters_cptr
parse_chapters(const std::string &file_name,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const std::string &language,
               const std::string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  try {
    mm_text_io_c in(new mm_file_io_c(file_name));
    return parse_chapters(&in, min_tc, max_tc, offset, language, charset, exception_on_error, is_simple_format, tags);

  } catch (mtx::chapter_parser_x &e) {
    if (exception_on_error)
      throw;
    mxerror(boost::format(Y("Could not parse the chapters in '%1%': %2%\n")) % file_name % e.error());

  } catch (...) {
    if (exception_on_error)
      throw mtx::chapter_parser_x(boost::format(Y("Could not open '%1%' for reading.\n")) % file_name);
    else
      mxerror(boost::format(Y("Could not open '%1%' for reading.\n")) % file_name);
  }

  return kax_chapters_cptr{};
}

/** \brief Probe a file for different chapter formats and parse the file.

   The file \a in is checked for supported chapter formats. These include
   simple OGM style chapters, CUE sheets and mkvtoolnix' own XML chapter
   format.

   The parameters are checked for validity.

   \param in The text file to read from.
   \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
     given then only those chapters that lie in the timerange
     <tt>[min_tc..max_tc]</tt> are kept.
   \param offset An optional offset that is subtracted from all start and
     end timecodes after the timerange check has been made.
   \param language This language is added as the \c KaxChapterLanguage
     for entries that don't specifiy it.
   \param charset The charset the chapters are supposed to be it. The entries
     will be converted to UTF-8 if necessary. This parameter is ignored for XML
     chapter files.
   \param exception_on_error If set to \c true then an exception is thrown
     if an error occurs. Otherwise \c nullptr will be returned.
   \param is_simple_format This boolean will be set to \c true if the chapter
     format is either the OGM style format or a CUE sheet. May be \c nullptr if
     the caller is not interested in the result.
   \param tags When parsing a CUE sheet tags will be created along with the
     chapter entries. These tags will be stored in this parameter.

   \return The chapters parsed from the file or \c nullptr if an error occured.

   \see ::parse_chapters(const std::string &file_name,int64_t min_tc,int64_t max_tc, int64_t offset,const std::string &language,const std::string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
*/
kax_chapters_cptr
parse_chapters(mm_text_io_c *in,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const std::string &language,
               const std::string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  assert(in);

  std::string error;

  try {
    if (probe_simple_chapters(in)) {
      if (is_simple_format)
        *is_simple_format = true;
      return parse_simple_chapters(in, min_tc, max_tc, offset, language, charset);

    } else if (probe_cue_chapters(in)) {
      if (is_simple_format)
        *is_simple_format = true;
      return parse_cue_chapters(in, min_tc, max_tc, offset, language, charset, tags);

    } else if (is_simple_format)
      *is_simple_format = false;

    if (mtx::xml::ebml_chapters_converter_c::probe_file(in->get_file_name())) {
      auto chapters = mtx::xml::ebml_chapters_converter_c::parse_file(in->get_file_name(), true);
      return select_chapters_in_timeframe(chapters.get(), min_tc, max_tc, offset) ? chapters : kax_chapters_cptr{};
    }

    error = (boost::format(Y("Unknown chapter file format in '%1%'. It does not contain a supported chapter format.\n")) % in->get_file_name()).str();
  } catch (mtx::chapter_parser_x &e) {
    error = e.error();
  } catch (mtx::mm_io::exception &ex) {
    error = (boost::format(Y("The XML chapter file '%1%' could not be read.\n")) % in->get_file_name()).str();
  } catch (mtx::xml::xml_parser_x &ex) {
    error = (boost::format(Y("The XML chapter file '%1%' contains an error at position %3%: %2%\n")) % in->get_file_name() % ex.result().description() % ex.result().offset).str();
  } catch (mtx::xml::exception &ex) {
    error = (boost::format(Y("The XML chapter file '%1%' contains an error: %2%\n")) % in->get_file_name() % ex.what()).str();
  }

  if (!error.empty()) {
    if (exception_on_error)
      throw mtx::chapter_parser_x(error);
    mxerror(error);
  }

  return kax_chapters_cptr{};
}

/** \brief Get the start timecode for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the start timecode should be returned.
   \param value_if_not_found The value to return if no start timecode child
     element was found. Defaults to -1.

   \return The start timecode or \c value_if_not_found if the atom doesn't
     contain such a child element.
*/
int64_t
get_chapter_start(KaxChapterAtom &atom,
                  int64_t value_if_not_found) {
  KaxChapterTimeStart *start = FindChild<KaxChapterTimeStart>(&atom);

  return !start ? value_if_not_found : static_cast<int64_t>(start->GetValue());
}

/** \brief Get the end timecode for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the end timecode should be returned.
   \param value_if_not_found The value to return if no end timecode child
     element was found. Defaults to -1.

   \return The start timecode or \c value_if_not_found if the atom doesn't
     contain such a child element.
*/
int64_t
get_chapter_end(KaxChapterAtom &atom,
                int64_t value_if_not_found) {
  KaxChapterTimeEnd *end = FindChild<KaxChapterTimeEnd>(&atom);

  return !end ? value_if_not_found : static_cast<int64_t>(end->GetValue());
}

/** \brief Get the name for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the name should be returned.

   \return The atom's name UTF-8 coded or \c "" if the atom doesn't contain
     such a child element.
*/
std::string
get_chapter_name(KaxChapterAtom &atom) {
  KaxChapterDisplay *display = FindChild<KaxChapterDisplay>(&atom);
  if (!display)
    return "";

  KaxChapterString *name = FindChild<KaxChapterString>(display);
  if (!name)
    return "";

  return UTFstring_to_cstrutf8(UTFstring(*name));
}

/** \brief Get the unique ID for a chapter atom.

   Its parameters don't have to be checked for validity.

   \param atom The atom for which the unique ID should be returned.

   \return The ID or \c -1 if the atom doesn't contain such a
     child element.
*/
int64_t
get_chapter_uid(KaxChapterAtom &atom) {
  KaxChapterUID *uid = FindChild<KaxChapterUID>(&atom);

  return !uid ? -1 : static_cast<int64_t>(uid->GetValue());
}

/** \brief Add missing mandatory elements

   The Matroska specs and \c libmatroska say that several elements are
   mandatory. This function makes sure that they all exist by adding them
   with their default values if they're missing. It works recursively. See
   <a href="http://www.matroska.org/technical/specs/chapters/index.html">
   the Matroska chapter specs</a> for a list or mandatory elements.

   The parameters are checked for validity.

   \param e An element that really is an \c EbmlMaster. \a e's children
     should be checked.
*/
void
fix_mandatory_chapter_elements(EbmlElement *e) {
  if (!e)
    return;

  if (dynamic_cast<KaxEditionEntry *>(e)) {
    KaxEditionEntry &ee = *static_cast<KaxEditionEntry *>(e);
    GetChild<KaxEditionFlagDefault>(ee);
    GetChild<KaxEditionFlagHidden>(ee);

    if (!FindChild<KaxEditionUID>(&ee))
      GetChild<KaxEditionUID>(ee).SetValue(create_unique_number(UNIQUE_EDITION_IDS));

  } else if (dynamic_cast<KaxChapterAtom *>(e)) {
    KaxChapterAtom &a = *static_cast<KaxChapterAtom *>(e);

    GetChild<KaxChapterFlagHidden>(a);
    GetChild<KaxChapterFlagEnabled>(a);

    if (!FindChild<KaxChapterUID>(&a))
      GetChild<KaxChapterUID>(a).SetValue(create_unique_number(UNIQUE_CHAPTER_IDS));

    if (!FindChild<KaxChapterTimeStart>(&a))
      GetChild<KaxChapterTimeStart>(a).SetValue(0);

  } else if (dynamic_cast<KaxChapterTrack *>(e)) {
    KaxChapterTrack &t = *static_cast<KaxChapterTrack *>(e);

    if (!FindChild<KaxChapterTrackNumber>(&t))
      GetChild<KaxChapterTrackNumber>(t).SetValue(0);

  } else if (dynamic_cast<KaxChapterDisplay *>(e)) {
    KaxChapterDisplay &d = *static_cast<KaxChapterDisplay *>(e);

    if (!FindChild<KaxChapterString>(&d))
      GetChild<KaxChapterString>(d).SetValue(L"");

    if (!FindChild<KaxChapterLanguage>(&d))
      GetChild<KaxChapterLanguage>(d).SetValue("eng");

  } else if (dynamic_cast<KaxChapterProcess *>(e)) {
    KaxChapterProcess &p = *static_cast<KaxChapterProcess *>(e);

    GetChild<KaxChapterProcessCodecID>(p);

  } else if (dynamic_cast<KaxChapterProcessCommand *>(e)) {
    KaxChapterProcessCommand &c = *static_cast<KaxChapterProcessCommand *>(e);

    GetChild<KaxChapterProcessTime>(c);
    GetChild<KaxChapterProcessData>(c);

  }

  if (dynamic_cast<EbmlMaster *>(e)) {
    EbmlMaster *m = static_cast<EbmlMaster *>(e);
    size_t i;
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_chapter_elements((*m)[i]);
  }
}

/** \brief Remove all chapter atoms that are outside of a time range

   All chapter atoms that lie completely outside the timecode range
   given with <tt>[min_tc..max_tc]</tt> are deleted. This is the workhorse
   for ::select_chapters_in_timeframe

   Chapters which start before the window but end inside or after the window
   are kept as well, and their start timecode is adjusted.

   Its parameters don't have to be checked for validity.

   \param min_tc The minimum timecode to accept.
   \param max_tc The maximum timecode to accept.
   \param offset This value is subtracted from both the start and end timecode
     for each chapter after the decision whether or not to keep it has been
     made.
   \param m The master containing the elements to check.
*/
static void
remove_entries(int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               EbmlMaster &m) {
  if (0 == m.ListSize())
    return;

  struct chapter_entry_t {
    bool remove, spans, is_atom;
    int64_t start, end;

    chapter_entry_t()
      : remove(false)
      , spans(false)
      , is_atom(false)
      , start(0)
      , end(-1)
    {
    }
  } *entries                = new chapter_entry_t[m.ListSize()];
  unsigned int last_atom_at = 0;
  bool last_atom_found      = false;

  // Determine whether or not an entry has to be removed. Also retrieve
  // the start and end timecodes.
  size_t i;
  for (i = 0; m.ListSize() > i; ++i) {
    KaxChapterAtom *atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (!atom)
      continue;

    last_atom_at       = i;
    last_atom_found    = true;
    entries[i].is_atom = true;

    KaxChapterTimeStart *cts = static_cast<KaxChapterTimeStart *>(atom->FindFirstElt(EBML_INFO(KaxChapterTimeStart), false));

    if (cts)
      entries[i].start = cts->GetValue();

    KaxChapterTimeEnd *cte = static_cast<KaxChapterTimeEnd *>(atom->FindFirstElt(EBML_INFO(KaxChapterTimeEnd), false));

    if (cte)
      entries[i].end = cte->GetValue();
  }

  // We can return if we don't have a single atom to work with.
  if (!last_atom_found)
    return;

  for (i = 0; m.ListSize() > i; ++i) {
    KaxChapterAtom *atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (!atom)
      continue;

    // Calculate the end timestamps and determine whether or not an entry spans
    // several segments.
    if (-1 == entries[i].end) {
      if (i == last_atom_at)
        entries[i].end = 1LL << 62;

      else {
        int next_atom = i + 1;

        while (!entries[next_atom].is_atom)
          ++next_atom;

        entries[i].end = entries[next_atom].start;
      }
    }

    if (   (entries[i].start < min_tc)
        || ((max_tc >= 0) && (entries[i].start > max_tc)))
      entries[i].remove = true;

    if (entries[i].remove && (entries[i].start < min_tc) && (entries[i].end > min_tc))
      entries[i].spans = true;

    mxverb(3,
           boost::format("remove_chapters: entries[%1%]: remove %2% spans %3% start %4% end %5%\n")
           % i % entries[i].remove % entries[i].spans % entries[i].start % entries[i].end);

    // Spanning entries must be kept, and their start timecode must be
    // adjusted. Entries that are to be deleted will be deleted later and
    // have to be skipped for now.
    if (entries[i].remove && !entries[i].spans)
      continue;

    KaxChapterTimeStart *cts = static_cast<KaxChapterTimeStart *>(atom->FindFirstElt(EBML_INFO(KaxChapterTimeStart), false));
    KaxChapterTimeEnd *cte   = static_cast<KaxChapterTimeEnd *>(atom->FindFirstElt(EBML_INFO(KaxChapterTimeEnd), false));

    if (entries[i].spans)
      cts->SetValue(min_tc);

    cts->SetValue(cts->GetValue() - offset);

    if (cte) {
      int64_t end_tc = cte->GetValue();

      if ((max_tc >= 0) && (end_tc > max_tc))
        end_tc = max_tc;
      end_tc -= offset;

      cte->SetValue(end_tc);
    }

    EbmlMaster *m2 = dynamic_cast<EbmlMaster *>(m[i]);
    if (m2)
      remove_entries(min_tc, max_tc, offset, *m2);
  }

  // Now really delete those entries.
  i = m.ListSize();
  while (0 < i) {
    --i;
    if (entries[i].remove && !entries[i].spans) {
      delete m[i];
      m.Remove(i);
    }
  }

  delete []entries;
}

/** \brief Merge all chapter atoms sharing the same UID

   If two or more chapters with the same UID are encountered on the same
   level then those are merged into a single chapter. The start timecode
   is the minimum start timecode of all the chapters, and the end timecode
   is the maximum end timecode of all the chapters.

   The parameters do not have to be checked for validity.

   \param master The master containing the elements to check.
*/
void
merge_chapter_entries(EbmlMaster &master) {
  size_t master_idx;

  // Iterate over all children of the atomaster.
  for (master_idx = 0; master.ListSize() > master_idx; ++master_idx) {
    // Not every child is a chapter atomaster. Skip those.
    KaxChapterAtom *atom = dynamic_cast<KaxChapterAtom *>(master[master_idx]);
    if (!atom)
      continue;

    int64_t uid = get_chapter_uid(*atom);
    if (-1 == uid)
      continue;

    // First get the start and end time, if present.
    int64_t start_tc = get_chapter_start(*atom, 0);
    int64_t end_tc   = get_chapter_end(*atom);

    mxverb(3, boost::format("chapters: merge_entries: looking for %1% with %2%, %3%\n") % uid % start_tc % end_tc);

    // Now iterate over all remaining atoms and find those with the same
    // UID.
    size_t merge_idx = master_idx + 1;
    while (true) {
      KaxChapterAtom *merge_this = nullptr;
      for (; master.ListSize() > merge_idx; ++merge_idx) {
        KaxChapterAtom *cmp_atom = dynamic_cast<KaxChapterAtom *>(master[merge_idx]);
        if (!cmp_atom)
          continue;

        if (get_chapter_uid(*cmp_atom) == uid) {
          merge_this = cmp_atom;
          break;
        }
      }

      // If we haven't found an atom with the same UID then we're done here.
      if (!merge_this)
        break;

      // Do the merger! First get the start and end timecodes if present.
      int64_t merge_start_tc = get_chapter_start(*merge_this, 0);
      int64_t merge_end_tc   = get_chapter_end(*merge_this);

      // Then compare them to the ones we have for the soon-to-be merged
      // chapter and assign accordingly.
      if (merge_start_tc < start_tc)
        start_tc = merge_start_tc;

      if ((-1 == end_tc) || (merge_end_tc > end_tc))
        end_tc = merge_end_tc;

      mxverb(3, boost::format("chapters: merge_entries:   found one at %1% with %2%, %3%; merged to %4%, %5%\n") % merge_idx % merge_start_tc % merge_end_tc % start_tc % end_tc);

      // Finally remove the entry itself.
      delete master[merge_idx];
      master.Remove(merge_idx);
    }

    // Assign the start and end timecode to the chapter. Only assign an
    // end timecode if one was present in at least one of the merged
    // chapter atoms.
    GetChild<KaxChapterTimeStart>(*atom).SetValue(start_tc);
    if (-1 != end_tc)
      GetChild<KaxChapterTimeEnd>(*atom).SetValue(end_tc);
  }

  // Recusively merge atoms.
  for (master_idx = 0; master.ListSize() > master_idx; ++master_idx) {
    EbmlMaster *merge_master = dynamic_cast<EbmlMaster *>(master[master_idx]);
    if (merge_master)
      merge_chapter_entries(*merge_master);
  }
}

/** \brief Remove all chapter atoms that are outside of a time range

   All chapter atoms that lie completely outside the timecode range
   given with <tt>[min_tc..max_tc]</tt> are deleted.

   Chapters which start before the window but end inside or after the window
   are kept as well, and their start timecode is adjusted.

   If two or more chapters with the same UID are encountered on the same
   level then those are merged into a single chapter. The start timecode
   is the minimum start timecode of all the chapters, and the end timecode
   is the maximum end timecode of all the chapters.

   The parameters are checked for validity.

   \param chapters The chapters to check.
   \param min_tc The minimum timecode to accept.
   \param max_tc The maximum timecode to accept.
   \param offset This value is subtracted from both the start and end timecode
     for each chapter after the decision whether or not to keep it has been
     made.

   \return \c false if all chapters were discarded, \c true otherwise
*/
bool
select_chapters_in_timeframe(KaxChapters *chapters,
                             int64_t min_tc,
                             int64_t max_tc,
                             int64_t offset) {
  // Check the parameters.
  if (!chapters)
    return false;

  // Remove the atoms that are outside of the requested range.
  size_t master_idx;
  for (master_idx = 0; chapters->ListSize() > master_idx; master_idx++) {
    EbmlMaster *work_master = dynamic_cast<KaxEditionEntry *>((*chapters)[master_idx]);
    if (work_master)
      remove_entries(min_tc, max_tc, offset, *work_master);
  }

  // Count the number of atoms in each edition. Delete editions without
  // any atom in them.
  master_idx = 0;
  while (chapters->ListSize() > master_idx) {
    KaxEditionEntry *eentry = dynamic_cast<KaxEditionEntry *>((*chapters)[master_idx]);
    if (!eentry) {
      master_idx++;
      continue;
    }

    size_t num_atoms = 0, eentry_idx;
    for (eentry_idx = 0; eentry->ListSize() > eentry_idx; eentry_idx++)
      if (dynamic_cast<KaxChapterAtom *>((*eentry)[eentry_idx]))
        num_atoms++;

    if (0 == num_atoms) {
      chapters->Remove(master_idx);
      delete eentry;

    } else
      master_idx++;
  }

  return chapters->ListSize() > 0;
}

/** \brief Find an edition with a specific UID.

   Its parameters don't have to be checked for validity.

   \param chapters The chapters in which to look for the edition.
   \param uid The requested unique edition ID. The special value \c 0
     results in the first edition being returned.

   \return A pointer to the edition or \c nullptr if none has been found.
*/
KaxEditionEntry *
find_edition_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  if (0 == uid)
    return FindChild<KaxEditionEntry>(&chapters);

  size_t eentry_idx;
  for (eentry_idx = 0; chapters.ListSize() > eentry_idx; eentry_idx++) {
    KaxEditionEntry *eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (!eentry)
      continue;

    KaxEditionUID *euid = FindChild<KaxEditionUID>(eentry);
    if (euid && (euid->GetValue() == uid))
      return eentry;
  }

  return nullptr;
}

/** \brief Find a chapter atom with a specific UID.

   Its parameters don't have to be checked for validity.

   \param chapters The chapters in which to look for the atom.
   \param uid The requested unique atom ID. The special value \c 0 results in
     the first atom in the first edition being returned.

   \return A pointer to the atom or \c nullptr if none has been found.
*/
KaxChapterAtom *
find_chapter_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  if (0 == uid) {
    KaxEditionEntry *eentry = FindChild<KaxEditionEntry>(&chapters);
    if (!eentry)
      return nullptr;
    return FindChild<KaxChapterAtom>(eentry);
  }

  size_t eentry_idx;
  for (eentry_idx = 0; chapters.ListSize() > eentry_idx; eentry_idx++) {
    KaxEditionEntry *eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (!eentry)
      continue;

    size_t atom_idx;
    for (atom_idx = 0; eentry->ListSize() > atom_idx; atom_idx++) {
      KaxChapterAtom *atom = dynamic_cast<KaxChapterAtom *>((*eentry)[atom_idx]);
      if (!atom)
        continue;

      KaxChapterUID *cuid = FindChild<KaxChapterUID>(atom);
      if (cuid && (cuid->GetValue() == uid))
        return atom;
    }
  }

  return nullptr;
}

/** \brief Move all chapter atoms to another container keeping editions intact

   This function moves all chapter atoms from \a src to \a dst.
   If there's already an edition in \a dst with the same UID as the current
   one in \a src, then all atoms will be put into that edition. Otherwise
   the complete edition will simply be moved over.

   After processing \a src will be empty.

   Its parameters don't have to be checked for validity.

   \param dst The container the atoms and editions will be put into.
   \param src The container the atoms and editions will be taken from.
*/
void
move_chapters_by_edition(KaxChapters &dst,
                         KaxChapters &src) {
  size_t src_idx;
  for (src_idx = 0; src.ListSize() > src_idx; src_idx++) {
    EbmlMaster *m = dynamic_cast<EbmlMaster *>(src[src_idx]);
    if (!m)
      continue;

    // Find an edition to which these atoms will be added.
    KaxEditionEntry *ee_dst = nullptr;
    KaxEditionUID *euid_src = FindChild<KaxEditionUID>(m);
    if (euid_src)
      ee_dst = find_edition_with_uid(dst, euid_src->GetValue());

    // No edition with the same UID found as the one we want to handle?
    // Then simply move the complete edition over.
    if (!ee_dst)
      dst.PushElement(*m);
    else {
      // Move all atoms from the old edition to the new one.
      size_t master_idx;
      for (master_idx = 0; m->ListSize() > master_idx; master_idx++)
        if (is_id((*m)[master_idx], KaxChapterAtom))
          ee_dst->PushElement(*(*m)[master_idx]);
        else
          delete (*m)[master_idx];

      m->RemoveAll();
      delete m;
    }
  }

  src.RemoveAll();
}

/** \brief Adjust all start and end timecodes by an offset

   All start and end timecodes are adjusted by an offset. This is done
   recursively.

   Its parameters don't have to be checked for validity.

   \param master A master containint the elements to adjust. This can be
     a KaxChapters, KaxEditionEntry or KaxChapterAtom object.
   \param offset The offset to add to each timecode. Can be negative. If
     the resulting timecode would be smaller than zero then it will be set
     to zero.
*/
void
adjust_chapter_timecodes(EbmlMaster &master,
                         int64_t offset) {
  size_t master_idx;
  for (master_idx = 0; master.ListSize() > master_idx; master_idx++) {
    if (!is_id(master[master_idx], KaxChapterAtom))
      continue;

    KaxChapterAtom *atom       = static_cast<KaxChapterAtom *>(master[master_idx]);
    KaxChapterTimeStart *start = FindChild<KaxChapterTimeStart>(atom);
    KaxChapterTimeEnd *end     = FindChild<KaxChapterTimeEnd>(atom);

    if (start)
      start->SetValue(std::max<int64_t>(static_cast<int64_t>(start->GetValue()) + offset, 0));

    if (end)
      end->SetValue(std::max<int64_t>(static_cast<int64_t>(end->GetValue()) + offset, 0));
  }

  for (master_idx = 0; master.ListSize() > master_idx; master_idx++) {
    EbmlMaster *work_master = dynamic_cast<EbmlMaster *>(master[master_idx]);
    if (work_master)
      adjust_chapter_timecodes(*work_master, offset);
  }
}

static int
count_chapter_atoms_recursively(EbmlMaster &master,
                                int count) {
  size_t master_idx;

  for (master_idx = 0; master.ListSize() > master_idx; ++master_idx)
    if (is_id(master[master_idx], KaxChapterAtom))
      ++count;

    else if (dynamic_cast<EbmlMaster *>(master[master_idx]))
      count = count_chapter_atoms_recursively(*static_cast<EbmlMaster *>(master[master_idx]), count);

  return count;
}

int
count_chapter_atoms(EbmlMaster &master) {
  return count_chapter_atoms_recursively(master, 0);
}

/** \brief Change the chapter edition UIDs to a single value

   This function changes the UIDs of all editions for which the
   function is called to a single value. This is intended for chapters
   read from source files which do not provide their own edition UIDs
   (e.g. MP4 or OGM files) so that their chapters can be appended and
   don't end up in separate editions.

   \c chapters may be nullptr in which case nothing is done.

   \param dst chapters The chapter structure for which all edition
      UIDs will be changed.
*/
void
align_chapter_edition_uids(KaxChapters *chapters) {
  if (!chapters)
    return;

  static uint64_t s_shared_edition_uid = 0;

  if (0 == s_shared_edition_uid)
    s_shared_edition_uid = create_unique_number(UNIQUE_CHAPTER_IDS);

  size_t idx;
  for (idx = 0; chapters->ListSize() > idx; ++idx) {
    KaxEditionEntry *edition_entry = dynamic_cast<KaxEditionEntry *>((*chapters)[idx]);
    if (!edition_entry)
      continue;

    GetChild<KaxEditionUID>(*edition_entry).SetValue(s_shared_edition_uid);
  }
}

void
align_chapter_edition_uids(KaxChapters &reference,
                           KaxChapters &modify) {
  size_t reference_idx = 0, modify_idx = 0;

  while (1) {
    KaxEditionEntry *ee_reference = nullptr;;
    while ((reference.ListSize() > reference_idx) && !(ee_reference = dynamic_cast<KaxEditionEntry *>(reference[reference_idx])))
      ++reference_idx;

    if (!ee_reference)
      return;

    KaxEditionEntry *ee_modify = nullptr;;
    while ((modify.ListSize() > modify_idx) && !(ee_modify = dynamic_cast<KaxEditionEntry *>(modify[modify_idx])))
      ++modify_idx;

    if (!ee_modify)
      return;

    GetChild<KaxEditionUID>(*ee_modify).SetValue(GetChild<KaxEditionUID>(*ee_reference).GetValue());
    ++reference_idx;
    ++modify_idx;
  }
}
