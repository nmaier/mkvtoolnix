#!/usr/bin/ruby -w

class T_228h264_no_idr_slices < Test
  def description
    return "mkvmerge / h264 raw without IDR slices / in(h264)"
  end

  def run
    merge("data/h264/no-idr-slices.h264")
    return hash_tmp
  end
end

