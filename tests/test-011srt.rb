#!/usr/bin/ruby -w

class T_011srt < Test
  def description
    return "mkvmerge / subtitles / in(SRT)"
  end

  def run
    merge("--sub-charset 0:ISO-8859-1 data/subtitles/srt/vde.srt")
    return hash_tmp
  end
end

