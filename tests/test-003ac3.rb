#!/usr/bin/ruby -w

class T_003ac3 < Test
  def description
    return "mkvmerge / audio only / in(AC3)"
  end

  def run
    merge("data/ac3/v.ac3")
    return hash_tmp
  end
end
