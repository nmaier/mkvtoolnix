#!/usr/bin/ruby -w

class T_327vp8_frame_type < Test
  def description
    "mkvmerge / VP8 in AVI, all frames marked as key"
  end

  def run
    merge "data/webm/vp8-in-avi.avi"
    hash_tmp
  end
end

