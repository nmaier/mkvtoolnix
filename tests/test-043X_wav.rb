#!/usr/bin/ruby -w

class T_043X_wav < Test
  def description
    return "mkvextract / PCM to WAV / out(WAV)"
  end

  def run
    return xtr_tracks_s(8)
  end
end

