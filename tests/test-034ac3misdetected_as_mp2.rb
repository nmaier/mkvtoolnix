#!/usr/bin/ruby -w

class T_034ac3misdetected_as_mp2 < Test
  def description
    return "mkvmerge / AC3 misdetected as MP2 / in(AC3)"
  end

  def run
    merge("data/ac3/misdetected_as_mp2.ac3")
    return hash_tmp
  end
end

