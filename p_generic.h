/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_generic.h
  class definition for the generic packetizer

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

#ifndef __P_GENERIC_H__
#define __P_GENERIC_H__

typedef class generic_packetizer_c {
//  protected:
//    int             serialno;
//    vorbis_comment *comments;
  public:
    generic_packetizer_c() {};
    virtual ~generic_packetizer_c() {};
//    virtual int              page_available() = 0;
//    virtual stamp_t          make_timestamp(ogg_int64_t granulepos) = 0;
//    virtual int              serial_in_use(int serial);
//    virtual int              flush_pages(int header_page = 0) = 0;
//    virtual int              queue_pages(int header_page = 0) = 0;
//    virtual stamp_t          get_smallest_timestamp() = 0;
//    virtual void             produce_eos_packet() = 0;
//    virtual void             produce_header_packets() = 0;
//    virtual void             reset() = 0;
//    virtual void             set_comments(vorbis_comment *ncomments);
} generic_packetizer_c;
 
#endif // __P_GENERIC_H__
