#!/usr/bin/ruby -w

class T_010realvideo_4 < Test
  def initialize
    @description = "mkvmerge / audio and video / in(RealVideo 4 and RealAudio)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/rv4.rm")
    return hash_tmp
  end
end

