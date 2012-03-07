/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the packet extensions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __PACKET_EXTENSIONS_H
#define __PACKET_EXTENSIONS_H

#include "common/common_pch.h"

#include <deque>

#include "merge/packet.h"

class multiple_timecodes_packet_extension_c: public packet_extension_c {
protected:
  std::deque<int64_t> m_timecodes;
  std::deque<int64_t> m_positions;

public:
  multiple_timecodes_packet_extension_c() {
  }

  virtual ~multiple_timecodes_packet_extension_c() {
  }

  virtual packet_extension_type_e get_type() {
    return MULTIPLE_TIMECODES;
  }

  inline void add(int64_t timecode, int64_t position) {
    m_timecodes.push_back(timecode);
    m_positions.push_back(position);
  }

  inline bool empty() {
    return m_timecodes.empty();
  }

  inline bool get_next(int64_t &timecode, int64_t &position) {
    if (m_timecodes.empty())
      return false;

    timecode = m_timecodes.front();
    position = m_positions.front();

    m_timecodes.pop_front();
    m_positions.pop_front();

    return true;
  }
};

typedef counted_ptr<multiple_timecodes_packet_extension_c> multiple_timecodes_packet_extension_cptr;

class subtitle_number_packet_extension_c: public packet_extension_c {
private:
  unsigned int m_number;

public:
  subtitle_number_packet_extension_c(unsigned int number)
    : m_number(number)
  {
  }

  virtual packet_extension_type_e get_type() {
    return SUBTITLE_NUMBER;
  }

  unsigned int get_number() {
    return m_number;
  }
};

typedef counted_ptr<subtitle_number_packet_extension_c> subtitle_number_packet_extension_cptr;

#endif  // __PACKET_EXTENSIONS_H
