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

void subtitles_c::add(int64_t nstart, int64_t nend, const char *nsubs) {
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

int subtitles_c::check() {
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

void subtitles_c::process(textsubs_packetizer_c *p) {
  sub_t *current;

  while ((current = get_next()) != NULL) {
    p->process((unsigned char *)current->subs, 0, current->start,
               current->end - current->start);
    safefree(current->subs);
    safefree(current);
  }
}

sub_t *subtitles_c::get_next() {
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
