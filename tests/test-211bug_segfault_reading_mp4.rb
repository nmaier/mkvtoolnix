#!/usr/bin/ruby -w

class T_211bug_segfault_reading_mp4 < Test
  def description
    return "mkvmerge / segfault reading a MP4 created with Nero / in(MP4)"
  end

  def run
    merge("data/bugs/From_Nero_AVC_Muxer.mp4")
    return hash_tmp
  end
end

