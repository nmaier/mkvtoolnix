#!/usr/bin/ruby -w

class T_283no_video_on_avi < Test
  def description
    "mkvmerge / --no-video for AVI files / in(AVI)"
  end

  def run
    merge "--no-video data/avi/v.avi"
    hash_tmp
  end
end

