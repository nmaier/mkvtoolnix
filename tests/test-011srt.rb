#!/usr/bin/ruby -w

class T_011srt < Test
  def initialize
    @description = "mkvmerge / subtitles / in(SRT)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/vde.srt")
    return hash_tmp
  end
end

