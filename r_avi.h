/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_avi.h
    \version \$Id: r_avi.h,v 1.5 2003/02/16 17:04:39 mosu Exp $
    \brief class definitions for the AVI demultiplexer module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __R_AVI_H
#define __R_AVI_H

#include <stdio.h>

extern "C" {
#include <avilib.h>
}

#include "pr_generic.h"
#include "common.h"
#include "error.h"
#include "p_video.h"

#define RAVI_UNKNOWN 0
#define RAVI_DIVX3   1
#define RAVI_MPEG4   2

typedef struct avi_demuxer_t {
  generic_packetizer_c *packetizer;
  int                   channels, bits_per_sample, samples_per_second;
  int                   aid;
  int                   eos;
  u_int64_t             bytes_processed;
  avi_demuxer_t        *next;
} avi_demuxer_t;

class avi_reader_c: public generic_reader_c {
  private:
    char               *chunk;
    avi_t              *avi;
    video_packetizer_c *vpacketizer;
    avi_demuxer_t      *ademuxers;
    double              fps;
    int                 frames;
    unsigned char      *astreams, *vstreams;
    char              **comments;
    int                 max_frame_size;
    int                 act_wchar;
    audio_sync_t        async;
    range_t             range;
    char               *old_chunk;
    int                 old_key, old_nread;
    int                 video_done, maxframes;
    int                 is_divx, rederive_keyframes;
     
  public:
    avi_reader_c(char *fname, unsigned char *astreams,
                 unsigned char *vstreams, audio_sync_t *nasync,
                 range_t *nrange, char *nfourcc) throw (error_c);
    virtual ~avi_reader_c();

    virtual int       read();
    virtual packet_t *get_packet();
    virtual int       display_priority();
    virtual void      display_progress();

    static int        probe_file(FILE *file, u_int64_t size);
    

  private:
    virtual int       add_audio_demuxer(avi_t *avi, int aid);
    virtual int       is_keyframe(unsigned char *data, long size,
                                  int suggestion);
};

#endif  // __R_AVI_H
