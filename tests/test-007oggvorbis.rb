#!/usr/bin/ruby -w

class T_007oggvorbis < Test
  def initialize
    @description = "mkvmerge / audio only / in(Vorbis in Ogg)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/v.ogg")
    return hash_tmp
  end
end
