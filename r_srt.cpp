/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_srt.cpp
  SRT text subtitle reader module

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
#include "r_srt.h"
#include "subtitles.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define iscolon(s) (*(s) == ':')
#define iscomma(s) (*(s) == ',')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (isdigit(*(s)) && isdigit(*(s + 1)) && \
                          isdigit(*(s + 2)))
#define isarrow(s) (!strncmp((s), " --> ", 5))
#define istimestamp(s) (istwodigits(s) && iscolon(s + 2) && \
                        istwodigits(s + 3) && iscolon(s + 5) && \
                        istwodigits(s + 6) && iscomma(s + 8) && \
                        isthreedigits(s + 9))
#define issrttimestamp(s) (istimestamp(s) && isarrow(s + 12) && \
                           istimestamp(s + 17))
                        
int srt_reader_c::probe_file(FILE *file, u_int64_t size) {
  char chunk[2048];
  
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if ((chunk[0] != '1') || ((chunk[1] != '\n') && (chunk[1] != '\r')))
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if ((strlen(chunk) < 29) ||  !issrttimestamp(chunk))
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  return 1;
}

srt_reader_c::srt_reader_c(char *fname, audio_sync_t *nasync,
                           range_t *nrange, char **ncomments) throw (error_c) {
  if ((file = fopen(fname, "r")) == NULL)
    throw error_c("srt_reader: Could not open source file.");
  if (!srt_reader_c::probe_file(file, 0))
    throw error_c("srt_reader: Source is not a valid SRT file.");
  textsubspacketizer = new textsubs_packetizer_c(nasync, nrange, ncomments);
  if (verbose)
    fprintf(stdout, "Using SRT subtitle reader for %s.\n+-> Using " \
            "text subtitle output module for subtitles.\n", fname);
}

srt_reader_c::~srt_reader_c() {
  if (textsubspacketizer != NULL)
    delete textsubspacketizer;
}

int srt_reader_c::read() {
  ogg_int64_t start, end;
  char *subtitles;
  subtitles_c subs;

  while (1) {  
    if (fgets(chunk, 2047, file) == NULL)
      break;
    if (fgets(chunk, 2047, file) == NULL)
      break;
    if ((strlen(chunk) < 29) ||  !issrttimestamp(chunk))
      break;
  
// 00:00:00,000 --> 00:00:00,000
// 01234567890123456789012345678
//           1         2  
    chunk[2] = 0;
    chunk[5] = 0;
    chunk[8] = 0;
    chunk[12] = 0;
    chunk[19] = 0;
    chunk[22] = 0;
    chunk[25] = 0;
    chunk[29] = 0;
    
    start = atol(chunk) * 3600000 + atol(&chunk[3]) * 60000 +
            atol(&chunk[6]) * 1000 + atol(&chunk[9]);
    end = atol(&chunk[17]) * 3600000 + atol(&chunk[20]) * 60000 +
          atol(&chunk[23]) * 1000 + atol(&chunk[26]);
    subtitles = NULL;
    while (1) {
      if (fgets(chunk, 2047, file) == NULL)
        break;
      if ((*chunk == '\n') || (*chunk == '\r'))
        break;
      if (subtitles == NULL) {
        subtitles = strdup(chunk);
        if (subtitles == NULL)
          die("malloc");
      } else {
        subtitles = (char *)realloc(subtitles, strlen(chunk) + 1 +
                                    strlen(subtitles));
        if (subtitles == NULL)
          die("malloc");
        strcat(subtitles, chunk);
      }
    }
    if (subtitles != NULL) {
      subs.add(start, end, subtitles);
      free(subtitles);
    }
  }

  if ((subs.check() != 0) && verbose)
    fprintf(stdout, "srt_reader: Warning: The subtitle file seems to be " \
            "badly broken. The output file might not be playable " \
            "correctly.\n");
  subs.process(textsubspacketizer);

  return 0;
}

int srt_reader_c::serial_in_use(int serial) {
  return textsubspacketizer->serial_in_use(serial);
}

ogmmerge_page_t *srt_reader_c::get_header_page(int header_type) {
  return textsubspacketizer->get_header_page(header_type);
}

ogmmerge_page_t *srt_reader_c::get_page() {
  return textsubspacketizer->get_page();
}

int srt_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

void srt_reader_c::reset() {
  if (textsubspacketizer != NULL)
    textsubspacketizer->reset();
}

static char wchar[] = "-\\|/-\\|/-";

void srt_reader_c::display_progress() {
  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}
