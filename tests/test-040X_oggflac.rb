#!/usr/bin/ruby -w

class T_040X_oggflac < Test
  def description
    return "mkvextract / FLAC to OggFLAC / out(OggFLAC)"
  end

  def run
    return xtr_tracks_s(5)
  end
end

