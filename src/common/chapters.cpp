/** \brief chapter parser and helper functions
 *
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * \file
 * \version $Id$
 *
 * \author Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <stdarg.h>

#include <cassert>
#include <string>

#include <matroska/KaxChapters.h>

#include "chapters.h"
#include "commonebml.h"
#include "error.h"

using namespace std;
using namespace libmatroska;

/** The default language for all chapter entries that don't have their own. */
string default_chapter_language;
/** The default country for all chapter entries that don't have their own. */
string default_chapter_country;

/** Is the current char an equal sign? */
#define isequal(s) (*(s) == '=')
/** Is the current char a colon? */
#define iscolon(s) (*(s) == ':')
/** Is the current char a dot? */
#define isdot(s) (*(s) == '.')
/** Do we have two consecutive digits? */
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
/** Do we have three consecutive digits? */
#define isthreedigits(s) (istwodigits(s) && isdigit(*(s + 2)))
/** Does \c s point to the string "CHAPTER"? */
#define ischapter(s) (!strncmp("CHAPTER", (s), 7))
/** Does \c s point to the string "NAME"? */
#define isname(s) (!strncmp("NAME", (s), 4))
/** Does \c s point to a valid OGM style chapter timecode entry? */
#define ischapterline(s) ((strlen(s) == 22) && \
                          ischapter(s) && \
                          istwodigits(s + 7) && \
                          isequal(s + 9) && \
                          istwodigits(s + 10) && \
                          iscolon(s + 12) && \
                          istwodigits(s + 13) && \
                          iscolon(s + 15) && \
                          istwodigits(s + 16) && \
                          isdot(s + 18) && \
                          isthreedigits(s + 19))
/** Does \c s point to a valid OGM style chapter name entry? */
#define ischapternameline(s) ((strlen(s) >= 15) && \
                          ischapter(s) && \
                          istwodigits(s + 7) && \
                          isname(s + 9) && \
                          isequal(s + 13) && \
                          !isblanktab(*(s + 14)))

/** \brief Format an error message and throw an exception.
 *
 * A \c printf like function that throws an ::error_c exception with
 * the formatted message.
 *
 * The parameters are checked for validity.
 *
 * \param fmt The \c printf like format.
 * \param ... Optional arguments for the format.
 */
static void
chapter_error(const char *fmt,
              ...) {
  va_list ap;
  string new_fmt;
  char *new_error;
  int len;

  assert(fmt != NULL);

  len = strlen("Error: Simple chapter parser: ");
  va_start(ap, fmt);
  fix_format(fmt, new_fmt);
  len += get_varg_len(new_fmt.c_str(), ap);
  new_error = (char *)safemalloc(len + 2);
  strcpy(new_error, "Error: Simple chapter parser: ");
  vsprintf(&new_error[strlen(new_error)], new_fmt.c_str(), ap);
  strcat(new_error, "\n");
  va_end(ap);
  throw error_c(new_error, true);
}

/** \brief Reads the start of a file and checks for OGM style comments.
 *
 * The first lines are read. OGM style comments are recognized if the first
 * non-empty line contains <tt>CHAPTER01=...</tt> and the first non-empty
 * line afterwards contains <tt>CHAPTER01NAME=...</tt>.
 *
 * The parameters are checked for validity.
 *
 * \param in The file to read from.
 *
 * \return \c true if the file contains OGM style comments and \c false
 *   otherwise.
 */
bool
probe_simple_chapters(mm_text_io_c *in) {
  string line;

  assert(in != NULL);

  in->setFilePointer(0);
  while (in->getline2(line)) {
    strip(line);
    if (line.length() == 0)
      continue;

    if (!ischapterline(line.c_str()))
      return false;

    while (in->getline2(line)) {
      strip(line);
      if (line.length() == 0)
        continue;

      if (!ischapternameline(line.c_str()))
        return false;

      return true;
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
 *
 * The file \a in is read. The content is assumed to be OGM style comments.
 *
 * The parameters are checked for validity.
 *
 * \param in The text file to read from.
 * \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
 *   given then only those chapters that lie in the timerange
 *   <tt>[min_tc..max_tc]</tt> are kept.
 * \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
 *   given then only those chapters that lie in the timerange
 *   <tt>[min_tc..max_tc]</tt> are kept.
 * \param offset An optional offset that is subtracted from all start and
 *   end timecodes after the timerange check has been made.
 * \param language This language is added as the \c KaxChapterLanguage
 *   for all entries.
 * \param country This country is added as the \c KaxChapterCountry for
 *   all entries.
 * \param exception_on_error If set to \c true then an exception is thrown
 *   if an error occurs. Otherwise \c NULL will be returned.
 *
 * \return The chapters parsed from the file or \c NULL if an error occured.
 */
KaxChapters *
parse_simple_chapters(mm_text_io_c *in,
                      int64_t min_tc,
                      int64_t max_tc,
                      int64_t offset,
                      const string &language,
                      const string &charset,
                      bool exception_on_error) {
  KaxChapters *chaps;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int64_t start, hour, minute, second, msecs;
  string name, line, use_language;
  int mode, num, cc_utf8;
  bool do_convert;
  UTFstring wchar_string;

  assert(in != NULL);

  in->setFilePointer(0);
  chaps = new KaxChapters;

  mode = 0;
  atom = NULL;
  edition = NULL;
  num = 0;
  start = 0;
  cc_utf8 = 0;

  // The core now uses ns precision timecodes.
  if (min_tc > 0)
    min_tc /= 1000000;
  if (max_tc > 0)
    max_tc /= 1000000;
  if (offset > 0)
    offset /= 1000000;

  if (in->get_byte_order() == BO_NONE) {
    do_convert = true;
    cc_utf8 = utf8_init(charset.c_str());

  } else
    do_convert = false;

  if (language == "") {
    if (default_chapter_language.length() > 0)
      use_language = default_chapter_language;
    else
      use_language = "eng";
  } else
    use_language = language;

  try {
    while (in->getline2(line)) {
      strip(line);
      if (line.length() == 0)
        continue;

      if (mode == 0) {
        if (!ischapterline(line.c_str()))
          chapter_error("'%s' is not a CHAPTERxx=... line.", line.c_str());
        parse_int(line.substr(10, 2), hour);
        parse_int(line.substr(13, 2), minute);
        parse_int(line.substr(16, 2), second);
        parse_int(line.substr(19, 3), msecs);
        if (hour > 23)
          chapter_error("Invalid hour: %d", hour);
        if (minute > 59)
          chapter_error("Invalid minute: %d", minute);
        if (second > 59)
          chapter_error("Invalid second: %d", second);
        start = msecs + second * 1000 + minute * 1000 * 60 +
          hour * 1000 * 60 * 60;
        mode = 1;

      } else {
        if (!ischapternameline(line.c_str()))
          chapter_error("'%s' is not a CHAPTERxxNAME=... line.", line.c_str());
        name = line.substr(14);
        mode = 0;

        if ((start >= min_tc) && ((start <= max_tc) || (max_tc == -1))) {
          if (edition == NULL)
            edition = &GetChild<KaxEditionEntry>(*chaps);
          if (atom == NULL)
            atom = &GetChild<KaxChapterAtom>(*edition);
          else
            atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
            create_unique_uint32(UNIQUE_CHAPTER_IDS);

          *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
            (start - offset) * 1000000;

          display = &GetChild<KaxChapterDisplay>(*atom);

          if (do_convert)
            wchar_string = cstrutf8_to_UTFstring(to_utf8(cc_utf8, name));
          else
            wchar_string = cstrutf8_to_UTFstring(name);
          *static_cast<EbmlUnicodeString *>
            (&GetChild<KaxChapterString>(*display)) = wchar_string;

          *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
            use_language;

          if (default_chapter_country.length() > 0)
            *static_cast<EbmlString *>
              (&GetChild<KaxChapterCountry>(*display)) =
              default_chapter_country;

          num++;
        }
      }
    }
  } catch (error_c e) {
    delete chaps;
    throw error_c(e);
  }

  if (num == 0) {
    delete chaps;
    return NULL;
  }

  return chaps;
}

/** \brief Probe a file for different chapter formats and parse the file.
 *
 * The file \a file_name is opened and checked for supported chapter formats.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \see ::parse_chapters(mm_text_io_c *in,int64_t min_tc,int64_t max_tc, int64_t offset,const string &language,const string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
 * for a full description of its parameters and return values.
 *
 * \param file_name The file name that is to be opened.
 */
KaxChapters *
parse_chapters(const string &file_name,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const string &language,
               const string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  mm_text_io_c *in;
  KaxChapters *result;

  in = NULL;
  result = NULL;
  try {

    in = new mm_text_io_c(new mm_file_io_c(file_name));
    result = parse_chapters(in, min_tc, max_tc, offset, language, charset,
                            exception_on_error, is_simple_format, tags);
    delete in;
  } catch (...) {
    if (in != NULL)
      delete in;
    if (exception_on_error)
      throw error_c(mxsprintf("Could not open '%s' for reading.\n",
                              file_name.c_str()));
    else
      mxerror("Could not open '%s' for reading.\n", file_name.c_str());
  }
  return result;
}

/** \brief Probe a file for different chapter formats and parse the file.
 *
 * The file \a in is checked for supported chapter formats. These include
 * simple OGM style chapters, CUE sheets and mkvtoolnix' own XML chapter
 * format.
 *
 * The parameters are checked for validity.
 *
 * \param in The text file to read from.
 * \param min_tc An optional timecode. If both \a min_tc and \a max_tc are
 *   given then only those chapters that lie in the timerange
 *   <tt>[min_tc..max_tc]</tt> are kept.
 * \param max_tc An optional timecode. If both \a min_tc and \a max_tc are
 *   given then only those chapters that lie in the timerange
 *   <tt>[min_tc..max_tc]</tt> are kept.
 * \param offset An optional offset that is subtracted from all start and
 *   end timecodes after the timerange check has been made.
 * \param language This language is added as the \c KaxChapterLanguage
 *   for entries that don't specifiy it.
 * \param country This country is added as the \c KaxChapterCountry for
 *   entries that don't specifiy it.
 * \param exception_on_error If set to \c true then an exception is thrown
 *   if an error occurs. Otherwise \c NULL will be returned.
 * \param is_simple_format This boolean will be set to \c true if the chapter
 *   format is either the OGM style format or a CUE sheet.
 * \param tags When parsing a CUE sheet tags will be created along with the
 *   chapter entries. These tags will be stored in this parameter.
 *
 * \return The chapters parsed from the file or \c NULL if an error occured.
 *
 * \see ::parse_chapters(const string &file_name,int64_t min_tc,int64_t max_tc, int64_t offset,const string &language,const string &charset,bool exception_on_error,bool *is_simple_format,KaxTags **tags)
 */
KaxChapters *
parse_chapters(mm_text_io_c *in,
               int64_t min_tc,
               int64_t max_tc,
               int64_t offset,
               const string &language,
               const string &charset,
               bool exception_on_error,
               bool *is_simple_format,
               KaxTags **tags) {
  assert(in != NULL);

  try {
    if (probe_simple_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_simple_chapters(in, min_tc, max_tc, offset, language,
                                   charset, exception_on_error);
    } else if (probe_cue_chapters(in)) {
      if (is_simple_format != NULL)
        *is_simple_format = true;
      return parse_cue_chapters(in, min_tc, max_tc, offset, language,
                                charset, exception_on_error, tags);
    } else if (is_simple_format != NULL)
      *is_simple_format = false;

    if (probe_xml_chapters(in))
      return parse_xml_chapters(in, min_tc, max_tc, offset,
                                exception_on_error);

    throw error_c(mxsprintf("Unknown file format for '%s'. It does not "
                            "contain a support chapter format.\n",
                            in->get_file_name().c_str()));
  } catch (error_c e) {
    if (exception_on_error)
      throw e;
    mxerror("%s", e.get_error());
  }

  return NULL;
}

/** \brief Get the start timecode for a chapter atom.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param atom The atom for which the start timecode should be returned.
 *
 * \return The start timecode or \c -1 if the atom doesn't contain such a
 *   child element.
 */
int64_t
get_chapter_start(KaxChapterAtom &atom) {
  KaxChapterTimeStart *start;

  start = FINDFIRST(&atom, KaxChapterTimeStart);
  if (start == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(start));
}

/** \brief Get the name for a chapter atom.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param atom The atom for which the name should be returned.
 *
 * \return The atom's name UTF-8 coded or \c "" if the atom doesn't contain
 *   such a child element.
 */
string
get_chapter_name(KaxChapterAtom &atom) {
  KaxChapterDisplay *display;
  KaxChapterString *name;

  display = FINDFIRST(&atom, KaxChapterDisplay);
  if (display == NULL)
    return "";
  name = FINDFIRST(display, KaxChapterString);
  if (name == NULL)
    return "";
  return UTFstring_to_cstrutf8(UTFstring(*name));
}

/** \brief Get the unique ID for a chapter atom.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param atom The atom for which the unique ID should be returned.
 *
 * \return The ID or \c -1 if the atom doesn't contain such a
 *   child element.
 */
int64_t
get_chapter_uid(KaxChapterAtom &atom) {
  KaxChapterUID *uid;

  uid = FINDFIRST(&atom, KaxChapterUID);
  if (uid == NULL)
    return -1;
  return uint64(*static_cast<EbmlUInteger *>(uid));
}

/** \brief Add missing mandatory elements
 *
 * The Matroska specs and \c libmatroska say that several elements are
 * mandatory. This function makes sure that they all exist by adding them
 * with their default values if they're missing. It works recursively. See
 * \url http://www.matroska.org/technical/specs/chapters/index.html
 * for a list or mandatory elements.
 *
 * The parameters are checked for validity.
 *
 * \param e An element that really is an \c EbmlMaster. \a e's children
 *   should be checked.
 */
void
fix_mandatory_chapter_elements(EbmlElement *e) {
  if (e == NULL)
    return;

  if (dynamic_cast<KaxEditionEntry *>(e) != NULL) {
    KaxEditionEntry &ee = *static_cast<KaxEditionEntry *>(e);
    GetChild<KaxEditionFlagDefault>(ee);
    GetChild<KaxEditionFlagHidden>(ee);
    GetChild<KaxEditionProcessed>(ee);
    if (FINDFIRST(&ee, KaxEditionUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxEditionUID>(ee)) =
        create_unique_uint32(UNIQUE_EDITION_IDS);

  } else if (dynamic_cast<KaxChapterAtom *>(e) != NULL) {
    KaxChapterAtom &a = *static_cast<KaxChapterAtom *>(e);

    GetChild<KaxChapterFlagHidden>(a);
    GetChild<KaxChapterFlagEnabled>(a);
    if (FINDFIRST(&a, KaxChapterUID) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(a)) =
        create_unique_uint32(UNIQUE_CHAPTER_IDS);
    if (FINDFIRST(&a, KaxChapterTimeStart) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(a)) = 0;

  } else if (dynamic_cast<KaxChapterTrack *>(e) != NULL) {
    KaxChapterTrack &t = *static_cast<KaxChapterTrack *>(e);

    if (FINDFIRST(&t, KaxChapterTrackNumber) == NULL)
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTrackNumber>(t)) = 0;

  } else if (dynamic_cast<KaxChapterDisplay *>(e) != NULL) {
    KaxChapterDisplay &d = *static_cast<KaxChapterDisplay *>(e);

    if (FINDFIRST(&d, KaxChapterString) == NULL)
      *static_cast<EbmlUnicodeString *>(&GetChild<KaxChapterString>(d)) = L"";
    if (FINDFIRST(&d, KaxChapterLanguage) == NULL)
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(d)) = "und";

  }

  if (dynamic_cast<EbmlMaster *>(e) != NULL) {
    EbmlMaster *m;
    int i;

    m = static_cast<EbmlMaster *>(e);
    for (i = 0; i < m->ListSize(); i++)
      fix_mandatory_chapter_elements((*m)[i]);
  }
}

/** \brief Remove all chapter atoms that are outside of a time range
 *
 * All chapter atoms that lie completely outside the timecode range
 * given with <tt>[min_tc..max_tc]</tt> are deleted. This is the workhorse
 * for ::select_chapters_in_timeframe
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param min_tc The minimum timecode to accept.
 * \param max_tc The maximum timecode to accept.
 * \param offset This value is subtracted from both the start and end timecode
 *   for each chapter after the decision whether or not to keep it has been
 *   made.
 * \param m The master containing the elements to check.
 */
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
  EbmlMaster *m2;
  int64_t start_tc, end_tc;

  i = 0;
  while (i < m.ListSize()) {
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (atom != NULL) {
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
    atom = dynamic_cast<KaxChapterAtom *>(m[i]);
    if (atom != NULL) {
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
    }

    m2 = dynamic_cast<EbmlMaster *>(m[i]);
    if (m2 != NULL)
      remove_entries(min_tc, max_tc, offset, *m2);
  }    
}

/** \brief Remove all chapter atoms that are outside of a time range
 *
 * All chapter atoms that lie completely outside the timecode range
 * given with <tt>[min_tc..max_tc]</tt> are deleted.
 *
 * The parameters are checked for validity.
 *
 * \param chapters The chapters to check.
 * \param min_tc The minimum timecode to accept.
 * \param max_tc The maximum timecode to accept.
 * \param offset This value is subtracted from both the start and end timecode
 *   for each chapter after the decision whether or not to keep it has been
 *   made.
 *
 * \return \a chapters if there are entries left and \c NULL otherwise.
 */
KaxChapters *
select_chapters_in_timeframe(KaxChapters *chapters,
                             int64_t min_tc,
                             int64_t max_tc,
                             int64_t offset) {
  uint32_t i, k, num_atoms;
  KaxEditionEntry *eentry;

  // Check the parameters.
  if (chapters == NULL)
    return NULL;
  assert(min_tc < max_tc);

  // Remove the atoms that are outside of the requested range.
  for (i = 0; i < chapters->ListSize(); i++) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL)
      continue;
    remove_entries(min_tc, max_tc, offset,
                   *static_cast<EbmlMaster *>((*chapters)[i]));
  }

  // Count the number of atoms in each edition. Delete editions without
  // any atom in them.
  i = 0;
  while (i < chapters->ListSize()) {
    if (dynamic_cast<KaxEditionEntry *>((*chapters)[i]) == NULL) {
      i++;
      continue;
    }
    eentry = static_cast<KaxEditionEntry *>((*chapters)[i]);
    num_atoms = 0;
    for (k = 0; k < eentry->ListSize(); k++)
      if (dynamic_cast<KaxChapterAtom *>((*eentry)[k]) != NULL)
        num_atoms++;

    if (num_atoms == 0) {
      chapters->Remove(i);
      delete eentry;

    } else
      i++;
  }

  // If we don't even have one edition then delete the chapters themselves.
  if (chapters->ListSize() == 0) {
    delete chapters;
    chapters = NULL;
  }

  return chapters;
}

/** \brief Find an edition with a specific UID.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param chapters The chapters in which to look for the edition.
 * \param uid The requested unique edition ID. The special value \c 0
 *   results in the first edition being returned.
 *
 * \return A pointer to the edition or \c NULL if none has been found.
 */
KaxEditionEntry *
find_edition_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxEditionUID *euid;
  int eentry_idx;

  if (uid == 0)
    return FINDFIRST(&chapters, KaxEditionEntry);

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    euid = FINDFIRST(eentry, KaxEditionUID);
    if ((euid != NULL) && (uint64(*euid) == uid))
      return eentry;
  }

  return NULL;
}

/** \brief Find a chapter atom with a specific UID.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param chapters The chapters in which to look for the atom.
 * \param uid The requested unique atom ID. The special value \c 0 results in
 *   the first atom in the first edition being returned.
 *
 * \return A pointer to the atom or \c NULL if none has been found.
 */
KaxChapterAtom *
find_chapter_with_uid(KaxChapters &chapters,
                      uint64_t uid) {
  KaxEditionEntry *eentry;
  KaxChapterAtom *atom;
  KaxChapterUID *cuid;
  int eentry_idx, atom_idx;

  if (uid == 0) {
    eentry = FINDFIRST(&chapters, KaxEditionEntry);
    if (eentry == NULL)
      return NULL;
    return FINDFIRST(eentry, KaxChapterAtom);
  }

  for (eentry_idx = 0; eentry_idx < chapters.ListSize(); eentry_idx++) {
    eentry = dynamic_cast<KaxEditionEntry *>(chapters[eentry_idx]);
    if (eentry == NULL)
      continue;
    for (atom_idx = 0; atom_idx < eentry->ListSize(); atom_idx++) {
      atom = dynamic_cast<KaxChapterAtom *>((*eentry)[atom_idx]);
      if (atom == NULL)
        continue;
      cuid = FINDFIRST(atom, KaxChapterUID);
      if ((cuid != NULL) && (uint64(*cuid) == uid))
        return atom;
    }
  }

  return NULL;
}

/** \brief Move all chapter atoms to another container keeping editions intact
 *
 * This function moves all chapter atoms from \a src to \a dst.
 * If there's already an edition in \a dst with the same UID as the current
 * one in \a src, then all atoms will be put into that edition. Otherwise
 * the complete edition will simply be moved over.
 *
 * After processing \a src will be empty.
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param dst The container the atoms and editions will be put into.
 * \param src The container the atoms and editions will be taken from.
 */
void
move_chapters_by_edition(KaxChapters &dst,
                         KaxChapters &src) {
  int i, k;

  for (i = 0; i < src.ListSize(); i++) {
    KaxEditionEntry *ee_dst;
    KaxEditionUID *euid_src;
    EbmlMaster *m;

    m = static_cast<EbmlMaster *>(src[i]);

    // Find an edition to which these atoms will be added.
    ee_dst = NULL;
    euid_src = FINDFIRST(m, KaxEditionUID);
    if (euid_src != NULL)
      ee_dst = find_edition_with_uid(dst, uint32(*euid_src));

    // No edition with the same UID found as the one we want to handle?
    // Then simply move the complete edition over.
    if (ee_dst == NULL)
      dst.PushElement(*m);
    else {
      // Move all atoms from the old edition to the new one.
      for (k = 0; k < m->ListSize(); k++)
        if (is_id((*m)[k], KaxChapterAtom))
          ee_dst->PushElement(*(*m)[k]);
        else
          delete (*m)[k];
      m->RemoveAll();
      delete m;
    }
  }
  src.RemoveAll();
}

/** \brief Adjust all start and end timecodes by an offset
 *
 * All start and end timecodes are adjusted by an offset. This is done
 * recursively. 
 *
 * Its parameters don't have to be checked for validity.
 *
 * \param master A master containint the elements to adjust. This can be
 *   a KaxChapters, KaxEditionEntry or KaxChapterAtom object.
 * \param offset The offset to add to each timecode. Can be negative. If
 *   the resulting timecode would be smaller than zero then it will be set
 *   to zero.
 */
void
adjust_chapter_timecodes(EbmlMaster &master,
                         int64_t offset) {
  int i;

  for (i = 0; i < master.ListSize(); i++) {
    KaxChapterAtom *atom;
    KaxChapterTimeStart *start;
    KaxChapterTimeEnd *end;
    int64_t new_value;

    if (!is_id(master[i], KaxChapterAtom))
      continue;

    atom = static_cast<KaxChapterAtom *>(master[i]);
    start = FINDFIRST(atom, KaxChapterTimeStart);
    if (start != NULL) {
      new_value = uint64(*start);
      new_value += offset;
      if (new_value < 0)
        new_value = 0;
      *static_cast<EbmlUInteger *>(start) = new_value;
    }

    end = FINDFIRST(atom, KaxChapterTimeEnd);
    if (end != NULL) {
      new_value = uint64(*end);
      new_value += offset;
      if (new_value < 0)
        new_value = 0;
      *static_cast<EbmlUInteger *>(end) = new_value;
    }
  }

  for (i = 0; i < master.ListSize(); i++) {
    EbmlMaster *m;

    m = dynamic_cast<EbmlMaster *>(master[i]);
    if (m != NULL)
      adjust_chapter_timecodes(*m, offset);
  }
}
