#!/usr/bin/ruby -w

class T_012ssa < Test
  def description
    return "mkvmerge / subtitles / in(SSA)"
  end

  def run
    merge("data/ssa-ass/fe.ssa")
    return hash_tmp
  end
end

