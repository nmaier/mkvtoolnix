/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  chapters.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief chapter parser/writer functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdarg.h>

#include <string>

#include <matroska/KaxChapters.h>

#include "common.h"
#include "mm_io.h"

using namespace std;
using namespace libmatroska;

#define isequal(s) (*(s) == '=')
#define iscolon(s) (*(s) == ':')
#define isdot(s) (*(s) == '.')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (istwodigits(s) && isdigit(*(s + 2)))
#define ischapter(s) (!strncmp("CHAPTER", (s), 7))
#define isname(s) (!strncmp("NAME", (s), 4))
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
#define ischapternameline(s) ((strlen(s) >= 15) && \
                          ischapter(s) && \
                          istwodigits(s + 7) && \
                          isname(s + 9) && \
                          isequal(s + 13) && \
                          !isspace(*(s + 14)))

static void chapter_error(const char *fmt, ...) {
  va_list ap;

  mxprint(stderr, "Error parsing chapters: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  mxprint(stderr, "\n");
  exit(1);
}

static bool probe_simple_chapters(mm_text_io_c *in) {
  string line;

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

static KaxChapters *parse_simple_chapters(mm_text_io_c *in) {
  KaxChapters *chaps;
  KaxEditionEntry *edition;
  KaxChapterAtom *atom;
  KaxChapterDisplay *display;
  int64_t start, hour, minute, second, msecs;
  string name, line;
  int mode;

  in->setFilePointer(0);
  chaps = new KaxChapters;

  mode = 0;
  atom = NULL;
  edition = NULL;

  while (in->getline2(line)) {
    strip(line);
    if (line.length() == 0)
      continue;

    if (mode == 0) {
      if (!ischapterline(line.c_str()))
        chapter_error("'%s' is not a CHAPTERxx=... line.", line.c_str());
      parse_int(line.substr(10, 2).c_str(), hour);
      parse_int(line.substr(13, 2).c_str(), minute);
      parse_int(line.substr(16, 2).c_str(), second);
      parse_int(line.substr(19, 3).c_str(), msecs);
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

      if (edition == NULL)
        edition = &GetChild<KaxEditionEntry>(*chaps);
      if (atom == NULL)
        atom = &GetChild<KaxChapterAtom>(*edition);
      else
        atom = &GetNextChild<KaxChapterAtom>(*edition, *atom);

      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterUID>(*atom)) =
        create_unique_uint32();
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeStart>(*atom)) =
        start;
      *static_cast<EbmlUInteger *>(&GetChild<KaxChapterTimeEnd>(*atom)) =
        start;                  // FIXME!
      display = &GetChild<KaxChapterDisplay>(*atom);
      *static_cast<EbmlUnicodeString *>
        (&GetChild<KaxChapterString>(*display)) =
        cstr_to_UTFstring(name.c_str());
      *static_cast<EbmlString *>(&GetChild<KaxChapterLanguage>(*display)) =
        "eng";
    }
  }

  delete in;

  return chaps;
}

static bool probe_xml_chapters(mm_text_io_c *) {
  return false;
}

static KaxChapters *parse_xml_chapters(mm_text_io_c *) {
  return NULL;
}

KaxChapters *parse_chapters(const char *file_name) {
  mm_text_io_c *in;

  try {
    in = new mm_text_io_c(file_name);
  } catch (...) {
    mxprint(stderr, "Error: Could not open '%s' for reading.\n", file_name);
    exit(1);
  }

  if (probe_simple_chapters(in))
    return parse_simple_chapters(in);

  if (probe_xml_chapters(in))
    return parse_xml_chapters(in);

  mxprint(stderr, "Error: Unknown file format for '%s'. It does not contain "
          "a supported chapter format.\n", file_name);
  delete in;
  exit(1);

  return NULL;
}

