/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  subtitles.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief subtitle helper
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <typeinfo>

#include "mkvmerge.h"
#include "common.h"
#include "subtitles.h"

subtitles_c::subtitles_c() {
  first = NULL;
  last = NULL;
}

subtitles_c::~subtitles_c() {
  sub_t *current = first;

  while (current != NULL) {
    if (current->subs != NULL)
      safefree(current->subs);
    last = current;
    current = current->next;
    safefree(last);
  }
}

void
subtitles_c::add(int64_t nstart,
                 int64_t nend,
                 const char *nsubs) {
  sub_t *s;

  s = (sub_t *)safemalloc(sizeof(sub_t));
  s->subs = safestrdup(nsubs);
  s->start = nstart;
  s->end = nend;
  s->next = NULL;

  if (last == NULL) {
    first = s;
    last = s;
  } else {
    last->next = s;
    last = s;
  }
}

int
subtitles_c::check() {
  sub_t *current;
  int error = 0;
  char *c;

  current = first;
  while ((current != NULL) && (current->next != NULL)) {
    if (current->end > current->next->start) {
      if (verbose) {
        char short_subs[21];

        memset(short_subs, 0, 21);
        strncpy(short_subs, current->subs, 20);
        for (c = short_subs; *c != 0; c++)
          if (*c == '\n')
            *c = ' ';
        mxwarn("subtitles: current entry ends after "
               "the next one starts. This end: %02lld:%02lld:%02lld,%03lld"
               "  next start: %02lld:%02lld:%02lld,%03lld  (\"%s\"...)\n",
               current->end / (60 * 60 * 1000),
               (current->end / (60 * 1000)) % 60,
               (current->end / 1000) % 60,
               current->end % 1000,
               current->next->start / (60 * 60 * 1000),
               (current->next->start / (60 * 1000)) % 60,
               (current->next->start / 1000) % 60,
               current->next->start % 1000,
               short_subs);
      }
      current->end = current->next->start - 1;
    }
    current = current->next;
  }

  current = first;
  while (current != NULL) {
    if (current->start > current->end) {
      error = 1;
      if (verbose) {
        char short_subs[21];

        memset(short_subs, 0, 21);
        strncpy(short_subs, current->subs, 20);
        for (c = short_subs; *c != 0; c++)
          if (*c == '\n')
            *c = ' ';
        mxwarn("subtitles: after fixing the time the "
               "current entry begins after it ends. This start: "
               "%02lld:%02lld:%02lld,%03lld  this end: %02lld:%02lld:"
               "%02lld,%03lld  (\"%s\"...)\n",
               current->start / (60 * 60 * 1000),
               (current->start / (60 * 1000)) % 60,
               (current->start / 1000) % 60,
               current->start % 1000,
               current->end / (60 * 60 * 1000),
               (current->end / (60 * 1000)) % 60,
               (current->end / 1000) % 60,
               current->end % 1000,
               short_subs);
       }
    }
    current = current->next;
  }

  return error;
}

void
subtitles_c::process(textsubs_packetizer_c *p) {
  sub_t *current;

  while ((current = get_next()) != NULL) {
    memory_c mem((unsigned char *)current->subs, 0, true);
    p->process(mem, current->start, current->end - current->start);
    safefree(current);
  }
}

sub_t *
subtitles_c::get_next() {
  sub_t *current;

  if (first == NULL)
    return NULL;

  current = first;
  if (first == last) {
    first = NULL;
    last = NULL;
  } else
    first = first->next;

  return current;
}

#undef PFX
#define PFX "spu_extract_duration: "

int64_t
spu_extract_duration(unsigned char *data,
                     int buf_size,
                     int64_t timecode) {
  uint32_t date, control_start, next_off, start_off, off;
  unsigned char type;
  int duration;
  bool unknown;

  control_start = get_uint16_be(data + 2);
  next_off = control_start;
  duration = -1;
  start_off = 0;

  while ((start_off != next_off) && (next_off < buf_size)) {
    start_off = next_off;
    date = get_uint16_be(data + start_off) * 1024;
    next_off = get_uint16_be(data + start_off + 2);
    if (next_off < start_off) {
      mxwarn(PFX "Encountered broken SPU packet (next_off < start_off) at "
             "timecode " FMT_TIMECODE ". This packet might be displayed "
             "incorrectly or not at all.\n", ARG_TIMECODE_NS(timecode));
      return -1;
    }
    mxverb(4, PFX "date = %u\n", date);
    off = start_off + 4;
    for (type = data[off++]; type != 0xff; type = data[off++]) {
      mxverb(4, PFX "cmd = %d ", type);
      unknown = false;
      switch(type) {
        case 0x00:
          /* Menu ID, 1 byte */
          mxverb(4, "menu ID");
          break;
        case 0x01:
          /* Start display */
          mxverb(4, "start display");
          break;
        case 0x02:
          /* Stop display */
          mxverb(4, "stop display: %u", date / 90);
          return (int64_t)date * 1000000 / 90;
          break;
        case 0x03:
          /* Palette */
          mxverb(4, "palette");
          off+=2;
          break;
        case 0x04:
          /* Alpha */
          mxverb(4, "alpha");
          off+=2;
          break;
        case 0x05:
          mxverb(4, "coords");
          off+=6;
          break;
        case 0x06:
          mxverb(4, "graphic lines");
          off+=4;
          break;
        case 0xff:
          /* All done, bye-bye */
          mxverb(4, "done");
          return duration;
        default:
          mxverb(4, "unknown (0x%02x), skipping %d bytes.", type,
                 next_off - off);
          unknown = true;
      }
      mxverb(4, "\n");
      if (unknown)
        break;
    }
  }
  return duration;
}
