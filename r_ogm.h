/*
  ogmmerge -- utility for splicing together ogg bitstreams
      from component media subtypes

  r_ogm.h
  class definitions for the OGG demultiplexer module

  Written by Moritz Bunkus <moritz@bunkus.org>
  Based on Xiph.org's 'oggmerge' found in their CVS repository
  See http://www.xiph.org

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __R_OGM_H__
#define __R_OGM_H__

#include <stdio.h>

#include <ogg/ogg.h>

#include "ogmmerge.h"
#include "queue.h"

#define OGM_STREAM_TYPE_UNKNOWN 0
#define OGM_STREAM_TYPE_VORBIS  1
#define OGM_STREAM_TYPE_VIDEO   2
#define OGM_STREAM_TYPE_PCM     3
#define OGM_STREAM_TYPE_MP3     4
#define OGM_STREAM_TYPE_AC3     5
#define OGM_STREAM_TYPE_TEXT    6

typedef struct ogm_demuxer_t {
  ogg_stream_state      os;
  generic_packetizer_c *packetizer;
  int                   sid;
  int                   stype;
  int                   eos;
  int                   serial;
  int                   units_processed;
  ogm_demuxer_t        *next;
} ogm_demuxer_t;

class ogm_reader_c: public generic_reader_c {
  private:
    ogg_sync_state    oy;
    unsigned char    *astreams, *vstreams, *tstreams;
    FILE             *file;
    char             *filename;
    int               act_wchar;
    ogm_demuxer_t    *sdemuxers;
    int               nastreams, nvstreams, ntstreams, numstreams;
    audio_sync_t      async;
    range_t           range;
    char            **comments;
    char             *fourcc;
    int               o_eos;
     
  public:
    ogm_reader_c(char *fname, unsigned char *astreams,
                 unsigned char *vstreams, unsigned char *tstreams,
                 audio_sync_t *nasync, range_t *nrange, char **ncomments,
                 char *nfourcc) throw (error_c);
    virtual ~ogm_reader_c();

    virtual int                   read();
    virtual int                   serial_in_use(int);
    virtual ogmmerge_page_t      *get_page();
    virtual ogmmerge_page_t      *get_header_page(int header_type =
                                                  PACKET_TYPE_HEADER);

    virtual void                  reset();
    virtual int                   display_priority();
    virtual void                  display_progress();
    
    virtual void                  overwrite_eos(int no_eos);
    virtual generic_packetizer_c *set_packetizer(generic_packetizer_c *np);
    
    static int                    probe_file(FILE *file, u_int64_t size);
    
  private:
    virtual ogm_demuxer_t        *find_demuxer(int serialno);
    virtual int                   demuxing_requested(unsigned char *, int);
    virtual void                  flush_packetizers();
    virtual int                   read_page(ogg_page *);
    virtual void                  add_new_demuxer(ogm_demuxer_t *);
    virtual int                   pages_available();
    virtual void                  handle_new_stream(ogg_page *);
    virtual void                  process_page(ogg_page *);
};


#endif  /* __R_OGM_H__*/
