#!/usr/bin/ruby -w

class T_038X_ac3 < Test
  def description
    return "mkvextract / raw AC3 / out(AC3)"
  end

  def run
    xtr_tracks_s 3
  end
end

