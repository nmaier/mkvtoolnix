#!/usr/bin/ruby -w

class T_039X_flac < Test
  def description
    return "mkvextract / FLAC (native) / out(FLAC)"
  end

  def run
    return xtr_tracks_s("--no-ogg", 5)
  end
end

