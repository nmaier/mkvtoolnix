#!/usr/bin/ruby -w

class T_200mp2_from_mp4 < Test
  def description
    return "mkvmerge / MPEG1 layer 2 audio from MP4 container / in(MP4)"
  end

  def run
    merge("data/mp4/test_mp2.mp4")
    return hash_tmp
  end
end

