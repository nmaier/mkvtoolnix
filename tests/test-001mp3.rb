#!/usr/bin/ruby -w

class T_001mp3 < Test
  def initialize
    @description = "mkvmerge / audio only / in(MP3)"
  end

  def run
    merge("data/v.mp3")
    return hash_tmp
  end
end
