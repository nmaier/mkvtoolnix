/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG TS (transport stream) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <memory>

#include "common.h"
#include "r_mpeg_ts.h"

#define TS_CONSECUTIVE_PACKETS 16
#define TS_PROBE_SIZE (2 * TS_CONSECUTIVE_PACKETS * 204)

int mpeg_ts_reader_c::potential_packet_sizes[] = { 188, 192, 204, 0 };

bool
mpeg_ts_reader_c::probe_file(mm_io_c *io,
                             int64_t size) {
  try {
    vector<int> positions;
    size = size > TS_PROBE_SIZE ? TS_PROBE_SIZE : size;
    memory_cptr buffer(new memory_c(safemalloc(size), size, true));
    unsigned char *mem = buffer->get();
    int i, k;

    io->setFilePointer(0, seek_beginning);
    size = io->read(mem, size);

    for (i = 0; i < size; ++i)
      if (0x47 == mem[i])
        positions.push_back(i);

    for (i = 0; positions.size() > i; ++i) {
      int pos = positions[i];

      for (k = 0; 0 != potential_packet_sizes[k]; ++k) {
        int packet_size    = potential_packet_sizes[k];
        int num_startcodes = 1;

        while ((TS_CONSECUTIVE_PACKETS > num_startcodes) && (pos < size) && (0x47 == mem[pos])) {
          pos += packet_size;
          ++num_startcodes;
        }

        if (TS_CONSECUTIVE_PACKETS <= num_startcodes) {
          id_result_container_unsupported(io->get_file_name(), "MPEG Transport Stream (TS)");
          // Never reached:
          return true;
        }
      }
    }

  } catch (...) {
  }

  return false;
}
