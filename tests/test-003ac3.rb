#!/usr/bin/ruby -w

class T_003ac3 < Test
  def initialize
    @description = "mkvmerge / audio only / in(AC3)"
  end

  def run
    merge("data/v.ac3")
    return hash_tmp
  end
end
