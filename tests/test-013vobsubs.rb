#!/usr/bin/ruby -w

class T_013vobsubs < Test
  def description
    return "mkvmerge / subtitles / in(VobSub)"
  end

  def run
    merge("data/vobsub/ally1-short.idx")
    return hash_tmp
  end
end

