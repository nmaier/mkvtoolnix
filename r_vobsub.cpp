/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_vobsub.cpp
  VobSub text subtitle reader module

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
#include "r_vobsub.h"
#include "subtitles.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define istimecodestr(s)      (!strncmp(s, "timecode: ", 11))
#define iscommafileposstr(s)   (!strncmp(s, ", filepos: ", 11))
#define iscolon(s)             (*(s) == ':')
#define istwodigits(s)         (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s)       (isdigit(*(s)) && isdigit(*(s + 1)) && \
                                isdigit(*(s + 2)))
#define istwodigitscolon(s)    (istwodigits(s) && iscolon(s + 2))
#define istimecode(s)         (istwodigitscolon(s) && \
                                istwodigitscolon(s + 3) && \
                                istwodigitscolon(s + 6) && \
                                isthreedigits(s + 9))
#define ishexdigit(s)          (isdigit(s) || \
                                (strchr("abcdefABCDEF", s) != NULL))
#define isfilepos(s)           (ishexdigit(*(s)) && ishexdigit(*(s + 1)) && \
                                ishexdigit(*(s + 2)) && \
                                ishexdigit(*(s + 3)) && \
                                ishexdigit(*(s + 4)) && \
                                ishexdigit(*(s + 5)) && \
                                ishexdigit(*(s + 6)) && \
                                ishexdigit(*(s + 7)) && \
                                ishexdigit(*(s + 8)))
#define isvobsubline(s)        (istimecodestr(s) && istimecode(s + 11) && \
                                iscommafileposstr(s + 23) && \
                                isfilepos(s + 34))
                        
int vobsub_reader_c::probe_file(FILE *file, int64_t size) {
  char chunk[2048];
  
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  if (fgets(chunk, 2047, file) == NULL)
    return 0;
  if (strncmp(chunk, "# VobSub index file, v7",
              strlen("# VobSub index file, v7")))
    return 0;
  if (fseek(file, 0, SEEK_SET) != 0)
    return 0;
  return 1;
}

vobsub_reader_c::vobsub_reader_c(char *fname, audio_sync_t *nasync)
  throw (error_c) {
  char *name;
  
  if ((file = fopen(fname, "r")) == NULL)
    throw error_c("vobsub_reader: Could not open source file.");
  if (!vobsub_reader_c::probe_file(file, 0))
    throw error_c("vobsub_reader: Source is not a valid VobSub index file.");

  name = strdup(fname);
  if (name == NULL)
    die("strdup");
  if ((strlen(name) > 4) && (name[strlen(name) - 4] == '.'))
    name[strlen(name) - 4] = 0;
  else {
    name = (char *)realloc(name, strlen(name) + 5);
    if (name == NULL)
      die("realloc");
  }
  strcat(name, ".sub");
  if ((subfile = fopen(name, "r")) == NULL)
    throw error_c("vobsub_reader: Could not open the sub file.");

  vobsub_packetizer = NULL;
  all_packetizers = NULL;
  num_packetizers = 0;
  if (verbose)
    fprintf(stdout, "Using VobSub subtitle reader for %s/%s.\n+-> Using " \
            "VobSub subtitle output module for subtitles.\n", fname, name);
  free(name);
  memcpy(&async, nasync, sizeof(audio_sync_t));
  if (ncomments == NULL)
    comments = ncomments;
  else
    comments = dup_comments(ncomments);
}

vobsub_reader_c::~vobsub_reader_c() {
  int i;
  for (i = 0; i < num_packetizers; i++)
    if (all_packetizers[i] != NULL)
      delete all_packetizers[i];
  if (comments != NULL)
    free_comments(comments);
}

void vobsub_reader_c::add_vobsub_packetizer(int width, int height,
                                            char *palette, int langidx,
                                            char *id, int index) {
  all_packetizers = (vobsub_packetizer_c **)realloc(all_packetizers,
                                                    (num_packetizers + 1) *
                                                    sizeof(void *));
  if (all_packetizers == NULL)
    die("realloc");
  try {
    vobsub_packetizer = new vobsub_packetizer_c(width, height, palette,
                                                langidx, id, index,
                                                &async);
  } catch (error_c error) {
    fprintf(stderr, "Error: vobsub_reader: Could not create a new "
            "vobsub_packetizer: %s\n", error.get_error());
    exit(1);
  }
  all_packetizers[num_packetizers] = vobsub_packetizer;
  num_packetizers++;
}

int vobsub_reader_c::read() {
  ogg_int64_t start, filepos, last_start, last_filepos;
  char *s, *s2;
  int width = -1, height = -1;
  char *palette = NULL;
  int langidx = -1;
  char *id = NULL;
  int index = -1;
  int lineno;

  chunk[2047] = 0;
  lineno = 0;
  last_start = -1;
  last_filepos = -1;
  while (1) {  
    if (fgets(chunk, 2047, file) == NULL)
      break;
    lineno++;
    if ((*chunk == 0) || (strchr("#\n\r", *chunk) != NULL))
      continue;
    if (!strncmp(chunk, "size: ", 6)) {
      if (sscanf(&chunk[6], "%dx%d", &width, &height) != 2) {
        width = -1;
        height = -1;
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"size:\" entry "
                "on line %d. Ignored.\n", lineno);
      }
    } else if (!strncmp(chunk, "palette: ", 9)) {
      if (strlen(chunk) < 10)
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"palette:\" entry "
                "on line %d. Ignored.\n", lineno);
      else {
        palette = strdup(&chunk[9]);
        if (palette == NULL)
          die("strdup");
      }
    } else if (!strncmp(chunk, "langidx: ", 9)) {
      langidx = strtol(&chunk[9], NULL, 10);
      if ((langidx < 0) || (errno != 0)) {
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"langidx:\" entry "
                "on line %d. Ignored.\n", lineno);
        langidx = -1;
      }
    } else if (!strncmp(chunk, "id:", 3)) {
      s = &chunk[3];
      while (isspace(*s))
        s++;
      s2 = strchr(s, ',');
      if (s2 == NULL) {
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"id:\" entry "
                "on line %d. Ignored.\n", lineno);
      
        continue;
      }
      *s2 = 0;
      id = strdup(s);
      if (id == NULL)
        die("strdup");
      s = s2 + 1;
      while (isspace(*s))
        s++;
      if (strncmp(s, "index:", 6)) {
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"id:\" entry "
                "on line %d. Ignored.\n", lineno);
      
        continue;
      }
      s += 6;
      while (isspace(*s))
        s++;
      index = strtol(s, NULL, 10);
      if ((index < 0) || (errno != 0)) {
        fprintf(stdout, "vobsub_reader: Warning: Incorrect \"id:\" entry "
                "on line %d. Ignored.\n", lineno);
        continue;
      }
    } else if (!isvobsubline(chunk))
      fprintf(stdout, "vobsub_reader: Warning: Unknown line format on line "
              "%d. Ignored.\n", lineno);
    else if (vobsub_packetizer == NULL) {
      if ((width == -1) || (height == -1)) {
        fprintf(stdout, "vobsub_reader: No \"size:\" entry found. File seems "
                "to be defect. Aborting.\n");
        exit(1);
      }
      if (palette == NULL) {
        fprintf(stdout, "vobsub_reader: No \"palette:\" entry found. File "
                "seems to be defect. Aborting.\n");
        exit(1);
      }
      if (langidx == -1) {
        fprintf(stdout, "vobsub_reader: No \"langidx:\" entry found. File "
                "seems to be defect. Aborting.\n");
        exit(1);
      }
      if ((id == NULL) || (index == -1)) {
              fprintf(stdout, "vobsub_reader: No \"id:\" entry found. File "
                "seems to be defect. Aborting.\n");
        exit(1);
      }
      add_vobsub_packetizer(width, height, palette, langidx, id, index);
      width = -1;
      height = -1;
      if (palette != NULL) {
        free(palette);
        palette = NULL;
      }
      langidx = -1;
      if (id != NULL) {
        free(id);
        id = NULL;
      }
      index = -1;
    } else {
// timecode: 00:00:03:440, filepos: 000000000
// 0123456789012345678901234567890123456789012
//           1         2         3         4
      chunk[13] = 0;
      chunk[16] = 0;
      chunk[19] = 0;
      chunk[23] = 0;
      
      start = atol(&chunk[11]) * 3600000 + atol(&chunk[14]) * 60000 +
              atol(&chunk[17]) * 1000 + atol(&chunk[20]);
      filepos = strtoll(&chunk[34], NULL, 16);

      if ((last_start != -1) && (last_filepos != -1)) {
        if (fseek(subfile, last_filepos, SEEK_SET) != 0)
          fprintf(stderr, "Warning: vobsub_reader: Could not seek to position "
                  "%lld. Ignoring this entry.\n", last_filepos);
        else if (last_filepos == filepos)
          fprintf(stderr, "Warning: vobsub_reader: This entry and the last "
                  "entry start at the same position in the file. Ignored.\n");
        else {
          s = (char *)malloc(filepos - last_filepos);
          if (s == NULL)
            die("malloc");
          if (fread(s, 1, filepos - last_filepos, subfile) != 
              (filepos - last_filepos))
            fprintf(stderr, "Warning: vobsub_reader: Could not read entry "
                    "from the sub file. Ignored.\n");
          else
            vobsub_packetizer->process(last_start, start - last_start, s,
                                       filepos - last_filepos, 0);
          free(s);
        }
      }
      last_start = start;
      last_filepos = filepos;
      
      fprintf(stdout, "line %d, start %lld, filepos %lld\n", lineno,
              start, filepos);
    }
  }
  if ((last_start != -1) && (last_filepos != -1) &&
      (vobsub_packetizer != NULL)) {
    if (fseek(subfile, 0, SEEK_END) != 0) {
      fprintf(stderr, "Warning: vobsub_reader: Could not seek to end of "
              "the sub file. Ignoring last entry.\n");
      vobsub_packetizer->produce_eos_packet();
      return 0;
    }
    filepos = ftell(subfile);
    if (fseek(subfile, last_filepos, SEEK_SET) != 0)
      fprintf(stderr, "Warning: vobsub_reader: Could not seek to position "
              "%lld. Ignoring this entry.\n", last_filepos);
    else if (last_filepos == filepos)
      fprintf(stderr, "Warning: vobsub_reader: This entry and the last "
              "entry start at the same position in the file. Ignored.\n");
    else {
      s = (char *)malloc(filepos - last_filepos);
      if (s == NULL)
        die("malloc");
      if (fread(s, 1, filepos - last_filepos, subfile) != 
          (filepos - last_filepos))
        fprintf(stderr, "Warning: vobsub_reader: Could not read entry "
                "from the sub file. Ignored.\n");
      else
        vobsub_packetizer->process(last_start, start - last_start, s,
                                   filepos - last_filepos, 1);
      free(s);
    }
  }

  return 0;
}

int vobsub_reader_c::serial_in_use(int serial) {
//  return vobsubpacketizer->serial_in_use(serial);
  return 0;
}

ogmmerge_page_t *vobsub_reader_c::get_header_page(int header_type) {
//  return vobsubpacketizer->get_header_page(header_type);
  return NULL;
}

ogmmerge_page_t *vobsub_reader_c::get_page() {
//  return vobsubpacketizer->get_page();
  return NULL;
}

int vobsub_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

void vobsub_reader_c::reset() {
//  if (vobsubpacketizer != NULL)
//    vobsubpacketizer->reset();
}

static char wchar[] = "-\\|/-\\|/-";

void vobsub_reader_c::display_progress() {
  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}
