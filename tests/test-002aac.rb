#!/usr/bin/ruby -w

class T_002aac < Test
  def initialize
    @description = "mkvmerge / audio only / in(AAC)"
  end

  def run
    merge("data/v.aac", 1)
    return hash_tmp
  end
end
