#!/usr/bin/ruby -w

class T_032cues < Test
  def description
    return "mkvmerge / cues / in(AVI)"
  end

  def run
    merge("--cues 0:all --cues 1:iframes data/avi/v.avi")
    return hash_tmp
  end
end

