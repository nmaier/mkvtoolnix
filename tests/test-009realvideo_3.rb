#!/usr/bin/ruby -w

class T_009realvideo_3 < Test
  def initialize
    @description = "mkvmerge / audio and video / in(RealVideo 3 and RealAudio)"
  end

  def run
    sys("mkvmerge --engage no_variable_data -o " + tmp + " data/rv3.rm")
    return hash_tmp
  end
end

