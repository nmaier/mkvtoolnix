#!/usr/bin/ruby -w

class T_201avc_from_mp4_with_par < Test
  def description
    return "mkvmerge / AVC from MP4 with aspect ratio / in(MP4)"
  end

  def run
    merge("data/mp4/rain_800.mp4")
    return hash_tmp
  end
end

