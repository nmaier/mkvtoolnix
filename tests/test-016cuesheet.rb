#!/usr/bin/ruby -w

class T_016cuesheet < Test
  def description
    return "mkvmerge / CUE sheet to chapters & tags / in(CUE)"
  end

  def run
    merge("--sub-charset 0:ISO-8859-1 data/subtitles/srt/vde.srt " +
           "--chapters data/text/cuewithtags2.cue")
    return hash_tmp
  end
end

