#!/usr/bin/ruby -w

class T_009realvideo_3 < Test
  def description
    return "mkvmerge / audio and video / in(RealVideo 3 and RealAudio)"
  end

  def run
    merge("data/rm/rv3.rm")
    return hash_tmp
  end
end

