/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: pr_generic.cpp,v 1.19 2003/04/18 10:08:24 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <malloc.h>

#include "common.h"
#include "pr_generic.h"

generic_packetizer_c::generic_packetizer_c(track_info_t *nti) {
  serialno = -1;
  track_entry = NULL;
  ti = duplicate_track_info(nti);
  free_refs = -1;
}

generic_packetizer_c::~generic_packetizer_c() {
  free_track_info(ti);
}

void generic_packetizer_c::set_free_refs(int64_t nfree_refs) {
  free_refs = nfree_refs;
}

int64_t generic_packetizer_c::get_free_refs() {
  return free_refs;
}

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c(track_info_t *nti) {
  ti = duplicate_track_info(nti);
}

generic_reader_c::~generic_reader_c() {
  free_track_info(ti);
}

//--------------------------------------------------------------------

track_info_t *duplicate_track_info(track_info_t *src) {
  track_info_t *dst;

  if (src == NULL)
    return NULL;

  dst = (track_info_t *)malloc(sizeof(track_info_t));
  if (dst == NULL)
    die("malloc");

  memcpy(dst, src, sizeof(track_info_t));
  if (src->fname != NULL) {
    dst->fname = strdup(src->fname);
    if (dst->fname == NULL)
      die("strdup");
  }
  if (src->atracks != NULL) {
    dst->atracks = (unsigned char *)strdup((char *)src->atracks);
    if (dst->atracks == NULL)
      die("strdup");
  }
  if (src->vtracks != NULL) {
    dst->vtracks = (unsigned char *)strdup((char *)src->vtracks);
    if (dst->vtracks == NULL)
      die("strdup");
  }
  if (src->stracks != NULL) {
    dst->stracks = (unsigned char *)strdup((char *)src->stracks);
    if (dst->stracks == NULL)
      die("strdup");
  }
  if (src->private_data != NULL) {
    dst->private_data = (unsigned char *)malloc(src->private_size);
    if (dst->private_data == NULL)
      die("malloc");
    memcpy(dst->private_data, src->private_data, src->private_size);
  }

  return dst;
}

void free_track_info(track_info_t *ti) {
  if (ti == NULL)
    return;

  if (ti->fname != NULL)
    free(ti->fname);
  if (ti->atracks != NULL)
    free(ti->atracks);
  if (ti->vtracks != NULL)
    free(ti->vtracks);
  if (ti->stracks != NULL)
    free(ti->stracks);
  if (ti->private_data != NULL)
    free(ti->private_data);

  free(ti);
}

