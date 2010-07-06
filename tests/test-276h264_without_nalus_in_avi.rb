#!/usr/bin/ruby -w

class T_276h264_without_nalus_in_avi < Test
  def description
    return "mkvmerge / h.264 without NALUs in AVI"
  end

  def run
    merge "data/avi/h264-without-nalus.avi"
    return hash_tmp
  end
end

