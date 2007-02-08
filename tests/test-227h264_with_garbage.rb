#!/usr/bin/ruby -w

class T_227h264_with_garbage < Test
  def description
    return "mkvmerge / h264 raw with garbage / in(h264)"
  end

  def run
    merge("data/h264/garbage-test.h264")
    return hash_tmp
  end
end

