#!/usr/bin/ruby -w

class T_011srt < Test
  def initialize
    @description = "mkvmerge / subtitles / in(SRT)"
  end

  def run
    merge("data/vde.srt")
    return hash_tmp
  end
end

