#!/usr/bin/ruby -w

class T_010realvideo_4 < Test
  def description
    return "mkvmerge / audio and video / in(RealVideo 4 and RealAudio)"
  end

  def run
    merge("data/rm/rv4.rm")
    return hash_tmp
  end
end

