#!/usr/bin/ruby -w

class T_008avi_divx3_mp3 < Test
  def initialize
    @description = "mkvmerge / audio and video / in(divx3 + mp3 from AVI)"
  end

  def run
    merge("data/v.avi")
    return hash_tmp
  end
end

