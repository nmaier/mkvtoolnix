/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_EXTRACT_XTR_ALAC_H
#define MTX_EXTRACT_XTR_ALAC_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_alac_c: public xtr_base_c {
private:
  memory_cptr m_priv;
  uint64_t m_free_chunk_size;
  uint64_t m_free_chunk_offset;
  uint64_t m_data_chunk_offset;
  uint64_t m_frames_written;
  uint64_t m_packets_written;
  uint64_t m_prev_written;

  std::vector<uint8_t> m_pkt_sizes;

public:
  xtr_alac_c(std::string const &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(xtr_frame_t &f);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "ALAC in Core Audio Format";
  };

protected:
};

#endif
