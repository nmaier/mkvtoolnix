/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_microdvd.cpp
  MicroDVD text subtitle reader module

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"
#include "ogmstreams.h"
#include "queue.h"
#include "r_microdvd.h"
#include "subtitles.h"

int microdvd_reader_c::probe_file(FILE *file, int64_t size) {
  char chunk[2048];
  int i;
  
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if ((chunk[0] != '{') || !isdigit(chunk[1]))
    return 0;
  i = 2;
  while (isdigit(chunk[i]))
    i++;
  if ((chunk[i] != '}') || (chunk[i + 1] != '{'))
    return 0;
  i += 2;
  while (isdigit(chunk[i]))
    i++;
  if ((chunk[i] != '}') || (chunk[i + 1] == 0) || (chunk[i + 1] == '\n') ||
      (chunk[i + 1] == '\r'))
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  return 1;
}

microdvd_reader_c::microdvd_reader_c(char *fname, audio_sync_t *nasync,
                                     char **ncomments) throw (error_c) {
  if ((file = fopen(fname, "r")) == NULL)
    throw error_c("microdvd_reader: Could not open source file.");
  if (!microdvd_reader_c::probe_file(file, 0))
    throw error_c("microdvd_reader: Source is not a valid MicroDVD file.");
  textsubspacketizer = new textsubs_packetizer_c(nasync);
  if (verbose)
    fprintf(stdout, "Using MicroDVD subtitle reader for %s.\n+-> Using " \
            "text subtitle output module for subtitles.\n", fname);
}

microdvd_reader_c::~microdvd_reader_c() {
  if (textsubspacketizer != NULL)
    delete textsubspacketizer;
}

int microdvd_reader_c::read() {
  ogg_int64_t start, end;
  char *subtitles, *s, *s2;
  subtitles_c subs;
  int i, j, lineno;

  if (video_fps < 0.0) {
    fprintf(stderr, "Error: microdvd_reader: You have to add a video source "
            "before a MicroDVD source. Otherwise the frame numbers cannot be "
            "converted to time stamps.");
    exit(1);
  }
  
  lineno = 0;
  while (1) {
    if (fgets(chunk, 2047, file) == NULL)
      break;
    
    lineno++;
  
// {123}{4567}Some text|and even another line.
    
    if ((chunk[0] == 0) || (strchr("\n\r", chunk[0]) != NULL))
      continue;
    if ((chunk[0] != '{') || !isdigit(chunk[1])) {
      fprintf(stdout, "microdvd_reader: Warning: Unrecognized line %d. "
              "Ignored.\n", lineno);
      continue;
    }
    i = 2;
    while (isdigit(chunk[i]))
      i++;
    if ((chunk[i] != '}') || (chunk[i + 1] != '{')) {
      fprintf(stdout, "microdvd_reader: Warning: Unrecognized line %d. "
              "Ignored.\n", lineno);
      continue;
    }
    chunk[i] = 0;
    start = strtol(&chunk[1], NULL, 10);
    if ((start < 0) || (errno != 0)) {
      fprintf(stdout, "microdvd_reader: Warning: Bad start frame '%s' on line "
              "%d. Ignored.\n", &chunk[1], lineno);
      continue;
    }
    i += 2;
    j = i;
    while (isdigit(chunk[i]))
      i++;
    if ((chunk[i] != '}') || (chunk[i + 1] == 0)) {
      fprintf(stdout, "microdvd_reader: Warning: Unrecognized line %d. "
              "Ignored.\n", lineno);
      continue;
    }
    if ((chunk[i + 1] == '\n') || (chunk[i + 1] == '\r'))
      continue;
    chunk[i] = 0;
    end = strtol(&chunk[j], NULL, 10);
    if ((end < 0) || (errno != 0)) {
      fprintf(stdout, "microdvd_reader: Warning: Bad end frame '%s' on line "
              "%d. Ignored.\n", &chunk[j], lineno);
      continue;
    }
    start = (ogg_int64_t)(start * 1000 / video_fps);
    end = (ogg_int64_t)(end * 1000 / video_fps);

    subtitles = NULL;
    s = &chunk[i + 1];
    s2 = &s[strlen(s) - 1];
    while ((s2 != s) && ((*s2 == '\n') || (*s2 == '\r'))) {
      *s2 = 0;
      s2--;
    }
    if (*s == 0) {
      fprintf(stdout, "microdvd_reader: Warning: Unrecognized line %d. "
              "Ignored.\n", lineno);
      continue;
    }
    while (s != NULL) {
      s2 = strchr(s, '|');
      if (s2 != NULL)
        *s2 = 0;
      if (subtitles == NULL) {
        subtitles = safestrdup(s);
      } else {
        subtitles = (char *)saferealloc(subtitles, strlen(s) + 2 +
                                        strlen(subtitles));
        sprintf(&subtitles[strlen(subtitles)], "\n%s", s);
      }
      if (s2 != NULL)
        s = &s2[1];
      else
        s = NULL;
    }
    if (subtitles != NULL) {
      subs.add(start, end, subtitles);
      safefree(subtitles);
      subtitles = NULL;
    }
  }

  if ((subs.check() != 0) && verbose)
    fprintf(stdout, "MicroDVD_reader: Warning: The subtitle file seems to be "
            "badly broken. The output file might not be playable "
            "correctly.\n");
  subs.process(textsubspacketizer);

  return 0;
}

int microdvd_reader_c::serial_in_use(int serial) {
  return textsubspacketizer->serial_in_use(serial);
}

ogmmerge_page_t *microdvd_reader_c::get_header_page(int header_type) {
  return textsubspacketizer->get_header_page(header_type);
}

ogmmerge_page_t *microdvd_reader_c::get_page() {
  return textsubspacketizer->get_page();
}

int microdvd_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

void microdvd_reader_c::reset() {
  if (textsubspacketizer != NULL)
    textsubspacketizer->reset();
}

static char wchar[] = "-\\|/-\\|/-";

void microdvd_reader_c::display_progress() {
  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}
