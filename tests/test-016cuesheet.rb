#!/usr/bin/ruby -w

class T_016cuesheet < Test
  def initialize
    @description = "mkvmerge / CUE sheet to chapters & tags / in(CUE)"
  end

  def run
    merge("data/vde.srt --chapters data/cuewithtags2.cue")
    return hash_tmp
  end
end

