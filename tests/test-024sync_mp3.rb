#!/usr/bin/ruby -w

class T_024sync_mp3 < Test
  def description
    return "mkvmerge / MP3 sync / in(MP3)"
  end

  def run
    merge("--sync 0:500 data/simple/v.mp3")
    hash = hash_tmp
    merge("--sync 0:-500 data/simple/v.mp3")
    return hash + "-" + hash_tmp
  end
end

