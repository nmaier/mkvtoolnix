#!/usr/bin/ruby -w

class T_232h264_changing_sps_pps < Test
  def description
    return "mkvmerge / h.264 with changing SPS/PPS NALUs / in(h264)"
  end

  def run
    merge("data/h264/changing-sps-pps.h264")
    return hash_tmp
  end
end

