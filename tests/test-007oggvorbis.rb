#!/usr/bin/ruby -w

class T_007oggvorbis < Test
  def description
    return "mkvmerge / audio only / in(Vorbis in Ogg)"
  end

  def run
    merge("data/ogg/v.ogg")
    return hash_tmp
  end
end
