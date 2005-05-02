/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   definitions for random number generating functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __RANDOM_H
#define __RANDOM_H

#include "os.h"

#include "mm_io.h"
#include "smart_pointers.h"

class MTX_DLL_API random_c {
private:
#if !defined(SYS_WINDOWS)
  static bool m_seeded;
  static auto_ptr<mm_file_io_c> m_dev_urandom;
  static bool m_tried_dev_urandom;
#endif

public:
  static void generate_bytes(void *destination, int num_bytes);

  static uint8_t generate_8bits() {
    uint8_t b;

    generate_bytes(&b, 1);
    return b;
  }

  static int16_t generate_15bits() {
    return (int16_t)(generate_16bits() & 0x7fff);
  }

  static uint16_t generate_16bits() {
    uint16_t b;

    generate_bytes(&b, 2);
    return b;
  }

  static int32_t generate_31bits() {
    return (int32_t)(generate_32bits() & 0x7fffffff);
  }

  static uint32_t generate_32bits() {
    uint32_t b;

    generate_bytes(&b, 4);
    return b;
  }

  static int64_t generate_63bits() {
    return (int64_t)(generate_64bits() & 0x7fffffffffffull);
  }

  static uint64_t generate_64bits() {
    uint64_t b;

    generate_bytes(&b, 8);
    return b;
  }

  static void test();
};

#endif // __RANDOM_H
