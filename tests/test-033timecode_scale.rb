#!/usr/bin/ruby -w

class T_033timecode_scale < Test
  def description
    return "mkvmerge / timecode scale / in(AVI,MP3)"
  end

  def run
    merge("--timecode-scale 1000000 data/simple/v.mp3")
    hash = hash_tmp
    merge("--timecode-scale -1 data/avi/v.avi")
    return hash + "-" + hash_tmp
  end
end

