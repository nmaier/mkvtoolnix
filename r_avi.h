/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_avi.h
  class definitions for the AVI demultiplexer module

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __R_AVI_H__
#define __R_AVI_H__

#include <stdio.h>

#include <ogg/ogg.h>

extern "C" {
#include <avilib.h>
}

#include "ogmmerge.h"
#include "queue.h"

#include "p_video.h"

#define RAVI_UNKNOWN 0
#define RAVI_DIVX3   1
#define RAVI_MPEG4   2

typedef struct avi_demuxer_t {
  generic_packetizer_c *packetizer;
  int                   channels, bits_per_sample, samples_per_second;
  int                   aid;
  int                   eos;
  ogg_int64_t           bytes_processed;
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
                 range_t *nrange, char **ncomments, char *nfourcc)
                 throw (error_c);
    virtual ~avi_reader_c();

    virtual int              read();
    virtual int              serial_in_use(int);
    virtual ogmmerge_page_t *get_page();
    virtual ogmmerge_page_t *get_header_page(int header_type =
                                             PACKET_TYPE_HEADER);
    
    virtual void             reset();
    virtual int              display_priority();
    virtual void             display_progress();

    static int               probe_file(FILE *file, u_int64_t size);
    

  private:
    virtual int              add_audio_demuxer(avi_t *avi, int aid);
    virtual int              is_keyframe(unsigned char *data, long size,
                                         int suggestion);
};

#endif  /* __R_AVI_H__*/
