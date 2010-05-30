#!/usr/bin/ruby -w

class T_272dirac < Test
  def description
    return "mkvmerge / Dirac elementary stream"
  end

  def run
    merge "data/dirac/v.drc"
    return hash_tmp
  end
end

