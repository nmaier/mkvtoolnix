/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_generic.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_generic.h,v 1.2 2003/02/16 12:17:10 mosu Exp $
    \brief class definition for the generic packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __P_GENERIC_H__
#define __P_GENERIC_H__

#include "common.h"

typedef class generic_packetizer_c {
//  protected:
//    int             serialno;
//    vorbis_comment *comments;
  public:
    generic_packetizer_c() {};
    virtual ~generic_packetizer_c() {};
    virtual int              packet_available() = 0;
    virtual packet_t        *get_packet() = 0;
//    virtual int              serial_in_use(int serial);
    virtual stamp_t          get_smallest_timestamp() = 0;
//    virtual void             produce_header_packets() = 0;
} generic_packetizer_c;
 
#endif // __P_GENERIC_H__
