#!/usr/bin/ruby -w

class T_005flac < Test
  def initialize
    @description = "mkvmerge / audio only / in(FLAC)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/v.flac")
    return hash_tmp
  end
end
