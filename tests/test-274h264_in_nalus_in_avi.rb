#!/usr/bin/ruby -w

class T_274h264_in_nalus_in_avi < Test
  def description
    return "mkvmerge / h264 in NALUs inside AVI"
  end

  def run
    merge "data/avi/h264-in-nalus.avi"
    return hash_tmp
  end
end

