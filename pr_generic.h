/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: pr_generic.h,v 1.21 2003/04/18 10:28:14 mosu Exp $
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

#include "error.h"
#include "queue.h"

using namespace LIBMATROSKA_NAMESPACE;

typedef struct {
  int    displacement;
  double linear;
} audio_sync_t;

typedef struct {
  // Options used by the readers.
  char *fname;
  unsigned char *atracks, *vtracks, *stracks;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;

  char fourcc[5];

  audio_sync_t async;
} track_info_t;

class generic_packetizer_c: public q_c {
protected:
  int serialno;
  track_info_t *ti;
  int64_t free_refs;
public:
  KaxTrackEntry *track_entry;

  generic_packetizer_c(track_info_t *nti) throw (error_c);
  virtual ~generic_packetizer_c();
  virtual void      set_free_refs(int64_t nfree_refs);
  virtual int64_t   get_free_refs();
  virtual void      set_header() = 0;
  virtual int       process(unsigned char *data, int size,
                            int64_t timecode = -1, int64_t length = -1) = 0;
};
 
class generic_reader_c {
protected:
  track_info_t *ti;
public:
  generic_reader_c(track_info_t *nti);
  virtual ~generic_reader_c();
  virtual int       read() = 0;
  virtual packet_t *get_packet() = 0;
  virtual int       display_priority() = 0;
  virtual void      display_progress() = 0;
};

track_info_t *duplicate_track_info(track_info_t *src);
void free_track_info(track_info_t *ti);

#endif  // __PR_GENERIC_H
