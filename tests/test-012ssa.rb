#!/usr/bin/ruby -w

class T_012ssa < Test
  def initialize
    @description = "mkvmerge / subtitles / in(SSA)"
  end

  def run
    merge("data/fe.ssa")
    return hash_tmp
  end
end

