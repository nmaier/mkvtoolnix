#!/usr/bin/ruby -w

class T_007oggvorbis < Test
  def initialize
    @description = "mkvmerge / audio only / in(Vorbis in Ogg)"
  end

  def run
    merge("data/v.ogg")
    return hash_tmp
  end
end
