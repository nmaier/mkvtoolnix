#!/usr/bin/ruby -w

class T_005flac < Test
  def description
    return "mkvmerge / audio only / in(FLAC)"
  end

  def run
    merge("data/v.flac")
    return hash_tmp
  end
end
