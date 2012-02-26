#!/usr/bin/ruby -w

class T_002aac < Test
  def description
    return "mkvmerge / audio only / in(AAC)"
  end

  def run
    merge("data/aac/v.aac", 1)
    return hash_tmp
  end
end
