#!/usr/bin/ruby -w

class T_013vobsubs < Test
  def initialize
    @description = "mkvmerge / subtitles / in(VobSub)"
  end

  def run
    merge("data/ally1-short.idx")
    return hash_tmp
  end
end

