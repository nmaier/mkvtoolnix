#!/usr/bin/ruby -w

class T_243avi_with_audio_garbage < Test
  def description
    return "mkvmerge / AVI with garbage at the beginning of audio tracks / in(AVI)"
  end

  def run
    merge("data/avi/bug300.avi")
    return hash_tmp
  end
end

