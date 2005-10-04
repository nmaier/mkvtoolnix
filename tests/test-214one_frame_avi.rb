#!/usr/bin/ruby -w

class T_214one_frame_avi < Test
  def description
    return "mkvmerge / AVI with only one frame / in(AVI)"
  end

  def run
    merge("data/avi/1-frame.avi")
    return hash_tmp
  end
end

