#!/usr/bin/ruby -w

class T_001mp3 < Test
  def description
    return "mkvmerge / audio only / in(MP3)"
  end

  def run
    merge("data/simple/v.mp3")
    return hash_tmp
  end
end
