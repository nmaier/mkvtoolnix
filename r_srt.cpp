/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_srt.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: r_srt.cpp,v 1.9 2003/05/04 10:05:41 mosu Exp $
    \brief Subripper subtitle reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "pr_generic.h"
#include "r_srt.h"
#include "subtitles.h"

#define iscolon(s) (*(s) == ':')
#define iscomma(s) (*(s) == ',')
#define istwodigits(s) (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s) (isdigit(*(s)) && isdigit(*(s + 1)) && \
                          isdigit(*(s + 2)))
#define isarrow(s) (!strncmp((s), " --> ", 5))
#define istimecode(s) (istwodigits(s) && iscolon(s + 2) && \
                        istwodigits(s + 3) && iscolon(s + 5) && \
                        istwodigits(s + 6) && iscomma(s + 8) && \
                        isthreedigits(s + 9))
#define issrttimecode(s) (istimecode(s) && isarrow(s + 12) && \
                           istimecode(s + 17))
                        
int srt_reader_c::probe_file(FILE *file, int64_t size) {
  char chunk[2048];
  
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if ((chunk[0] != '1') || ((chunk[1] != '\n') && (chunk[1] != '\r')))
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if ((strlen(chunk) < 29) ||  !issrttimecode(chunk))
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  return 1;
}

srt_reader_c::srt_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  if ((file = fopen(ti->fname, "r")) == NULL)
    throw error_c("srt_reader: Could not open source file.");
  if (!srt_reader_c::probe_file(file, 0))
    throw error_c("srt_reader: Source is not a valid SRT file.");
  textsubs_packetizer = new textsubs_packetizer_c(ti);
  if (verbose)
    fprintf(stdout, "Using SRT subtitle reader for %s.\n+-> Using " \
            "text subtitle output module for subtitles.\n", ti->fname);
}

srt_reader_c::~srt_reader_c() {
  if (textsubs_packetizer != NULL)
    delete textsubs_packetizer;
}

int srt_reader_c::read() {
  int64_t start, end;
  char *subtitles;
  subtitles_c subs;

  while (1) {  
    if (fgets(chunk, 2047, file) == NULL)
      break;
    if (fgets(chunk, 2047, file) == NULL)
      break;
    if ((strlen(chunk) < 29) ||  !issrttimecode(chunk))
      break;
    chunk[2047] = 0;

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
      chunk[2047] = 0;
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
  subs.process(textsubs_packetizer);

  return 0;
}

packet_t *srt_reader_c::get_packet() {
  return textsubs_packetizer->get_packet();
}

int srt_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void srt_reader_c::display_progress() {
  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void srt_reader_c::set_headers() {
  textsubs_packetizer->set_headers();
}
