/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   random number generating functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if !defined(SYS_WINDOWS)
# include <sys/time.h>
# include <time.h>
#else
# include <windows.h>
#endif

#include "common/mm_io.h"
#include "common/random.h"

bool random_c::m_seeded = false;

#if defined(SYS_WINDOWS)

bool random_c::m_tried_uuidcreate = false;
bool random_c::m_use_uuidcreate   = false;

void
random_c::generate_bytes(void *destination,
                         size_t num_bytes) {
  UUID uuid;

  if (!m_seeded) {
    srand(GetTickCount());
    m_seeded = true;
  }

  if (!m_tried_uuidcreate) {
    // Find out whether UuidCreate returns different
    // data in Data4 on each call by comparing up to five
    // results. If not use srand() and rand().
    UUID first_uuid;
    int i;

    m_use_uuidcreate = true;
    RPC_STATUS status = UuidCreate(&first_uuid);
    if ((RPC_S_OK == status) || (RPC_S_UUID_LOCAL_ONLY == status)) {
      for (i = 0; i < 5; ++i) {
        status = UuidCreate(&uuid);
        if (((RPC_S_OK != status) && (RPC_S_UUID_LOCAL_ONLY != status)) ||
            !memcmp(first_uuid.Data4, uuid.Data4, sizeof(uuid.Data4))) {
          m_use_uuidcreate = false;
          break;
        }
      }
    } else
      m_use_uuidcreate = false;

    m_tried_uuidcreate = true;
  }

  size_t num_written = 0;
  while (num_written < num_bytes) {
    if (m_use_uuidcreate) {
      RPC_STATUS status = UuidCreate(&uuid);
      if ((RPC_S_OK != status) && (RPC_S_UUID_LOCAL_ONLY != status)) {
        m_use_uuidcreate = false;
        continue;
      }

      int num_left = num_bytes - num_written;
      if (num_left > 8)
        num_left = 8;
      memcpy((unsigned char *)destination + num_written, &uuid.Data4, num_left);
      num_written += num_left;

    } else
      for (; num_written < num_bytes; ++num_written)
        ((unsigned char *)destination)[num_written] = (unsigned char)(256.0 * rand() / (RAND_MAX + 1.0));
  }
}

#else  // defined(SYS_WINDOWS)

mm_io_cptr random_c::m_dev_urandom;
bool random_c::m_tried_dev_urandom = false;

void
random_c::generate_bytes(void *destination,
                         size_t num_bytes) {
  try {
    if (!m_tried_dev_urandom) {
      m_tried_dev_urandom = true;
      m_dev_urandom       = mm_file_io_c::open("/dev/urandom");
    }
    if (m_dev_urandom.is_set() && (m_dev_urandom->read(destination, num_bytes) == num_bytes))
      return;
  } catch(...) {
  }

  if (!m_seeded) {
    struct timeval tv;

    gettimeofday(&tv, nullptr);
    srand(tv.tv_usec + tv.tv_sec);
    m_seeded = true;
  }

  unsigned int i;
  for (i = 0; i < num_bytes; ++i)
    ((unsigned char *)destination)[i] = (unsigned char)(256.0 * rand() / (RAND_MAX + 1.0));
}

#endif // defined(SYS_WINDOWS)

#if defined(DEBUG)
void
random_c::test() {
  uint32_t ranges[16];
  const unsigned int num = 1000000;

  unsigned int i;
  for (i = 0; 16 > i; ++i)
    ranges[i] = 0;

  for (i = 0; num > i; ++i) {
    uint32_t n = random_c::generate_32bits();
    bool found = false;
    unsigned int k;
    for (k = 1; 15 >= k; ++k)
      if (n < (k * 0x10000000)) {
        ++ranges[k - 1];
        found = true;
        break;
      }
    if (!found)
      ++ranges[15];
  }

  for (i = 0; i < 16; i++)
    printf("%0d: %d (%.2f%%)\n", i, ranges[i], (double)ranges[i] * 100.0 / num);

#if !defined(SYS_WINDOWS)
  m_tried_dev_urandom = true;
  m_dev_urandom.clear();

  for (i = 0; 16 > i; i++)
    ranges[i] = 0;

  for (i = 0; num > i; i++) {
    uint32_t n = random_c::generate_32bits();
    bool found = false;
    unsigned int k;
    for (k = 1; 15 >= k; ++k)
      if (n < (k * 0x10000000)) {
        ++ranges[k - 1];
        found = true;
        break;
      }
    if (!found)
      ++ranges[15];
  }

  for (i = 0; i < 16; i++)
    printf("%0d: %d (%.2f%%)\n", i, ranges[i], (double)ranges[i] * 100.0 / num);
#endif

  exit(0);
}
#endif  // defined(DEBUG)

void
random_c::cleanup() {
#if !defined(SYS_WINDOWS)
  m_dev_urandom.clear();
#endif
}
