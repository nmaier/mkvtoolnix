#!/usr/bin/ruby -w

class T_281idr_after_non_idr_not_recognized < Test
  def description
    return "mkvmerge / h264: IDR slice after non-IDR slice not recognized as key frame"
  end

  def run
    merge "data/h264/test_frameloss_track1.h264"
    hash_tmp
  end
end

